cmake_minimum_required(VERSION 3.15)
project(PhotoTransfer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_GUI "Build Qt GUI version" ON)
option(ENABLE_ANDROID "Enable Android/MTP support" ON)
option(ENABLE_IOS "Enable iOS/libimobiledevice support" ON)

# Find required packages (PkgConfig not available on Windows)
if(NOT WIN32)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        # Find libmtp (for Android support)
        pkg_check_modules(LIBMTP libmtp)
        
        # Find libimobiledevice (for iOS support) - optional
        pkg_check_modules(LIBIMOBILEDEVICE libimobiledevice-1.0)
        
        # Find libfuse3 (for later phases) - optional
        pkg_check_modules(FUSE3 fuse3)
        
        # Add library directories for proper linking
        if(LIBMTP_FOUND)
            link_directories(${LIBMTP_LIBRARY_DIRS})
        endif()
        if(LIBIMOBILEDEVICE_FOUND)
            link_directories(${LIBIMOBILEDEVICE_LIBRARY_DIRS})
        endif()
    endif()
else()
    # Windows: Use WPD API for Android/MTP support (built into Windows)
    set(LIBMTP_FOUND FALSE)
    set(WPD_FOUND TRUE)
    set(LIBIMOBILEDEVICE_FOUND FALSE)
    set(FUSE3_FOUND FALSE)
    message(STATUS "Windows detected: Using Windows Portable Devices (WPD) API for MTP")
endif()

# Find SQLite3 (for photo tracking database)
find_library(SQLITE3_LIB sqlite3)
find_path(SQLITE3_INCLUDE sqlite3.h)

# Find OpenSSL (for hashing)
find_package(OpenSSL REQUIRED)

# Find Qt6 (for GUI) - optional
if(BUILD_GUI)
    find_package(Qt6 COMPONENTS Widgets Core Gui QUIET)
    if(NOT Qt6_FOUND)
        find_package(Qt5 COMPONENTS Widgets Core Gui QUIET)
        if(Qt5_FOUND)
            set(QT_FOUND TRUE)
            set(QT_VERSION 5)
        endif()
    else()
        set(QT_FOUND TRUE)
        set(QT_VERSION 6)
    endif()
    
    if(NOT QT_FOUND)
        message(WARNING "Qt not found. GUI will not be built.")
        set(BUILD_GUI OFF)
    endif()
endif()

# Validate device support
if(ENABLE_ANDROID)
    if(WIN32 AND WPD_FOUND)
        message(STATUS "Android support via Windows Portable Devices (WPD)")
    elseif(NOT LIBMTP_FOUND)
        message(WARNING "Android support requested but libmtp not found. Android support will be disabled.")
        set(ENABLE_ANDROID OFF)
    endif()
endif()

if(ENABLE_IOS AND NOT LIBIMOBILEDEVICE_FOUND)
    message(WARNING "iOS support requested but libimobiledevice not found. iOS support will be disabled.")
    set(ENABLE_IOS OFF)
endif()

