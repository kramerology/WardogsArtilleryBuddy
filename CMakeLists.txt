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
    Dwmapi
    Gdi32
    Shell32
    User32
    WindowsApp
)
