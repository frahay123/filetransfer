use serde::{Deserialize, Serialize};
use std::process::Command;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Device {
    pub id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub device_type: String,
    pub manufacturer: String,
    #[serde(rename = "storageUsed")]
    pub storage_used: u64,
    #[serde(rename = "storageTotal")]
    pub storage_total: u64,
    #[serde(rename = "photoCount")]
    pub photo_count: u32,
    pub connected: bool,
    // Internal fields
    #[serde(skip)]
    pub mount_point: Option<String>,
    #[serde(skip)]
    pub usb_bus: Option<u8>,
    #[serde(skip)]
    pub usb_address: Option<u8>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct MediaItem {
    pub id: String,
    pub name: String,
    #[serde(rename = "type")]
    pub media_type: String,
    pub size: u64,
    pub date: String,
    pub thumbnail: Option<String>,
    // Internal
    #[serde(skip)]
    pub full_path: String,
}

/// Scan all connected devices (Android MTP + iOS)
pub async fn scan_all_devices() -> Vec<Device> {
    let mut devices = Vec::new();
    
    // Platform-specific scanning
    #[cfg(target_os = "linux")]
    {
        if let Ok(mtp) = scan_mtp_linux().await {
            devices.extend(mtp);
        }
        if let Ok(ios) = scan_ios_unix().await {
            devices.extend(ios);
        }
    }
    
    #[cfg(target_os = "macos")]
    {
        if let Ok(ios) = scan_ios_unix().await {
            devices.extend(ios);
        }
    }
    
    #[cfg(target_os = "windows")]
    {
        if let Ok(wpd) = scan_wpd_windows() {
            devices.extend(wpd);
        }
    }
    
    // Fallback: scan USB for known phone vendors
    if devices.is_empty() {
        if let Ok(usb) = scan_usb_devices() {
            devices.extend(usb);
        }
    }
    
    devices
}

/// Linux: Scan for MTP devices using gio or mtp-detect
#[cfg(target_os = "linux")]
async fn scan_mtp_linux() -> Result<Vec<Device>, String> {
    let mut devices = Vec::new();
    
    // Try gio mount list (GVFS)
    if let Ok(output) = Command::new("gio").args(["mount", "-l"]).output() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        
        for line in stdout.lines() {
            if line.contains("mtp://") || line.contains("gphoto2://") {
                let name = line.split("->").next()
                    .map(|s| s.trim().to_string())
                    .unwrap_or_else(|| "Android Device".to_string());
                
                let mount_point = if let Some(idx) = line.find("mtp://") {
                    Some(line[idx..].split_whitespace().next().unwrap_or("").to_string())
                } else {
                    None
                };
                
                devices.push(Device {
                    id: format!("mtp-{}", devices.len()),
                    name,
                    device_type: "android".to_string(),
                    manufacturer: "Unknown".to_string(),
                    storage_used: 0,
                    storage_total: 0,
                    photo_count: 0,
                    connected: true,
                    mount_point,
                    usb_bus: None,
                    usb_address: None,
                });
            }
        }
    }
    
    // Fallback: mtp-detect
    if devices.is_empty() {
        if let Ok(output) = Command::new("mtp-detect").output() {
            let stdout = String::from_utf8_lossy(&output.stdout);
            
            if stdout.contains("LIBMTP") && !stdout.contains("No devices") {
                let mut name = "Android Device".to_string();
                let mut manufacturer = "Unknown".to_string();
                
                for line in stdout.lines() {
                    if line.contains("Manufacturer:") {
                        if let Some(val) = line.split(':').nth(1) {
                            manufacturer = val.trim().to_string();
                        }
                    }
                    if line.contains("Model:") {
                        if let Some(val) = line.split(':').nth(1) {
                            name = val.trim().to_string();
                        }
                    }
                }
                
                devices.push(Device {
                    id: "mtp-0".to_string(),
                    name,
                    device_type: "android".to_string(),
                    manufacturer,
                    storage_used: 0,
                    storage_total: 0,
                    photo_count: 0,
                    connected: true,
                    mount_point: None,
                    usb_bus: None,
                    usb_address: None,
                });
            }
        }
    }
    
    Ok(devices)
}

