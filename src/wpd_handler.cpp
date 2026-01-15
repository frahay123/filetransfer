#ifdef _WIN32

#include "wpd_handler.h"
#include <windows.h>
#include <PortableDeviceApi.h>
#include <PortableDevice.h>
#include <wrl/client.h>
#include <propvarutil.h>
#include <iostream>
#include <algorithm>
#include <cctype>

#pragma comment(lib, "PortableDeviceGUIDs.lib")
#pragma comment(lib, "Propsys.lib")

using Microsoft::WRL::ComPtr;
using namespace std;

// Client information for WPD
static const wchar_t* CLIENT_NAME = L"PhotoTransfer";
static const wchar_t* CLIENT_MAJOR_VER = L"1";
static const wchar_t* CLIENT_MINOR_VER = L"0";
static const wchar_t* CLIENT_REVISION = L"0";

WPDHandler::WPDHandler()
    : device_manager_(nullptr)
    , device_(nullptr)
    , content_(nullptr)
    , com_initialized_(false)
    , connected_(false)
{
    initializeCOM();
}

WPDHandler::~WPDHandler() {
    disconnect();
    uninitializeCOM();
}

bool WPDHandler::initializeCOM() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == S_FALSE) {
        com_initialized_ = true;
        return true;
    }
    setError("Failed to initialize COM");
    return false;
}

void WPDHandler::uninitializeCOM() {
    if (com_initialized_) {
        CoUninitialize();
        com_initialized_ = false;
    }
}

wstring WPDHandler::stringToWide(const string& str) {
    if (str.empty()) return wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    wstring result(size - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

string WPDHandler::wideToString(const wstring& wstr) {
    if (wstr.empty()) return string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    string result(size - 1, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

bool WPDHandler::detectDevices() {
    if (!com_initialized_) {
        setError("COM not initialized");
        return false;
    }

    // Create device manager
    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceManager, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device_manager_));
    if (FAILED(hr)) {
        setError("Failed to create device manager");
        return false;
    }

    // Get device count
    DWORD device_count = 0;
    hr = device_manager_->GetDevices(nullptr, &device_count);
    if (FAILED(hr)) {
        setError("Failed to get device count");
        return false;
    }

    if (device_count == 0) {
        setError("No MTP devices detected. Make sure your phone is connected and unlocked.");
        return false;
    }

    cout << "Found " << device_count << " portable device(s)" << endl;

    // Get device IDs
    vector<PWSTR> device_ids(device_count);
    hr = device_manager_->GetDevices(device_ids.data(), &device_count);
    if (FAILED(hr)) {
        setError("Failed to get device IDs");
        return false;
    }

    // Store first device ID
    if (device_count > 0 && device_ids[0]) {
        device_id_ = device_ids[0];
        
        // Get device friendly name
        DWORD name_length = 0;
        device_manager_->GetDeviceFriendlyName(device_ids[0], nullptr, &name_length);
        if (name_length > 0) {
            vector<wchar_t> name(name_length);
            if (SUCCEEDED(device_manager_->GetDeviceFriendlyName(device_ids[0], name.data(), &name_length))) {
                device_name_ = wideToString(name.data());
            }
        }

        // Get manufacturer
        DWORD mfr_length = 0;
        device_manager_->GetDeviceManufacturer(device_ids[0], nullptr, &mfr_length);
        if (mfr_length > 0) {
            vector<wchar_t> mfr(mfr_length);
            if (SUCCEEDED(device_manager_->GetDeviceManufacturer(device_ids[0], mfr.data(), &mfr_length))) {
                device_manufacturer_ = wideToString(mfr.data());
            }
        }

        // Get description as model
        DWORD desc_length = 0;
        device_manager_->GetDeviceDescription(device_ids[0], nullptr, &desc_length);
        if (desc_length > 0) {
            vector<wchar_t> desc(desc_length);
            if (SUCCEEDED(device_manager_->GetDeviceDescription(device_ids[0], desc.data(), &desc_length))) {
                device_model_ = wideToString(desc.data());
            }
        }

        cout << "Device: " << device_name_ << " (" << device_manufacturer_ << ")" << endl;
    }

    // Free device IDs
    for (auto& id : device_ids) {
        CoTaskMemFree(id);
    }

    return true;
}

bool WPDHandler::connectToDevice(const string& device_name, bool auto_unmount) {
    (void)device_name;  // Not used in WPD - we use the detected device
    (void)auto_unmount;

    if (device_id_.empty()) {
        setError("No device detected. Call detectDevices() first.");
        return false;
    }

    // Create client information
    ComPtr<IPortableDeviceValues> client_info;
    HRESULT hr = CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                   CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&client_info));
    if (FAILED(hr)) {
        setError("Failed to create client info");
        return false;
    }

    client_info->SetStringValue(WPD_CLIENT_NAME, CLIENT_NAME);
    client_info->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, 1);
    client_info->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, 0);
    client_info->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, 0);
    client_info->SetUnsignedIntegerValue(WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);

    // Create device object
    hr = CoCreateInstance(CLSID_PortableDeviceFTM, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device_));
    if (FAILED(hr)) {
        setError("Failed to create device object");
        return false;
    }

    // Open device
    hr = device_->Open(device_id_.c_str(), client_info.Get());
    if (FAILED(hr)) {
        setError("Failed to open device. Make sure the phone is unlocked and set to file transfer mode.");
        device_->Release();
        device_ = nullptr;
        return false;
    }

    // Get content interface
    hr = device_->Content(&content_);
    if (FAILED(hr)) {
        setError("Failed to get device content");
        device_->Close();
        device_->Release();
        device_ = nullptr;
        return false;
    }

    connected_ = true;
    cout << "Connected to device: " << device_name_ << endl;
    return true;
}

