cmake_minimum_required(VERSION 3.10.0)
project(main VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Freetype
find_package(Freetype REQUIRED)

# GLFW
find_package(glfw3 CONFIG REQUIRED)

# Vulkan
find_package(Vulkan REQUIRED)

# WolfSSL
list(APPEND CMAKE_PREFIX_PATH "vendor/wolfssl")
find_package(wolfssl CONFIG REQUIRED)

# ImGui
add_library(imgui STATIC
        vendor/imgui/imgui_demo.cpp
        vendor/imgui/imgui_draw.cpp
        vendor/imgui/imgui_tables.cpp
        vendor/imgui/imgui_widgets.cpp
        vendor/imgui/imgui.cpp
        vendor/imgui/backends/imgui_impl_glfw.cpp
        vendor/imgui/backends/imgui_impl_vulkan.cpp
        vendor/imgui/misc/freetype/imgui_freetype.cpp
)
target_include_directories(imgui PUBLIC
        vendor/imgui
        vendor/imgui/backends
        vendor/imgui/misc/freetype
)
target_link_libraries(imgui PUBLIC glfw Vulkan::Vulkan Freetype::Freetype)

# ImGuiFileDialog
add_library(imguifiledialog STATIC vendor/imguifiledialog/ImGuiFileDialog.cpp)
target_include_directories(imguifiledialog PUBLIC vendor/imguifiledialog)
target_link_libraries(imguifiledialog PUBLIC imgui)

# Exe
include_directories(include)
add_executable(main src/main.cpp src/hash.cpp)
target_link_libraries(main PRIVATE wolfssl imgui imguifiledialog)