#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN

#include <iostream>
#include <format>
#include <map>
#include <filesystem>
#include <future>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "ImGuiFileDialog.h"
#include <GLFW/glfw3.h>
#include "hash.h"

static VkAllocationCallbacks*   g_Allocator = nullptr;
static VkInstance               g_Instance = VK_NULL_HANDLE;
static VkPhysicalDevice         g_PhysicalDevice = VK_NULL_HANDLE;
static VkDevice                 g_Device = VK_NULL_HANDLE;
static uint32_t                 g_QueueFamily = (uint32_t)-1;
static VkQueue                  g_Queue = VK_NULL_HANDLE;
static VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
static VkPipelineCache          g_PipelineCache = VK_NULL_HANDLE;
static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;
static ImGui_ImplVulkanH_Window g_MainWindowData;
static uint32_t                 g_MinImageCount = 2;
static bool                     g_SwapChainRebuild = false;

static void glfw_error_callback(int error, const char* description)
{
    std::cerr << std::format("GLFW Error {}: {}", error, description) << std::endl;
}

static void check_vk_result(VkResult err)
{
    if (err == VK_SUCCESS)
    {
        return;
    }
    std::cerr << std::format("[vulkan] Error: VkResult = {}", static_cast<int>(err)) << std::endl;
    if (err < 0)
    {
        std::terminate();
    }
}

static bool IsExtensionAvailable(const ImVector<VkExtensionProperties>& properties, const char* extension)
{
    for (const VkExtensionProperties& p : properties)
    {
        if (strcmp(p.extensionName, extension) == 0)
        {
            return true;
        }
    }
    return false;
}

static void SetupVulkan(ImVector<const char*> instance_extensions)
{
    VkResult err;

    // Create Vulkan Instance
    {
        VkInstanceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

        // Enumerate available extensions
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
        check_vk_result(err);

        // Enable required extensions
        if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
        }
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
        {
            instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }
#endif

        // Create Vulkan Instance
        create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
        create_info.ppEnabledExtensionNames = instance_extensions.Data;
        err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
        check_vk_result(err);
    }

    // Select Physical Device (GPU)
    g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
    IM_ASSERT(g_PhysicalDevice != VK_NULL_HANDLE);

    // Select graphics queue family
    g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
    IM_ASSERT(g_QueueFamily != (uint32_t)-1);

    // Create Logical Device (with 1 queue)
    {
        ImVector<const char*> device_extensions;
        device_extensions.push_back("VK_KHR_swapchain");

        // Enumerate physical device extension
        uint32_t properties_count;
        ImVector<VkExtensionProperties> properties;
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, nullptr);
        properties.resize(properties_count);
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &properties_count, properties.Data);
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
        if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        {
            device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
        }
#endif

        const float queue_priority[] = { 1.0f };
        VkDeviceQueueCreateInfo queue_info[1] = {};
        queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_info[0].queueFamilyIndex = g_QueueFamily;
        queue_info[0].queueCount = 1;
        queue_info[0].pQueuePriorities = queue_priority;
        VkDeviceCreateInfo create_info = {};
        create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
        create_info.pQueueCreateInfos = queue_info;
        create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
        create_info.ppEnabledExtensionNames = device_extensions.Data;
        err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
        check_vk_result(err);
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
    }

    // Create Descriptor Pool
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
        {
            pool_info.maxSets += pool_size.descriptorCount;
        }
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
        check_vk_result(err);
    }
}

static void SetupVulkanWindow(ImGui_ImplVulkanH_Window* wd, VkSurfaceKHR surface, int width, int height)
{
    wd->Surface = surface;

    // Check for WSI support
    VkBool32 res;
    vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, wd->Surface, &res);
    if (res != VK_TRUE)
    {
        std::cerr << "Error no WSI support on physical device 0" << std::endl;
        std::exit(-1);
    }

    // Select Surface Format
    const VkFormat requestSurfaceImageFormat[] = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8_UNORM, VK_FORMAT_R8G8B8_UNORM };
    const VkColorSpaceKHR requestSurfaceColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
    wd->SurfaceFormat = ImGui_ImplVulkanH_SelectSurfaceFormat(g_PhysicalDevice, wd->Surface, requestSurfaceImageFormat, (size_t)IM_ARRAYSIZE(requestSurfaceImageFormat), requestSurfaceColorSpace);

    // Select Present Mode
    VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_FIFO_KHR };
    // VkPresentModeKHR present_modes[] = { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR };
    wd->PresentMode = ImGui_ImplVulkanH_SelectPresentMode(g_PhysicalDevice, wd->Surface, &present_modes[0], IM_ARRAYSIZE(present_modes));

    // Create SwapChain, RenderPass, Framebuffer, etc.
    IM_ASSERT(g_MinImageCount >= 2);
    ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, wd, g_QueueFamily, g_Allocator, width, height, g_MinImageCount);
}

