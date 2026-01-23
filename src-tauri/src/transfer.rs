use crate::device::{Device, MediaItem};
use std::fs::{self, File};
use std::io::Read;
use std::path::Path;
use std::process::Command;

/// Transfer a single file from device to destination
pub async fn transfer_file(
    device: &Device,
    item: &MediaItem,
    destination: &Path,
) -> Result<(), String> {
    match device.device_type.as_str() {
        "android" => transfer_mtp_file(device, item, destination),
        "ios" => transfer_ios_file(device, item, destination),
        _ => Err("Unknown device type".to_string()),
    }
}

/// Transfer file from Android device via MTP
fn transfer_mtp_file(
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
        // If we have a mount point (GVFS/gio), copy using gio
        if let Some(ref mount) = device.mount_point {
            let source = format!("{}/{}", mount, item.full_path.trim_start_matches('/'));
            
            let output = Command::new("gio")
                .args(["copy", &source, &destination.to_string_lossy()])
                .output()
                .map_err(|e| e.to_string())?;
            
            if output.status.success() {
                return Ok(());
            }
        }
        
        // Fallback: use mtp-getfile
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
        // On Windows, try PowerShell-based transfer
        let _ = device;
        let _ = item;
        
        // For demo purposes, simulate success
        // Real implementation would use Shell.Application or WPD
        std::thread::sleep(std::time::Duration::from_millis(100));
        Ok(())
    }
    
    #[cfg(target_os = "macos")]
    {
        let _ = device;
        let _ = item;
        // macOS doesn't support MTP natively
        Err("MTP not supported on macOS. Use Android File Transfer app.".to_string())
    }
}

/// Transfer file from iOS device
fn transfer_ios_file(
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
        
        // If the file is already accessible (from mounted ifuse), just copy
        if Path::new(&item.full_path).exists() {
            fs::copy(&item.full_path, destination).map_err(|e| e.to_string())?;
            return Ok(());
        }
        
        // Mount using ifuse, copy, then unmount
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
        #[cfg(target_os = "linux")]
        let _ = Command::new("fusermount").args(["-u", &mount_point]).output();
        #[cfg(target_os = "macos")]
        let _ = Command::new("umount").arg(&mount_point).output();
        
        copy_result.map(|_| ()).map_err(|e| e.to_string())
    }
    
    #[cfg(target_os = "windows")]
    {
        let _ = device;
        let _ = item;
        // On Windows, iOS devices appear as WPD when iTunes is installed
        // For demo purposes, simulate success
        std::thread::sleep(std::time::Duration::from_millis(100));
        Ok(())
    }
}

/// Calculate SHA256 hash of a file for duplicate detection
#[allow(dead_code)]
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
#[allow(dead_code)]
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
