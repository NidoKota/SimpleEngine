# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

set(APP_NAME "vulkanslidetest")
project(${APP_NAME} VERSION 1.0 LANGUAGES CXX)

set(APPLICATION_SRC_DIR ${PROJECT_SOURCE_DIR}/../../../../../../Vulkan_Test)

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(${APP_NAME} SHARED
        main.cpp
        AndroidOut.cpp
        Renderer.cpp
)

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

target_compile_definitions(${APP_NAME} PRIVATE VK_USE_PLATFORM_ANDROID)
target_compile_definitions(${APP_NAME} PRIVATE VK_USE_PLATFORM_ANDROID_KHR)

target_include_directories(${APP_NAME} PUBLIC
        ${APPLICATION_SRC_DIR}/android/app/Vulkan-Hpp)

# Configure libraries CMake uses to link your target library.
target_link_libraries(${APP_NAME}
        # The game activity
        game-activity::game-activity
        vulkan
        android
        log)