# Include directories
set(INCLUDE_DIRS
    ${SQLITE3_INCLUDE}
    ${OPENSSL_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(LIBMTP_FOUND)
    list(APPEND INCLUDE_DIRS ${LIBMTP_INCLUDE_DIRS})
endif()

if(LIBIMOBILEDEVICE_FOUND)
    list(APPEND INCLUDE_DIRS ${LIBIMOBILEDEVICE_INCLUDE_DIRS})
endif()

if(FUSE3_FOUND)
    list(APPEND INCLUDE_DIRS ${FUSE3_INCLUDE_DIRS})
endif()

include_directories(${INCLUDE_DIRS})

# Core source files (shared between CLI and GUI)
set(CORE_SOURCES
    src/photo_db.cpp
    src/utils.cpp
    src/photo_sync.cpp
    src/config.cpp
    src/transfer_queue.cpp
)

# Add MTP/WPD handler if Android support is enabled
if(ENABLE_ANDROID)
    if(WIN32 AND WPD_FOUND)
        list(APPEND CORE_SOURCES src/wpd_handler.cpp)
        add_definitions(-DENABLE_ANDROID -DUSE_WPD)
    elseif(LIBMTP_FOUND)
        list(APPEND CORE_SOURCES src/mtp_handler.cpp)
        add_definitions(-DENABLE_ANDROID)
    endif()
endif()

# Add iOS handler if iOS support is enabled
if(ENABLE_IOS AND LIBIMOBILEDEVICE_FOUND)
    list(APPEND CORE_SOURCES src/ios_handler.cpp)
    add_definitions(-DENABLE_IOS)
endif()

# Link libraries (shared between CLI and GUI)
set(CORE_LIBS
    ${SQLITE3_LIB}
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(ENABLE_ANDROID)
    if(WIN32 AND WPD_FOUND)
        # WPD libraries are linked via #pragma comment in the source
        list(APPEND CORE_LIBS ole32 uuid)
    elseif(LIBMTP_FOUND)
        list(APPEND CORE_LIBS ${LIBMTP_LIBRARIES})
    endif()
endif()

if(ENABLE_IOS AND LIBIMOBILEDEVICE_FOUND)
    list(APPEND CORE_LIBS ${LIBIMOBILEDEVICE_LIBRARIES})
endif()

if(FUSE3_FOUND)
    list(APPEND CORE_LIBS ${FUSE3_LIBRARIES})
endif()

# Compiler flags
if(MSVC)
    set(COMPILE_FLAGS /W4)
else()
    set(COMPILE_FLAGS -Wall -Wextra)
endif()

if(LIBMTP_FOUND)
    list(APPEND COMPILE_FLAGS ${LIBMTP_CFLAGS_OTHER})
endif()

if(FUSE3_FOUND)
    list(APPEND COMPILE_FLAGS ${FUSE3_CFLAGS_OTHER})
endif()

# ==========================================
# CLI Executable
# ==========================================
set(CLI_SOURCES
    ${CORE_SOURCES}
    src/main.cpp
)

add_executable(photo_transfer ${CLI_SOURCES})
target_link_libraries(photo_transfer ${CORE_LIBS})
target_compile_options(photo_transfer PRIVATE ${COMPILE_FLAGS})

# ==========================================
# GUI Executable (Qt)
# ==========================================
if(BUILD_GUI AND QT_FOUND)
    # Enable Qt MOC, UIC, RCC
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
    
    set(GUI_SOURCES
        ${CORE_SOURCES}
        gui/main.cpp
        gui/mainwindow.cpp
        gui/mainwindow.h
        gui/settingsdialog.cpp
        gui/settingsdialog.h
    )
    
    if(QT_VERSION EQUAL 6)
        qt6_add_executable(photo_transfer_gui ${GUI_SOURCES})
        target_link_libraries(photo_transfer_gui PRIVATE 
            ${CORE_LIBS}
            Qt6::Widgets
            Qt6::Core
            Qt6::Gui
        )
    else()
        add_executable(photo_transfer_gui ${GUI_SOURCES})
        target_link_libraries(photo_transfer_gui 
            ${CORE_LIBS}
            Qt5::Widgets
            Qt5::Core
            Qt5::Gui
        )
    endif()
    
    target_compile_options(photo_transfer_gui PRIVATE ${COMPILE_FLAGS})
    
    # Platform-specific GUI settings
    if(APPLE)
        set_target_properties(photo_transfer_gui PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/Info.plist
        )
    elseif(WIN32)
        set_target_properties(photo_transfer_gui PROPERTIES
            WIN32_EXECUTABLE TRUE
        )
    endif()
endif()

# Installation
install(TARGETS photo_transfer DESTINATION bin)
if(BUILD_GUI AND QT_FOUND)
    install(TARGETS photo_transfer_gui DESTINATION bin)
endif()

# ==========================================
# Packaging
# ==========================================
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "PhotoTransfer")
set(CPACK_PACKAGE_VENDOR "PhotoTransfer")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform photo transfer from mobile devices")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "PhotoTransfer")

# Platform-specific packaging
if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "PhotoTransfer")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Photo Transfer")
    set(CPACK_NSIS_PACKAGE_NAME "PhotoTransfer")
else()
    set(CPACK_GENERATOR "TGZ;DEB;RPM")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libmtp9, libimobiledevice6, libssl3, libsqlite3-0")
    set(CPACK_RPM_PACKAGE_REQUIRES "libmtp, libimobiledevice, openssl, sqlite")
endif()

include(CPack)

# Print configuration summary
message(STATUS "")
message(STATUS "=== PhotoTransfer Configuration ===")
message(STATUS "Android support (libmtp): ${ENABLE_ANDROID}")
message(STATUS "iOS support (libimobiledevice): ${ENABLE_IOS}")
message(STATUS "FUSE support: ${FUSE3_FOUND}")
message(STATUS "Qt GUI: ${BUILD_GUI}")
if(QT_FOUND)
    message(STATUS "Qt version: ${QT_VERSION}")
endif()
message(STATUS "SQLite3: ${SQLITE3_LIB}")
message(STATUS "OpenSSL: ${OPENSSL_FOUND}")
message(STATUS "")
