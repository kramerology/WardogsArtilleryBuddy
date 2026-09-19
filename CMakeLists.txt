cmake_minimum_required(VERSION 3.25)

project(WarDogsArtillery LANGUAGES CXX)

add_executable(WarDogsArtillery WIN32
    src/main.cpp
)

target_compile_features(WarDogsArtillery PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(WarDogsArtillery PRIVATE /W4 /permissive-)
    set_property(TARGET WarDogsArtillery PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    set(CPPWINRT_INCLUDE_DIR
        "$ENV{WindowsSdkDir}/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/cppwinrt")
    if (NOT EXISTS "${CPPWINRT_INCLUDE_DIR}")
        file(GLOB CPPWINRT_INCLUDE_CANDIDATES
            "C:/Program Files (x86)/Windows Kits/10/Include/*/cppwinrt")
        list(SORT CPPWINRT_INCLUDE_CANDIDATES ORDER DESCENDING)
        list(GET CPPWINRT_INCLUDE_CANDIDATES 0 CPPWINRT_INCLUDE_DIR)
    endif()
    target_include_directories(WarDogsArtillery PRIVATE "${CPPWINRT_INCLUDE_DIR}")
endif()

target_link_libraries(WarDogsArtillery PRIVATE
    Comctl32
    Advapi32
    Dwmapi
    Gdi32
    Shell32
    User32
    WindowsApp
    Winhttp
)

add_executable(ArtyBuddyUpdater WIN32
    src/updater.cpp
)

target_compile_features(ArtyBuddyUpdater PRIVATE cxx_std_20)

if (MSVC)
    target_compile_options(ArtyBuddyUpdater PRIVATE /W4 /permissive-)
    set_property(TARGET ArtyBuddyUpdater PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(ArtyBuddyUpdater PRIVATE
    Shell32
    User32
)
