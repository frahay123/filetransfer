#include "device_handler.h"
#include "photo_db.h"
#include "photo_sync.h"
#include "utils.h"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <cstring>
#include <memory>

#ifdef ENABLE_ANDROID
#include "mtp_handler.h"
#endif

#ifdef ENABLE_IOS
#include "ios_handler.h"
#endif

#include "config.h"

using namespace std;

// Interactive prompt for device type selection
string promptDeviceType() {
    cout << "\n=== Device Type Selection ===" << endl;
    cout << "Which device type are you connecting?" << endl;
    
    int option = 1;
#ifdef ENABLE_ANDROID
    cout << "  " << option++ << ") Android" << endl;
#endif
#ifdef ENABLE_IOS
    cout << "  " << option++ << ") iPhone/iPad" << endl;
#endif
    cout << "  " << option << ") Auto-detect" << endl;
    
    cout << "\nPlease select [1-" << option << "]: ";
    
    string input;
    getline(cin, input);
    
    if (input.empty()) {
        return "auto";
    }
    
    int choice = 0;
    try {
        choice = stoi(input);
    } catch (...) {
        return "auto";
    }
    
    int idx = 1;
#ifdef ENABLE_ANDROID
    if (choice == idx++) return "android";
#endif
#ifdef ENABLE_IOS
    if (choice == idx++) return "ios";
#endif
    
    return "auto";
}

// Interactive prompt for destination folder
string promptDestination(const string& current_default) {
    cout << "\n=== Destination Folder ===" << endl;
    cout << "Where should photos/videos be saved?" << endl;
    cout << "Default: " << current_default << endl;
    cout << "\nPress Enter for default, or type custom path: ";
    
    string input;
    getline(cin, input);
    
    if (input.empty()) {
        return current_default;
    }
    
    // Expand path
    string expanded = Utils::expandPath(input);
    
    // Validate or create directory
    if (!Utils::createDirectory(expanded)) {
        cout << "Warning: Could not create directory. Using default." << endl;
        return current_default;
    }
    
    return input;
}

// Interactive prompt for transfer mode
bool promptTransferAll() {
    cout << "\n=== Transfer Mode ===" << endl;
    cout << "What would you like to transfer?" << endl;
    cout << "  1) New photos/videos only (recommended)" << endl;
    cout << "  2) All photos/videos" << endl;
    cout << "\nPlease select [1-2]: ";
    
    string input;
    getline(cin, input);
    
    if (input == "2") {
        return true;
    }
    
    return false;
}

void printStorageInfo(const vector<DeviceStorageInfo>& storages) {
    cout << "\n=== Storage Information ===" << endl;
    
    if (storages.empty()) {
        cout << "No storage found." << endl;
        return;
    }

    for (const auto& storage : storages) {
        cout << "\nStorage ID: " << storage.storage_id << endl;
        cout << "Description: " << storage.description << endl;
        
        // Convert bytes to GB
        double max_gb = storage.max_capacity / (1024.0 * 1024.0 * 1024.0);
        double free_gb = storage.free_space / (1024.0 * 1024.0 * 1024.0);
        double used_gb = max_gb - free_gb;
        double used_percent = (used_gb / max_gb) * 100.0;
        
        cout << fixed << setprecision(2);
        cout << "Capacity: " << max_gb << " GB" << endl;
        cout << "Used: " << used_gb << " GB (" << used_percent << "%)" << endl;
        cout << "Free: " << free_gb << " GB" << endl;
    }
}

void printMediaInfo(const vector<MediaInfo>& photos) {
    cout << "\n=== Media Found: " << photos.size() << " ===" << endl;
    
    if (photos.empty()) {
        cout << "No photos/videos found." << endl;
        return;
    }

    // Print first 10 photos as sample
    int count = 0;
    const int max_display = 10;
    
    for (const auto& photo : photos) {
        if (count++ >= max_display) {
            cout << "\n... and " << (photos.size() - max_display) 
                 << " more photos/videos" << endl;
            break;
        }
        
        cout << "\nMedia #" << count << ":" << endl;
        cout << "  ID: " << photo.object_id << endl;
        cout << "  Filename: " << photo.filename << endl;
        cout << "  Path: " << photo.path << endl;
        cout << "  Size: " << (photo.file_size / 1024.0) << " KB" << endl;
        cout << "  MIME Type: " << photo.mime_type << endl;
        
        // Convert timestamp to readable date
        time_t time = photo.modification_date;
        cout << "  Modified: " << put_time(localtime(&time), "%Y-%m-%d %H:%M:%S") 
             << endl;
    }
}

