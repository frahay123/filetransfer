// Prevents additional console window on Windows in release
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod device;
mod transfer;

use device::{Device, MediaItem};
use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;
use std::sync::Mutex;
use tauri::State;

// Global state for connected devices
struct AppState {
    devices: Mutex<Vec<Device>>,
    media_cache: Mutex<HashMap<String, Vec<MediaItem>>>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TransferProgress {
    current: u32,
    total: u32,
    #[serde(rename = "currentFile")]
    current_file: String,
    #[serde(rename = "bytesTransferred")]
    bytes_transferred: u64,
    #[serde(rename = "bytesTotal")]
    bytes_total: u64,
    speed: u64,
    status: String,
}

// Scan for connected devices (supports hot-swapping - can be called repeatedly)
#[tauri::command]
async fn scan_devices(state: State<'_, AppState>) -> Result<Vec<Device>, String> {
    log::info!("Scanning for devices...");
    
    let devices = device::scan_all_devices().await;
    
    // Update state and handle hot-swapping
    {
        let mut state_devices = state.devices.lock().map_err(|e| e.to_string())?;
        let old_device_ids: std::collections::HashSet<String> = state_devices.iter()
            .map(|d| d.id.clone())
            .collect();
        let new_device_ids: std::collections::HashSet<String> = devices.iter()
            .map(|d| d.id.clone())
            .collect();
        
        // Clear media cache for devices that are no longer connected
        {
            let mut cache = state.media_cache.lock().map_err(|e| e.to_string())?;
            for old_id in old_device_ids.difference(&new_device_ids) {
                cache.remove(old_id);
                log::info!("Removed cache for disconnected device: {}", old_id);
            }
        }
        
        // Update devices list
        *state_devices = devices.clone();
    }
    
    log::info!("Found {} devices (hot-swap ready)", devices.len());
    Ok(devices)
}

// Connect to a specific device
#[tauri::command]
async fn connect_device(device_id: String, state: State<'_, AppState>) -> Result<bool, String> {
    log::info!("Connecting to device: {}", device_id);
    
    let device_exists = {
        let devices = state.devices.lock().map_err(|e| e.to_string())?;
        devices.iter().any(|d| d.id == device_id)
    };
    
    if device_exists {
        log::info!("Connected to device: {}", device_id);
        Ok(true)
    } else {
        Err("Device not found".to_string())
    }
}

// Get media items from connected device
#[tauri::command]
async fn get_media_items(device_id: String, state: State<'_, AppState>) -> Result<Vec<MediaItem>, String> {
    log::info!("Getting media from device: {}", device_id);
    
    // Check cache first
    {
        let cache = state.media_cache.lock().map_err(|e| e.to_string())?;
        if let Some(items) = cache.get(&device_id) {
            log::info!("Returning {} cached media items", items.len());
            return Ok(items.clone());
        }
    }
    
    // Get device info - clone it so we can drop the lock
    let device = {
        let devices = state.devices.lock().map_err(|e| e.to_string())?;
        devices.iter().find(|d| d.id == device_id).cloned()
    };
    
    let device = device.ok_or("Device not found")?;
    
    // Enumerate media (no lock held here)
    let items = device::enumerate_media(&device).await?;
    
    // Cache results
    {
        let mut cache = state.media_cache.lock().map_err(|e| e.to_string())?;
        cache.insert(device_id, items.clone());
        log::info!("Cached {} media items", items.len());
    }
    
    Ok(items)
}

// Refresh media items for a device (clears cache and re-enumerates)
#[tauri::command]
async fn refresh_media_items(device_id: String, state: State<'_, AppState>) -> Result<Vec<MediaItem>, String> {
    log::info!("Refreshing media for device: {}", device_id);
    
    // Clear cache for this device
    {
        let mut cache = state.media_cache.lock().map_err(|e| e.to_string())?;
        cache.remove(&device_id);
    }
    
    // Re-enumerate (this will cache the new results)
    get_media_items(device_id, state).await
}

// Transfer files from device to computer
#[tauri::command]
async fn transfer_files(
    window: tauri::Window,
    device_id: String,
    file_ids: Vec<String>,
    destination: String,
    state: State<'_, AppState>,
) -> Result<(), String> {
    log::info!(
        "Transferring {} files from {} to {}",
        file_ids.len(),
        device_id,
        destination
    );

    // Expand destination path
    let dest_path = PathBuf::from(shellexpand::tilde(&destination).to_string());
    fs::create_dir_all(&dest_path).map_err(|e| e.to_string())?;

    // Get device - clone it so we can drop the lock
    let device = {
        let devices = state.devices.lock().map_err(|e| e.to_string())?;
        devices.iter().find(|d| d.id == device_id).cloned()
    };
    let device = device.ok_or("Device not found")?;
    
    // Get media items - clone them so we can drop the lock
    let all_items = {
        let cache = state.media_cache.lock().map_err(|e| e.to_string())?;
        cache.get(&device_id).cloned()
    };
    let all_items = all_items.ok_or("No media cached")?;
    
    // Filter to selected items
    let items: Vec<_> = all_items.iter()
        .filter(|item| file_ids.contains(&item.id))
        .cloned()
        .collect();
    
    let total = items.len() as u32;
    let mut transferred: u64 = 0;
    let total_bytes: u64 = items.iter().map(|i| i.size).sum();
    
    for (i, item) in items.iter().enumerate() {
        // Emit progress
        let progress = TransferProgress {
            current: i as u32 + 1,
            total,
            current_file: item.name.clone(),
            bytes_transferred: transferred,
            bytes_total: total_bytes,
            speed: 25_000_000, // Estimate
            status: "transferring".to_string(),
        };
        let _ = window.emit("transfer-progress", &progress);
        
        // Transfer file
        let dest_file = dest_path.join(&item.name);
        match transfer::transfer_file(&device, item, &dest_file).await {
            Ok(_) => {
                transferred += item.size;
                log::info!("Transferred: {}", item.name);
            }
            Err(e) => {
                log::error!("Failed to transfer {}: {}", item.name, e);
            }
        }
    }
    
    // Final progress
    let progress = TransferProgress {
        current: total,
        total,
        current_file: String::new(),
        bytes_transferred: total_bytes,
        bytes_total: total_bytes,
        speed: 0,
        status: "complete".to_string(),
    };
    let _ = window.emit("transfer-progress", &progress);

    Ok(())
}

// Get default destination path
#[tauri::command]
fn get_default_destination() -> String {
    dirs::picture_dir()
        .map(|p| p.join("PhonePhotos").to_string_lossy().to_string())
        .unwrap_or_else(|| "~/Pictures/PhonePhotos".to_string())
}

// Open folder in system file manager
#[tauri::command]
fn open_folder(path: String) -> Result<(), String> {
    let expanded = shellexpand::tilde(&path).to_string();
    open::that(&expanded).map_err(|e| e.to_string())
}

// Select folder dialog
#[tauri::command]
async fn select_folder() -> Result<Option<String>, String> {
    use tauri::api::dialog::blocking::FileDialogBuilder;
    
    let folder = FileDialogBuilder::new()
        .set_title("Select Destination Folder")
        .pick_folder();
    
    Ok(folder.map(|p| p.to_string_lossy().to_string()))
}

fn main() {
    env_logger::init();
    
    tauri::Builder::default()
        .manage(AppState {
            devices: Mutex::new(Vec::new()),
            media_cache: Mutex::new(HashMap::new()),
        })
        .invoke_handler(tauri::generate_handler![
            scan_devices,
            connect_device,
            get_media_items,
            refresh_media_items,
            transfer_files,
            get_default_destination,
            open_folder,
            select_folder,
        ])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