void WPDHandler::disconnect(bool auto_unmount) {
    (void)auto_unmount;

    if (content_) {
        content_->Release();
        content_ = nullptr;
    }

    if (device_) {
        device_->Close();
        device_->Release();
        device_ = nullptr;
    }

    if (device_manager_) {
        device_manager_->Release();
        device_manager_ = nullptr;
    }

    connected_ = false;
    object_id_map_.clear();
}

bool WPDHandler::isConnected() const {
    return connected_;
}

string WPDHandler::getDeviceName() const {
    return device_name_;
}

string WPDHandler::getDeviceManufacturer() const {
    return device_manufacturer_;
}

string WPDHandler::getDeviceModel() const {
    return device_model_;
}

vector<DeviceStorageInfo> WPDHandler::getStorageInfo() const {
    vector<DeviceStorageInfo> storages;
    
    if (!content_) return storages;

    // Enumerate storage objects
    ComPtr<IEnumPortableDeviceObjectIDs> enum_ids;
    HRESULT hr = content_->EnumObjects(0, WPD_DEVICE_OBJECT_ID, nullptr, &enum_ids);
    if (FAILED(hr)) return storages;

    PWSTR object_ids[10];
    DWORD fetched = 0;
    uint32_t storage_index = 0;

    while (SUCCEEDED(enum_ids->Next(10, object_ids, &fetched)) && fetched > 0) {
        for (DWORD i = 0; i < fetched; i++) {
            // Get properties
            ComPtr<IPortableDeviceProperties> properties;
            if (SUCCEEDED(content_->Properties(&properties))) {
                ComPtr<IPortableDeviceValues> values;
                if (SUCCEEDED(properties->GetValues(object_ids[i], nullptr, &values))) {
                    GUID content_type;
                    if (SUCCEEDED(values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE, &content_type))) {
                        if (IsEqualGUID(content_type, WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT)) {
                            DeviceStorageInfo info;
                            info.storage_id = storage_index++;
                            
                            PWSTR name = nullptr;
                            if (SUCCEEDED(values->GetStringValue(WPD_OBJECT_NAME, &name))) {
                                info.description = wideToString(name);
                                CoTaskMemFree(name);
                            }
                            
                            ULONGLONG capacity = 0, free_space = 0;
                            values->GetUnsignedLargeIntegerValue(WPD_STORAGE_CAPACITY, &capacity);
                            values->GetUnsignedLargeIntegerValue(WPD_STORAGE_FREE_SPACE_IN_BYTES, &free_space);
                            
                            info.max_capacity = capacity;
                            info.free_space = free_space;
                            info.storage_type = 3; // Fixed storage
                            
                            if (info.max_capacity > 0) {
                                storages.push_back(info);
                            }
                        }
                    }
                }
            }
            CoTaskMemFree(object_ids[i]);
        }
        fetched = 0;
    }

    return storages;
}

bool WPDHandler::isMediaFile(const wstring& filename) {
    wstring ext = filename;
    size_t pos = ext.rfind(L'.');
    if (pos == wstring::npos) return false;
    ext = ext.substr(pos);
    
    // Convert to lowercase
    transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    
    // Photo extensions
    if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || 
        ext == L".gif" || ext == L".bmp" || ext == L".heic" ||
        ext == L".heif" || ext == L".webp" || ext == L".raw" ||
        ext == L".cr2" || ext == L".nef" || ext == L".arw") {
        return true;
    }
    
    // Video extensions
    if (ext == L".mp4" || ext == L".mov" || ext == L".avi" ||
        ext == L".mkv" || ext == L".wmv" || ext == L".3gp" ||
        ext == L".m4v" || ext == L".webm") {
        return true;
    }
    
    return false;
}

string WPDHandler::getMimeType(const wstring& filename) {
    wstring ext = filename;
    size_t pos = ext.rfind(L'.');
    if (pos == wstring::npos) return "application/octet-stream";
    ext = ext.substr(pos);
    transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
    
    if (ext == L".jpg" || ext == L".jpeg") return "image/jpeg";
    if (ext == L".png") return "image/png";
    if (ext == L".gif") return "image/gif";
    if (ext == L".heic" || ext == L".heif") return "image/heic";
    if (ext == L".mp4" || ext == L".m4v") return "video/mp4";
    if (ext == L".mov") return "video/quicktime";
    if (ext == L".avi") return "video/x-msvideo";
    if (ext == L".mkv") return "video/x-matroska";
    
    return "application/octet-stream";
}