/// Unix: Scan for iOS devices using libimobiledevice
#[cfg(any(target_os = "linux", target_os = "macos"))]
async fn scan_ios_unix() -> Result<Vec<Device>, String> {
    let mut devices = Vec::new();
    
    // Use idevice_id to list connected iOS devices
    if let Ok(output) = Command::new("idevice_id").arg("-l").output() {
        let stdout = String::from_utf8_lossy(&output.stdout);
        
        for udid in stdout.lines() {
            let udid = udid.trim();
            if udid.is_empty() {
                continue;
            }
            
            // Get device name
            let name = Command::new("ideviceinfo")
                .args(["-u", udid, "-k", "DeviceName"])
                .output()
                .ok()
                .map(|o| String::from_utf8_lossy(&o.stdout).trim().to_string())
                .filter(|s| !s.is_empty())
                .unwrap_or_else(|| "iPhone/iPad".to_string());
            
            devices.push(Device {
                id: format!("ios-{}", udid),
                name,
                device_type: "ios".to_string(),
                manufacturer: "Apple".to_string(),
                storage_used: 0,
                storage_total: 0,
                photo_count: 0,
                connected: true,
                mount_point: None,
                usb_bus: None,
                usb_address: None,
            });
        }
    }
    
    Ok(devices)
}

/// Windows: Scan using Windows Portable Devices API
#[cfg(target_os = "windows")]
fn scan_wpd_windows() -> Result<Vec<Device>, String> {
    // Simplified WPD implementation
    // Full implementation would use windows crate for COM interop
    
    let mut devices = Vec::new();
    
    // Try PowerShell to list portable devices
    if let Ok(output) = Command::new("powershell")
        .args(["-Command", "Get-WmiObject -Class Win32_PnPEntity | Where-Object { $_.Name -match 'MTP|Apple|Android|iPhone|iPad|Phone' } | Select-Object -ExpandProperty Name"])
        .output()
    {
        let stdout = String::from_utf8_lossy(&output.stdout);
        
        for (i, line) in stdout.lines().enumerate() {
            let name = line.trim();
            if name.is_empty() {
                continue;
            }
            
            let device_type = if name.to_lowercase().contains("apple") 
                || name.to_lowercase().contains("iphone")
                || name.to_lowercase().contains("ipad") {
                "ios"
            } else {
                "android"
            };
            
            let manufacturer = if device_type == "ios" {
                "Apple"
            } else {
                "Unknown"
            };
            
            devices.push(Device {
                id: format!("wpd-{}", i),
                name: name.to_string(),
                device_type: device_type.to_string(),
                manufacturer: manufacturer.to_string(),
                storage_used: 0,
                storage_total: 0,
                photo_count: 0,
                connected: true,
                mount_point: None,
                usb_bus: None,
                usb_address: None,
            });
        }
    }
    
    Ok(devices)
}

/// Fallback: Scan USB devices for known phone vendors
fn scan_usb_devices() -> Result<Vec<Device>, String> {
    let mut devices = Vec::new();
    
    // Known phone vendor IDs
    let phone_vendors: [(u16, &str); 12] = [
        (0x04e8, "Samsung"),
        (0x18d1, "Google"),
        (0x22b8, "Motorola"),
        (0x0bb4, "HTC"),
        (0x2717, "Xiaomi"),
        (0x1004, "LG"),
        (0x05c6, "Qualcomm"),
        (0x2a70, "OnePlus"),
        (0x0fce, "Sony"),
        (0x2916, "OPPO"),
        (0x2ae5, "Huawei"),
        (0x05ac, "Apple"),
    ];
    
    if let Ok(usb_devices) = rusb::devices() {
        for device in usb_devices.iter() {
            if let Ok(desc) = device.device_descriptor() {
                for (vendor_id, vendor_name) in &phone_vendors {
                    if desc.vendor_id() == *vendor_id {
                        let device_type = if *vendor_id == 0x05ac { "ios" } else { "android" };
                        
                        devices.push(Device {
                            id: format!("usb-{:04x}-{:04x}", desc.vendor_id(), desc.product_id()),
                            name: format!("{} Device", vendor_name),
                            device_type: device_type.to_string(),
                            manufacturer: vendor_name.to_string(),
                            storage_used: 0,
                            storage_total: 0,
                            photo_count: 0,
                            connected: true,
                            mount_point: None,
                            usb_bus: Some(device.bus_number()),
                            usb_address: Some(device.address()),
                        });
                        break;
                    }
                }
            }
        }
    }
    
    Ok(devices)
}

/// Enumerate media files on a device
pub async fn enumerate_media(device: &Device) -> Result<Vec<MediaItem>, String> {
    match device.device_type.as_str() {
        "android" => enumerate_android_media(device).await,
        "ios" => enumerate_ios_media(device).await,
        _ => Ok(vec![]),
    }
}