static void CleanupVulkan()
{
    vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);

    vkDestroyDevice(g_Device, g_Allocator);
    vkDestroyInstance(g_Instance, g_Allocator);
}

static void CleanupVulkanWindow()
{
    ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
}

static void FrameRender(ImGui_ImplVulkanH_Window* wd, ImDrawData* draw_data)
{
    VkSemaphore image_acquired_semaphore  = wd->FrameSemaphores[wd->SemaphoreIndex].ImageAcquiredSemaphore;
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkResult err = vkAcquireNextImageKHR(g_Device, wd->Swapchain, UINT64_MAX, image_acquired_semaphore, VK_NULL_HANDLE, &wd->FrameIndex);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (err != VK_SUBOPTIMAL_KHR)
    {
        check_vk_result(err);
    }

    ImGui_ImplVulkanH_Frame* fd = &wd->Frames[wd->FrameIndex];
    {
        err = vkWaitForFences(g_Device, 1, &fd->Fence, VK_TRUE, UINT64_MAX); // wait indefinitely instead of periodically checking
        check_vk_result(err);

        err = vkResetFences(g_Device, 1, &fd->Fence);
        check_vk_result(err);
    }
    {
        err = vkResetCommandPool(g_Device, fd->CommandPool, 0);
        check_vk_result(err);
        VkCommandBufferBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        err = vkBeginCommandBuffer(fd->CommandBuffer, &info);
        check_vk_result(err);
    }
    {
        VkRenderPassBeginInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        info.renderPass = wd->RenderPass;
        info.framebuffer = fd->Framebuffer;
        info.renderArea.extent.width = wd->Width;
        info.renderArea.extent.height = wd->Height;
        info.clearValueCount = 1;
        info.pClearValues = &wd->ClearValue;
        vkCmdBeginRenderPass(fd->CommandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);
    }

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(draw_data, fd->CommandBuffer);

    // Submit command buffer
    vkCmdEndRenderPass(fd->CommandBuffer);
    {
        VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        info.waitSemaphoreCount = 1;
        info.pWaitSemaphores = &image_acquired_semaphore;
        info.pWaitDstStageMask = &wait_stage;
        info.commandBufferCount = 1;
        info.pCommandBuffers = &fd->CommandBuffer;
        info.signalSemaphoreCount = 1;
        info.pSignalSemaphores = &render_complete_semaphore;

        err = vkEndCommandBuffer(fd->CommandBuffer);
        check_vk_result(err);
        err = vkQueueSubmit(g_Queue, 1, &info, fd->Fence);
        check_vk_result(err);
    }
}

static void FramePresent(ImGui_ImplVulkanH_Window* wd)
{
    if (g_SwapChainRebuild)
    {
        return;
    }
    VkSemaphore render_complete_semaphore = wd->FrameSemaphores[wd->SemaphoreIndex].RenderCompleteSemaphore;
    VkPresentInfoKHR info = {};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = 1;
    info.pWaitSemaphores = &render_complete_semaphore;
    info.swapchainCount = 1;
    info.pSwapchains = &wd->Swapchain;
    info.pImageIndices = &wd->FrameIndex;
    VkResult err = vkQueuePresentKHR(g_Queue, &info);
    if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR)
    {
        g_SwapChainRebuild = true;
    }
    if (err == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }
    if (err != VK_SUBOPTIMAL_KHR)
    {
        check_vk_result(err);
    }
    wd->SemaphoreIndex = (wd->SemaphoreIndex + 1) % wd->SemaphoreCount; // Now we can use the next set of semaphores
}

