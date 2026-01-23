use crate::device::{Device, MediaItem};
use std::fs::{self, File};
use std::io::{Read, Write};
use std::path::Path;
use std::process::Command;

/// Transfer a single file from device to destination
pub async fn transfer_file(
    device: &Device,
    item: &MediaItem,
    destination: &Path,
) -> Result<(), String> {
    match device.device_type.as_str() {
        "android" => transfer_mtp_file(device, item, destination).await,
        "ios" => transfer_ios_file(device, item, destination).await,
        _ => Err("Unknown device type".to_string()),
    }
}

/// Transfer file from Android device via MTP
async fn transfer_mtp_file(
    device: &Device,
    item: &MediaItem,
    destination: &Path,
) -> Result<(), String> {
    // Create parent directories
    if let Some(parent) = destination.parent() {
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    
    #[cfg(target_os = "linux")]
    {
        // If we have a mount point (GVFS/gio), copy directly
        if let Some(ref mount) = device.mount_point {
            let source = format!("{}/{}", mount, item.full_path.trim_start_matches('/'));
            
            // Use gio copy for MTP
            let output = Command::new("gio")
                .args(["copy", &source, &destination.to_string_lossy()])
                .output()
                .map_err(|e| e.to_string())?;
            
            if output.status.success() {
                return Ok(());
            }
            
            // Fallback to direct copy if mounted
            if let Ok(mut src_file) = File::open(&source) {
                let mut dst_file = File::create(destination).map_err(|e| e.to_string())?;
                let mut buffer = vec![0u8; 1024 * 1024]; // 1MB buffer
                
                loop {
                    let bytes_read = src_file.read(&mut buffer).map_err(|e| e.to_string())?;
                    if bytes_read == 0 {
                        break;
                    }
                    dst_file.write_all(&buffer[..bytes_read]).map_err(|e| e.to_string())?;
                }
                
                return Ok(());
            }
        }
        
        // Use mtp-getfile as fallback
        let output = Command::new("mtp-getfile")
            .args([&item.full_path, &destination.to_string_lossy()])
            .output();
        
        match output {
            Ok(out) if out.status.success() => Ok(()),
            Ok(out) => Err(String::from_utf8_lossy(&out.stderr).to_string()),
            Err(e) => Err(e.to_string()),
        }
    }
    
    #[cfg(target_os = "windows")]
    {
        // Use Windows Portable Devices API
        transfer_wpd_file(device, item, destination)
    }
    
    #[cfg(target_os = "macos")]
    {
        // macOS typically doesn't support MTP natively
        Err("MTP not supported on macOS. Please use Android File Transfer.".to_string())
    }
    
    #[cfg(not(any(target_os = "linux", target_os = "windows", target_os = "macos")))]
    {
        Err("Platform not supported".to_string())
    }
}

/// Windows: Transfer using WPD
#[cfg(target_os = "windows")]
fn transfer_wpd_file(
    device: &Device,
    item: &MediaItem,
    destination: &Path,
) -> Result<(), String> {
    use windows::Win32::Devices::PortableDevices::*;
    use windows::Win32::System::Com::*;
    use windows::core::*;
    
    unsafe {
        CoInitializeEx(None, COINIT_MULTITHREADED).ok();
        
        // Get the WPD device ID
        let device_id = device.mount_point.as_ref()
            .ok_or("No device path")?;
        
        // This is a simplified implementation
        // Full WPD file transfer would require:
        // 1. Opening the device
        // 2. Finding the file object by path
        // 3. Getting IPortableDeviceResources
        // 4. Opening a stream to read the file
        // 5. Writing to local destination
        
        // For now, return an error indicating we need the full implementation
        Err("WPD transfer not fully implemented yet".to_string())
    }
}

/// Transfer file from iOS device
async fn transfer_ios_file(
    device: &Device,
    item: &MediaItem,
    destination: &Path,
) -> Result<(), String> {
    // Create parent directories
    if let Some(parent) = destination.parent() {
        fs::create_dir_all(parent).map_err(|e| e.to_string())?;
    }
    
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    {
        let udid = device.id.strip_prefix("ios-").unwrap_or(&device.id);
        
        // If the file is already at full_path (from mounted ifuse), just copy
        if Path::new(&item.full_path).exists() {
            fs::copy(&item.full_path, destination).map_err(|e| e.to_string())?;
            return Ok(());
        }
        
        // Otherwise, use idevicepair + ifuse approach
        // Mount, copy, unmount
        let mount_point = format!("/tmp/ios-transfer-{}", udid);
        let _ = fs::create_dir_all(&mount_point);
        
        // Mount
        let _ = Command::new("ifuse")
            .args(["-u", udid, &mount_point])
            .output();
        
        // Copy
        let source_path = format!("{}/{}", mount_point, item.full_path.trim_start_matches('/'));
        let copy_result = fs::copy(&source_path, destination);
        
        // Unmount
        let _ = Command::new("fusermount")
            .args(["-u", &mount_point])
            .output();
        
        copy_result.map(|_| ()).map_err(|e| e.to_string())
    }
    
    #[cfg(target_os = "windows")]
    {
        // On Windows, iOS devices appear as WPD when iTunes is installed
        transfer_wpd_file(device, item, destination)
    }
    
    #[cfg(not(any(target_os = "linux", target_os = "macos", target_os = "windows")))]
    {
        Err("Platform not supported".to_string())
    }
}

/// Calculate SHA256 hash of a file for duplicate detection
pub fn calculate_file_hash(path: &Path) -> Result<String, String> {
    use sha2::{Sha256, Digest};
    
    let mut file = File::open(path).map_err(|e| e.to_string())?;
    let mut hasher = Sha256::new();
    let mut buffer = vec![0u8; 1024 * 1024];
    
    loop {
        let bytes_read = file.read(&mut buffer).map_err(|e| e.to_string())?;
        if bytes_read == 0 {
            break;
        }
        hasher.update(&buffer[..bytes_read]);
    }
    
    Ok(hex::encode(hasher.finalize()))
}

/// Organize file by date (create folder structure like 2024/01/15/)
pub fn get_organized_path(base_path: &Path, item: &MediaItem, organize_by_date: bool) -> std::path::PathBuf {
    if !organize_by_date {
        return base_path.join(&item.name);
    }
    
    // Parse date from item
    let date = chrono::DateTime::parse_from_rfc3339(&item.date)
        .map(|d| d.naive_local().date())
        .unwrap_or_else(|_| chrono::Local::now().date_naive());
    
    let year = date.format("%Y").to_string();
    let month = date.format("%m").to_string();
    let day = date.format("%d").to_string();
    
    base_path.join(year).join(month).join(day).join(&item.name)
}