/// Enumerate Android media via MTP
async fn enumerate_android_media(device: &Device) -> Result<Vec<MediaItem>, String> {
    let _ = device; // Used conditionally per platform
    let mut items = Vec::new();
    
    #[cfg(target_os = "linux")]
    {
        if let Some(ref mount) = device.mount_point {
            // Use gio to list DCIM folder
            if let Ok(output) = Command::new("gio")
                .args(["list", "-l", &format!("{}/DCIM", mount)])
                .output()
            {
                let stdout = String::from_utf8_lossy(&output.stdout);
                for (i, line) in stdout.lines().enumerate() {
                    let parts: Vec<&str> = line.split_whitespace().collect();
                    if !parts.is_empty() {
                        let name = parts[0].to_string();
                        let size: u64 = parts.get(2).and_then(|s| s.parse().ok()).unwrap_or(0);
                        
                        let media_type = if name.to_lowercase().ends_with(".mp4") 
                            || name.to_lowercase().ends_with(".mov") {
                            "video"
                        } else {
                            "photo"
                        };
                        
                        items.push(MediaItem {
                            id: format!("media-{}", i),
                            name: name.clone(),
                            media_type: media_type.to_string(),
                            size,
                            date: chrono::Utc::now().to_rfc3339(),
                            thumbnail: None,
                            full_path: format!("{}/DCIM/{}", mount, name),
                        });
                    }
                }
            }
        }
    }
    
    // Demo items if nothing found
    if items.is_empty() {
        for i in 0..20 {
            let is_video = i % 5 == 0;
            items.push(MediaItem {
                id: format!("demo-{}", i),
                name: format!("{}{:04}.{}", 
                    if is_video { "VID_" } else { "IMG_" },
                    1000 + i,
                    if is_video { "mp4" } else { "jpg" }
                ),
                media_type: if is_video { "video" } else { "photo" }.to_string(),
                size: 3_500_000 + (i as u64 * 500_000),
                date: chrono::Utc::now().to_rfc3339(),
                thumbnail: None,
                full_path: format!("/DCIM/Camera/IMG_{:04}.jpg", 1000 + i),
            });
        }
    }
    
    Ok(items)
}

/// Enumerate iOS media using ifuse
async fn enumerate_ios_media(device: &Device) -> Result<Vec<MediaItem>, String> {
    let mut items = Vec::new();
    
    #[cfg(any(target_os = "linux", target_os = "macos"))]
    {
        let udid = device.id.strip_prefix("ios-").unwrap_or(&device.id);
        let mount_point = format!("/tmp/ios-mount-{}", udid);
        
        // Try to mount and enumerate
        let _ = std::fs::create_dir_all(&mount_point);
        
        if Command::new("ifuse")
            .args(["-u", udid, &mount_point])
            .output()
            .is_ok()
        {
            // Look for DCIM folder
            if let Ok(entries) = std::fs::read_dir(format!("{}/DCIM", mount_point)) {
                for (i, entry) in entries.flatten().enumerate() {
                    if let Ok(metadata) = entry.metadata() {
                        let name = entry.file_name().to_string_lossy().to_string();
                        let media_type = if name.to_lowercase().ends_with(".mov") 
                            || name.to_lowercase().ends_with(".mp4") {
                            "video"
                        } else {
                            "photo"
                        };
                        
                        items.push(MediaItem {
                            id: format!("ios-{}", i),
                            name,
                            media_type: media_type.to_string(),
                            size: metadata.len(),
                            date: chrono::Utc::now().to_rfc3339(),
                            thumbnail: None,
                            full_path: entry.path().to_string_lossy().to_string(),
                        });
                    }
                }
            }
            
            // Unmount
            let _ = Command::new("fusermount").args(["-u", &mount_point]).output();
        }
    }
    
    // Demo items if nothing found
    if items.is_empty() {
        for i in 0..15 {
            let is_video = i % 4 == 0;
            items.push(MediaItem {
                id: format!("ios-demo-{}", i),
                name: format!("{}{:04}.{}", 
                    if is_video { "IMG_" } else { "IMG_" },
                    5000 + i,
                    if is_video { "MOV" } else { "HEIC" }
                ),
                media_type: if is_video { "video" } else { "photo" }.to_string(),
                size: 4_000_000 + (i as u64 * 800_000),
                date: chrono::Utc::now().to_rfc3339(),
                thumbnail: None,
                full_path: format!("/DCIM/100APPLE/IMG_{:04}.HEIC", 5000 + i),
            });
        }
    }
    
    Ok(items)
}