// Main code
int main(int argc, char* argv[])
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
    {
        return 1;
    }

    // Create window with Vulkan context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow* window = glfwCreateWindow(1, 1, "", nullptr, nullptr);
    if (!glfwVulkanSupported())
    {
        std::cerr << "GLFW: Vulkan Not Supported" << std::endl;
        return 1;
    }

    ImVector<const char*> extensions;
    uint32_t extensions_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++)
    {
        extensions.push_back(glfw_extensions[i]);
    }
    SetupVulkan(extensions);

    // Create Window Surface
    VkSurfaceKHR surface;
    VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
    check_vk_result(err);

    // Create Framebuffers
    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    ImGui_ImplVulkanH_Window* wd = &g_MainWindowData;
    SetupVulkanWindow(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigViewportsNoAutoMerge = true;
    io.ConfigDockingTransparentPayload = true;
    io.IniFilename = NULL;

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = g_Instance;
    init_info.PhysicalDevice = g_PhysicalDevice;
    init_info.Device = g_Device;
    init_info.QueueFamily = g_QueueFamily;
    init_info.Queue = g_Queue;
    init_info.PipelineCache = g_PipelineCache;
    init_info.DescriptorPool = g_DescriptorPool;
    init_info.RenderPass = wd->RenderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = g_MinImageCount;
    init_info.ImageCount = wd->ImageCount;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = g_Allocator;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);

    // Fonts
    io.Fonts->AddFontFromFileTTF("assets/Inter-Medium.woff2", 15.0f);
    ImFont* cascadia = io.Fonts->AddFontFromFileTTF("assets/CascadiaCodeNF-Regular.woff2", 15.0f);

    // State
    std::atomic<bool> hashThreadShouldCancel(false);
    bool isCalculating = true;
    std::string errorMessage = "";
    bool running = true;
    std::string filePath = "";

    bool showDemoWindow = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Set file path
    if (argc > 1)
    {
        if (std::filesystem::exists(argv[1]))
        {
            if (!std::filesystem::is_empty(argv[1]))
            {
                filePath = argv[1];
            }
            else
            {
                errorMessage = "File passed is empty";
            }
        }
        else
        {
            errorMessage = "File passed doesn't exist";
        }
    }
    else
    {
        errorMessage = "No file passed";
    }

    std::vector<wc_HashType> hashesToCalculate = {
        WC_HASH_TYPE_MD5,
        WC_HASH_TYPE_SHA,
        WC_HASH_TYPE_SHA256,
        WC_HASH_TYPE_SHA512,
        WC_HASH_TYPE_SHA3_256,
        WC_HASH_TYPE_SHA3_512,
        // WC_HASH_TYPE_BLAKE2B,
    };

    std::map<wc_HashType, std::string> displayNames = {
        {WC_HASH_TYPE_MD5, "MD5"},
        {WC_HASH_TYPE_SHA, "SHA1"},
        {WC_HASH_TYPE_SHA256, "SHA256"},
        {WC_HASH_TYPE_SHA512, "SHA512"},
        {WC_HASH_TYPE_SHA3_256, "SHA3_256"},
        {WC_HASH_TYPE_SHA3_512, "SHA3_512"},
        // {WC_HASH_TYPE_BLAKE2B, "BLAKE2b"},
    };

    std::future<std::map<wc_HashType, std::string>> hashThread;

    if (!filePath.empty())
    {
        hashThread = std::async(std::launch::async, [&]() {
            return calculateHashes(filePath, hashesToCalculate, hashThreadShouldCancel);
        });
    }

    // Main loop
    while (!glfwWindowShouldClose(window))
    {
        // Poll and handle events
        glfwPollEvents();

        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 && (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily, g_Allocator, fb_width, fb_height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (!running)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        // Main content
        static std::map<wc_HashType, std::string> calculatedHashes = {};

        if (errorMessage == "" && isCalculating)
        {
            if (hashThread.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                calculatedHashes = hashThread.get();
                isCalculating = false;
            }
        }

        ImGui::Begin("Hasher", &running, ImGuiWindowFlags_MenuBar);
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Show Demo Window"))
                {
                    showDemoWindow = true;
                }
                ImGui::Separator();
                // Open file dialog
                if (ImGui::MenuItem("Open"))
                {
                    IGFD::FileDialogConfig config;
	                config.path = ".";
                    ImGuiFileDialog::Instance()->OpenDialog("ChooseHashFile", "Choose File", ".*", config);
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        if (showDemoWindow)
        {
            ImGui::ShowDemoWindow(&showDemoWindow);
        }

        // Calculate hash for file selected when ok clicked
        if (ImGuiFileDialog::Instance()->Display("ChooseHashFile")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                filePath = ImGuiFileDialog::Instance()->GetFilePathName();
                hashThread = std::async(std::launch::async, [&]() {
                    return calculateHashes(filePath, hashesToCalculate, hashThreadShouldCancel);
                });
                calculatedHashes = {};
                isCalculating = true;
            }
            ImGuiFileDialog::Instance()->Close();
        }

        ImGui::Text("File: %s", filePath.c_str());
        ImGui::Spacing();
        if (errorMessage == "") 
        {
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(7, 7));

            if (ImGui::BeginTable("HashTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_PadOuterX))
            {
                ImGui::TableSetupColumn("Algorithm", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("Hash", ImGuiTableColumnFlags_WidthStretch);

                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, ImGui::GetStyle().Colors[ImGuiCol_TitleBgActive]);
                ImGui::TableHeadersRow();
                ImGui::PopStyleColor();

                if (isCalculating)
                {
                    for (const auto& algorithm : hashesToCalculate)
                    {
                        ImGui::TableNextRow();
                        
                        // Algorithm column
                        ImGui::TableNextColumn();

                        // Center the text vertically due to copy button
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeight() / 4));
                        ImGui::Text(displayNames.at(algorithm).c_str());

                        // Hash column
                        ImGui::TableNextColumn();
                        ImGui::BeginDisabled();
                        ImGui::Button(std::format("Copy##{}", displayNames.at(algorithm)).c_str());
                        ImGui::EndDisabled();
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Be patient! This hash is still calculating");
                        }
                        ImGui::SameLine();
                        ImGui::Text("Calculating...");
                    }
                }
                else
                {
                    for (const auto& [algorithm, hash] : calculatedHashes)
                    {
                        ImGui::TableNextRow();
                        
                        // Algorithm column
                        ImGui::TableNextColumn();

                        // Center the text vertically due to copy button
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (ImGui::GetTextLineHeight() / 4));
                        ImGui::Text(displayNames.at(algorithm).c_str());

                        // Hash column
                        ImGui::TableNextColumn();
                        if (ImGui::Button(std::format("Copy##{}", displayNames.at(algorithm)).c_str()))
                        {
                            ImGui::SetClipboardText(hash.c_str());
                        }
                        if (hash.contains("Err-crypt code: "))
                        {
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("Click to copy error");
                            }
                        }
                        else
                        {
                            if (ImGui::IsItemHovered())
                            {
                                ImGui::SetTooltip("Click to copy hash");
                            }
                        }
                        ImGui::SameLine();
                        ImGui::PushFont(cascadia);
                        ImGui::Text(hash.c_str());
                        ImGui::PopFont();
                    }
                }
                ImGui::EndTable();
            }

            ImGui::PopStyleVar();

            ImGui::Spacing();

            static std::string message = "No hash to check";
            static ImVec4 color = ImVec4(244 * (1.0f / 255.0f), 105 * (1.0f / 255.0f), 105 * (1.0f / 255.0f), 255); // Tailwind Red 400

            // Reset on file change
            if (isCalculating)
            {
                message = "No hash to check";
                color = ImVec4(244 * (1.0f / 255.0f), 105 * (1.0f / 255.0f), 105 * (1.0f / 255.0f), 255); // Tailwind Red 400
            }

            std::vector<char> inputBuffer(129);
            if (isCalculating) 
            {
                ImGui::BeginDisabled();
            }
            ImGui::InputText("Check Hash", inputBuffer.data(), inputBuffer.size());
            if (isCalculating) 
            {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                bool found = false;

                for (const auto& [algorithm, hash] : calculatedHashes)
                {
                    if (hash.contains("Err-crypt code: "))
                    {
                        continue;
                    }
                    // Compare case-insensitively
                    if (std::equal(
                        inputBuffer.data(), 
                        inputBuffer.data() + strlen(inputBuffer.data()), 
                        hash.begin(), 
                        hash.end(), 
                        [](char a, char b) { 
                            return std::tolower(a) == std::tolower(b); 
                        }
                    ))
                    {
                        message = std::format("Match found for algorithm: {}", displayNames.at(algorithm));
                        color = ImVec4(32 * (1.0f / 255.0f), 187 * (1.0f / 255.0f), 126 * (1.0f / 255.0f), 255); // Tailwind Emerald 500
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    message = "No match found.";
                    color = ImVec4(244 * (1.0f / 255.0f), 105 * (1.0f / 255.0f), 105 * (1.0f / 255.0f), 255);  // Tailwind Red 400
                }
            }

            ImGui::TextColored(color, message.c_str());
        }
        else
        {
            ImGui::Text("Error: %s", errorMessage.c_str());
        }
        ImGui::End();

        // Rendering
        ImGui::Render();
        ImDrawData* main_draw_data = ImGui::GetDrawData();
        const bool main_is_minimized = (main_draw_data->DisplaySize.x <= 0.0f || main_draw_data->DisplaySize.y <= 0.0f);
        wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
        wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
        wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
        wd->ClearValue.color.float32[3] = clear_color.w;
        if (!main_is_minimized)
        {
            FrameRender(wd, main_draw_data);
        }

        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        // Present Main Platform Window
        if (!main_is_minimized)
        {
            FramePresent(wd);
        }
    }

    // Cleanup
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();

    // End hash thread if it is still running
    if (isCalculating)
    {
        hashThreadShouldCancel.store(true);
    }

    return 0;
}