void printUsage(const char* program_name) {
    cout << "Usage: " << program_name << " [OPTIONS]" << endl;
    cout << "\nOptions:" << endl;
    cout << "  -d, --destination PATH    Destination folder for photos" << endl;
    cout << "  -t, --device-type TYPE    Device type: android, ios, or auto" << endl;
    cout << "  -a, --all                 Transfer all photos (not just new ones)" << endl;
    cout << "  -l, --list-only           Only list photos, don't transfer" << endl;
    cout << "  --no-interactive          Skip interactive prompts, use saved config" << endl;
    cout << "  --reset-config            Reset configuration to defaults" << endl;
    cout << "  -h, --help                Show this help message" << endl;
    cout << "\nExamples:" << endl;
    cout << "  " << program_name << "                              # Interactive mode (prompts for options)" << endl;
    cout << "  " << program_name << " --no-interactive             # Use saved configuration" << endl;
    cout << "  " << program_name << " -d ~/Desktop/Photos          # Transfer to custom location" << endl;
    cout << "  " << program_name << " -t android                   # Force Android/MTP mode" << endl;
    cout << "  " << program_name << " -t ios                       # Force iOS mode" << endl;
    cout << "  " << program_name << " -a                           # Transfer all photos" << endl;
    cout << "  " << program_name << " -l                           # Just list photos, don't transfer" << endl;
}

// Create device handler based on type
unique_ptr<DeviceHandler> createDeviceHandler(const string& device_type) {
#ifdef ENABLE_ANDROID
    if (device_type == "android") {
        return make_unique<MTPHandler>();
    }
#endif

#ifdef ENABLE_IOS
    if (device_type == "ios") {
        return make_unique<iOSHandler>();
    }
#endif

    // Auto-detect: try Android first, then iOS
    if (device_type == "auto" || device_type.empty()) {
#ifdef ENABLE_ANDROID
        auto mtp = make_unique<MTPHandler>();
        if (mtp->detectDevices()) {
            cout << "Auto-detected Android device" << endl;
            return mtp;
        }
#endif

#ifdef ENABLE_IOS
        auto ios = make_unique<iOSHandler>();
        if (ios->detectDevices()) {
            cout << "Auto-detected iOS device" << endl;
            return ios;
        }
#endif
    }
    
    return nullptr;
}

// Check which device backends are available
void printAvailableBackends() {
    cout << "Supported device types:" << endl;
#ifdef ENABLE_ANDROID
    cout << "  - android (Android phones via MTP)" << endl;
#endif
#ifdef ENABLE_IOS
    cout << "  - ios (iPhone/iPad via libimobiledevice)" << endl;
#endif
#if !defined(ENABLE_ANDROID) && !defined(ENABLE_IOS)
    cout << "  (No device backends available - install libmtp-dev or libimobiledevice-dev)" << endl;
#endif
}

