cmake_minimum_required(VERSION 3.16)

project(SecureVault VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

find_package(Qt6 REQUIRED COMPONENTS Quick Concurrent Gui)

qt_standard_project_setup(REQUIRES 6.10)

include(FetchContent)

if (NOT TARGET sodium AND NOT TARGET unofficial-sodium::sodium)
    find_package(unofficial-sodium CONFIG QUIET)
endif()

if (TARGET unofficial-sodium::sodium)
    add_library(sodium ALIAS unofficial-sodium::sodium)
elseif (NOT TARGET sodium)
    set(SODIUM_DISABLE_TESTS ON CACHE BOOL "" FORCE)
    set(SODIUM_MINIMAL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        Sodium
        GIT_REPOSITORY https://github.com/robinlinden/libsodium-cmake.git
        GIT_TAG 99f14233eab1d4f7f49c2af4ec836f2e701c445e
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(Sodium)
endif()

set(VAULT_CORE_SOURCES
    src/crypto/CryptoService.cpp
    src/crypto/CryptoService.h
    src/vault/BlobStore.h
    src/vault/ContainerBlobStore.cpp
    src/vault/ContainerBlobStore.h
    src/vault/EncryptedIndex.cpp
    src/vault/EncryptedIndex.h
    src/vault/FileIndexModel.cpp
    src/vault/FileIndexModel.h
    src/vault/FolderBlobStore.cpp
    src/vault/FolderBlobStore.h
    src/vault/VaultEntry.h
    src/vault/VaultFormat.cpp
    src/vault/VaultFormat.h
    src/vault/VaultSession.cpp
    src/vault/VaultSession.h
)

add_library(vaultcore STATIC ${VAULT_CORE_SOURCES})
target_include_directories(vaultcore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(vaultcore PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent sodium)
set_target_properties(vaultcore PROPERTIES AUTOMOC ON POSITION_INDEPENDENT_CODE ON)

if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(appSecureVault
    main.cpp
    src/app/AppSettings.cpp
    src/app/AppSettings.h
)

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(appSecureVault
    URI SecureVault
    QML_FILES
        Main.qml
        Theme.qml
        LoginScreen.qml
        VaultScreen.qml
        VaultGlyph.qml
        AppDialog.qml
        FileGlyph.qml
        AppButton.qml
        AppTextField.qml
        Toast.qml
        CheckSquare.qml
        AppCheckBox.qml
        ChangePasswordDialog.qml
        SettingsScreen.qml
)

set_target_properties(appSecureVault PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(appSecureVault
    PRIVATE vaultcore Qt6::Quick
)

add_executable(vault_tests
    tests/vault_roundtrip.cpp
)
set_target_properties(vault_tests PROPERTIES WIN32_EXECUTABLE FALSE AUTOMOC ON)
target_link_libraries(vault_tests PRIVATE vaultcore)

enable_testing()
add_test(NAME vault_roundtrip COMMAND vault_tests)

install(TARGETS appSecureVault
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
