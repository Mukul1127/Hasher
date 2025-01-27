cmake_minimum_required(VERSION 3.10.0)
project(Hasher VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)

list(APPEND CMAKE_PREFIX_PATH "vendor/wolfssl")

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(wolfssl CONFIG REQUIRED)
find_package(Freetype REQUIRED)

include_directories(include vendor/imgui vendor/imgui/backends vendor/imgui/misc/freetype vendor/imguifiledialog)

add_executable(Hasher src/main.cpp src/hash.cpp vendor/imgui/imgui_demo.cpp vendor/imgui/imgui_draw.cpp vendor/imgui/imgui_tables.cpp vendor/imgui/imgui_widgets.cpp vendor/imgui/imgui.cpp vendor/imgui/backends/imgui_impl_glfw.cpp vendor/imgui/backends/imgui_impl_vulkan.cpp vendor/imgui/misc/freetype/imgui_freetype.cpp vendor/imguifiledialog/ImGuiFileDialog.cpp)

target_link_libraries(Hasher PRIVATE glfw Vulkan::Vulkan wolfssl Freetype::Freetype) 