int main(int argc, char* argv[]) {
    // Load configuration
    Config config;
    config.load();
    
    string destination = config.getDestinationFolder();
    string device_type = config.getDeviceType();
    bool transfer_all = (config.getTransferMode() == "all");
    bool list_only = false;
    bool interactive = true;
    bool reset_config = false;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printUsage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--destination") == 0) {
            if (i + 1 < argc) {
                destination = argv[++i];
                interactive = false; // Skip destination prompt
            } else {
                cerr << "Error: -d requires a path argument" << endl;
                return 1;
            }
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--device-type") == 0) {
            if (i + 1 < argc) {
                device_type = argv[++i];
                if (device_type != "android" && device_type != "ios" && device_type != "auto") {
                    cerr << "Error: device type must be 'android', 'ios', or 'auto'" << endl;
                    return 1;
                }
                interactive = false; // Skip device type prompt
            } else {
                cerr << "Error: -t requires a type argument" << endl;
                return 1;
            }
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            transfer_all = true;
            interactive = false;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list-only") == 0) {
            list_only = true;
            interactive = false;
        } else if (strcmp(argv[i], "--no-interactive") == 0) {
            interactive = false;
        } else if (strcmp(argv[i], "--reset-config") == 0) {
            reset_config = true;
        } else {
            cerr << "Unknown option: " << argv[i] << endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Reset config if requested
    if (reset_config) {
        config.reset();
        cout << "Configuration reset to defaults." << endl;
        destination = config.getDestinationFolder();
        device_type = config.getDeviceType();
        transfer_all = false;
    }
    
    cout << "=== Photo Transfer ===" << endl;
    
    // Interactive mode prompts (if no command-line overrides and first run or interactive enabled)
    if (interactive && (config.isFirstRun() || argc == 1)) {
        cout << "Welcome to Photo Transfer!" << endl;
        
        // Device type selection
        device_type = promptDeviceType();
        
        // Destination folder selection
        destination = promptDestination(config.getDestinationFolder());
        
        // Transfer mode selection (only if not list-only)
        if (!list_only) {
            transfer_all = promptTransferAll();
        }
        
        // Save preferences if user wants to remember
        if (config.getRememberSettings()) {
            config.setDeviceType(device_type);
            config.setDestinationFolder(destination);
            config.setTransferMode(transfer_all ? "all" : "new_only");
            config.save();
        }
        
        cout << endl;
    }
    
    cout << "Destination: " << destination << endl;
    cout << "Device Type: " << (device_type == "auto" ? "Auto-detect" : device_type) << endl;
    cout << "Mode: " << (list_only ? "List only" : (transfer_all ? "Transfer all photos/videos" : "Transfer new photos/videos")) << "\n" << endl;

    // Create device handler
    unique_ptr<DeviceHandler> handler = createDeviceHandler(device_type);
    if (!handler) {
        cerr << "ERROR: Invalid device type or device handler not available" << endl;
        return 1;
    }

    // Step 1: Detect devices
    cout << "Step 1: Detecting devices..." << endl;
    if (!handler->detectDevices()) {
        cerr << "ERROR: " << handler->getLastError() << endl;
        cerr << "\nTroubleshooting:" << endl;
        cerr << "1. Make sure your phone is connected via USB" << endl;
        cerr << "2. Unlock your phone" << endl;
        cerr << "3. For Android: Select 'File Transfer' or 'MTP' mode" << endl;
        cerr << "4. For iPhone: Trust this computer when prompted" << endl;
        cerr << "5. Make sure you have proper USB permissions" << endl;
        return 1;
    }
    cout << "✓ Devices detected!" << endl;

    // Step 2: Connect to device
    cout << "\nStep 2: Connecting to device..." << endl;
    if (!handler->connectToDevice()) {
        cerr << "ERROR: " << handler->getLastError() << endl;
        return 1;
    }
    cout << "✓ Connected!" << endl;

    // Step 3: Display device information
    cout << "\nStep 3: Device Information" << endl;
    cout << "Device Type: " << DeviceHandler::getDeviceTypeName(handler->getDeviceType()) << endl;
    cout << "Manufacturer: " << handler->getDeviceManufacturer() << endl;
    cout << "Model: " << handler->getDeviceModel() << endl;
    cout << "Full Name: " << handler->getDeviceName() << endl;

    // Step 4: List storage locations
    cout << "\nStep 4: Listing storage locations..." << endl;
    auto storages = handler->getStorageInfo();
    printStorageInfo(storages);

    // Step 5: Enumerate photos and videos
    cout << "\nStep 5: Enumerating photos and videos..." << endl;
    auto photos = handler->enumerateMedia();
    printMediaInfo(photos);

    // Summary
    cout << "\n=== Summary ===" << endl;
    cout << "Device: " << handler->getDeviceName() << endl;
    cout << "Storage locations: " << storages.size() << endl;
    cout << "Total media files found: " << photos.size() << endl;

    // If list-only mode, exit here
    if (list_only) {
        handler->disconnect();
        cout << "\n✓ Media listing completed!" << endl;
        return 0;
    }

    // Initialize database
    cout << "\n=== Initializing Database ===" << endl;
    string dest_folder = Utils::expandPath(destination);
    
    // Create destination directory if it doesn't exist
    if (!Utils::createDirectory(dest_folder)) {
        cerr << "ERROR: Failed to create destination directory: " << dest_folder << endl;
        handler->disconnect();
        return 1;
    }
    cout << "Destination folder: " << dest_folder << endl;
    
    string db_path = dest_folder + "/.photo_transfer.db";
    PhotoDB db;
    
    if (!db.open(db_path)) {
        cerr << "ERROR: Failed to open database: " << db.getLastError() << endl;
        handler->disconnect();
        return 1;
    }
    
    if (!db.initialize()) {
        cerr << "ERROR: Failed to initialize database: " << db.getLastError() << endl;
        handler->disconnect();
        return 1;
    }
    
    cout << "Database: " << db_path << endl;
    cout << "Photos in database: " << db.getPhotoCount() << endl;
    uint64_t total_size = db.getTotalSizeTransferred();
    cout << "Total size transferred: " << (total_size / (1024.0 * 1024.0)) << " MB" << endl;

    // Perform sync
    PhotoSync sync(handler.get(), &db, destination);
    PhotoSync::SyncResult result = sync.syncPhotos(!transfer_all);

    // Final summary
    cout << "\n=== Final Summary ===" << endl;
    cout << "Device: " << handler->getDeviceName() << endl;
    cout << "Device Type: " << DeviceHandler::getDeviceTypeName(handler->getDeviceType()) << endl;
    cout << "Total media on device: " << result.total_photos << endl;
    cout << "New media transferred: " << result.new_photos << endl;
    cout << "Skipped (already exist): " << result.skipped_photos << endl;
    cout << "Failed: " << result.failed_photos << endl;
    cout << "Total size transferred: " << (result.transferred_size / (1024.0 * 1024.0)) << " MB" << endl;
    cout << "Database now contains: " << db.getPhotoCount() << " photos" << endl;

    handler->disconnect(true); // Auto-unmount on disconnect
    cout << "\n✓ Photo transfer completed successfully!" << endl;
    cout << "Device has been released and unmounted." << endl;

    return 0;
}