void WPDHandler::enumerateContent(const wstring& parent_id, vector<MediaInfo>& media) {
    if (!content_) return;

    ComPtr<IEnumPortableDeviceObjectIDs> enum_ids;
    HRESULT hr = content_->EnumObjects(0, parent_id.c_str(), nullptr, &enum_ids);
    if (FAILED(hr)) return;

    PWSTR object_ids[100];
    DWORD fetched = 0;

    while (SUCCEEDED(enum_ids->Next(100, object_ids, &fetched)) && fetched > 0) {
        for (DWORD i = 0; i < fetched; i++) {
            ComPtr<IPortableDeviceProperties> properties;
            if (FAILED(content_->Properties(&properties))) {
                CoTaskMemFree(object_ids[i]);
                continue;
            }

            ComPtr<IPortableDeviceValues> values;
            if (FAILED(properties->GetValues(object_ids[i], nullptr, &values))) {
                CoTaskMemFree(object_ids[i]);
                continue;
            }

            GUID content_type;
            if (SUCCEEDED(values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE, &content_type))) {
                if (IsEqualGUID(content_type, WPD_CONTENT_TYPE_FOLDER) ||
                    IsEqualGUID(content_type, WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT)) {
                    // Recurse into folders
                    enumerateContent(object_ids[i], media);
                } else {
                    // Check if it's a media file
                    PWSTR name = nullptr;
                    if (SUCCEEDED(values->GetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME, &name)) ||
                        SUCCEEDED(values->GetStringValue(WPD_OBJECT_NAME, &name))) {
                        wstring filename = name;
                        CoTaskMemFree(name);
                        
                        if (isMediaFile(filename)) {
                            MediaInfo info;
                            info.object_id = static_cast<uint32_t>(object_id_map_.size());
                            object_id_map_.push_back(object_ids[i]);
                            
                            info.filename = wideToString(filename);
                            info.mime_type = getMimeType(filename);
                            
                            ULONGLONG size = 0;
                            values->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE, &size);
                            info.file_size = size;
                            
                            // Get modification date
                            PROPVARIANT pv;
                            PropVariantInit(&pv);
                            if (SUCCEEDED(values->GetValue(WPD_OBJECT_DATE_MODIFIED, &pv))) {
                                if (pv.vt == VT_DATE) {
                                    SYSTEMTIME st;
                                    VariantTimeToSystemTime(pv.date, &st);
                                    FILETIME ft;
                                    SystemTimeToFileTime(&st, &ft);
                                    ULARGE_INTEGER uli;
                                    uli.LowPart = ft.dwLowDateTime;
                                    uli.HighPart = ft.dwHighDateTime;
                                    info.modification_date = (uli.QuadPart - 116444736000000000ULL) / 10000000ULL;
                                }
                                PropVariantClear(&pv);
                            }
                            
                            media.push_back(info);
                        }
                    }
                }
            }
            CoTaskMemFree(object_ids[i]);
        }
        fetched = 0;
    }
}

vector<MediaInfo> WPDHandler::enumerateMedia(const string& directory_path) {
    (void)directory_path;  // We enumerate all media
    
    vector<MediaInfo> media;
    object_id_map_.clear();
    
    if (!content_) {
        setError("Not connected to device");
        return media;
    }

    cout << "Enumerating media files..." << endl;
    enumerateContent(WPD_DEVICE_OBJECT_ID, media);
    cout << "Found " << media.size() << " media files" << endl;
    
    return media;
}

bool WPDHandler::readFile(uint32_t object_id, vector<uint8_t>& data) {
    if (!content_ || object_id >= object_id_map_.size()) {
        setError("Invalid object ID or not connected");
        return false;
    }

    const wstring& obj_id = object_id_map_[object_id];
    
    ComPtr<IPortableDeviceResources> resources;
    HRESULT hr = content_->Transfer(&resources);
    if (FAILED(hr)) {
        setError("Failed to get transfer interface");
        return false;
    }

    ComPtr<IStream> stream;
    DWORD optimal_buffer_size = 0;
    hr = resources->GetStream(obj_id.c_str(), WPD_RESOURCE_DEFAULT, STGM_READ,
                               &optimal_buffer_size, &stream);
    if (FAILED(hr)) {
        setError("Failed to open file stream");
        return false;
    }

    // Read file in chunks
    data.clear();
    vector<BYTE> buffer(optimal_buffer_size > 0 ? optimal_buffer_size : 262144);
    ULONG bytes_read = 0;

    while (true) {
        hr = stream->Read(buffer.data(), static_cast<ULONG>(buffer.size()), &bytes_read);
        if (FAILED(hr) || bytes_read == 0) break;
        data.insert(data.end(), buffer.begin(), buffer.begin() + bytes_read);
    }

    return true;
}

bool WPDHandler::deleteFile(uint32_t object_id) {
    // Not implemented - we don't delete files
    (void)object_id;
    return false;
}

bool WPDHandler::fileExists(uint32_t object_id) {
    return object_id < object_id_map_.size();
}

#endif // _WIN32
