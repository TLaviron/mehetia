
#define GLFW_INCLUDE_VULKAN
#define VK_NO_PROTOTYPES
#include <GLFW/glfw3.h>

#include <volk.h>

#include <iostream>
#include <vector>
#include <cstring>
#include <algorithm>
#include <cassert>

#include "mainApplication.h"
#include "vulkan_mehetia/utilities.h"

constexpr uint32_t vkVersionMajor(uint32_t _version) noexcept {
    return _version >> 22;
}
constexpr uint32_t vkVersionMinor(uint32_t _version) noexcept {
    return (_version >> 12) & 0x3FF;
}
constexpr uint32_t vkVersionPatch(uint32_t _version) noexcept {
    return _version & 0xFFF;
}

std::string vkVersionString(uint32_t _version) {
    return std::to_string(vkVersionMajor(_version)) + "." + std::to_string(vkVersionMinor(_version)) +
            "." + std::to_string(vkVersionPatch(_version));
}

class InstanceInfo
{
    uint32_t m_instanceVersion;
    std::vector<VkLayerProperties> m_layerProperties;
    std::vector<std::vector<VkExtensionProperties>> m_perLayerExtensionProperties;

public:
    InstanceInfo(){
        vkEnumerateInstanceVersion(&m_instanceVersion);

        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        m_layerProperties.resize(layerCount, VkLayerProperties{});
        vkEnumerateInstanceLayerProperties(&layerCount, m_layerProperties.data());

        m_perLayerExtensionProperties.resize(layerCount + 1);

        // query non-layer specific extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        auto& generalExtensions = m_perLayerExtensionProperties[0];
        generalExtensions.resize(extensionCount, VkExtensionProperties{});
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, generalExtensions.data());

        // query layer-specific extensions
        for (std::ptrdiff_t i = 0; i < layerCount; ++i) {
            vkEnumerateInstanceExtensionProperties(m_layerProperties[i].layerName, &extensionCount, nullptr);
            auto& layerExtensions = m_perLayerExtensionProperties[i + 1];

            layerExtensions.resize(extensionCount, VkExtensionProperties{});
            vkEnumerateInstanceExtensionProperties(m_layerProperties[i].layerName, &extensionCount,
                    layerExtensions.data());
        }


    }

    bool hasLayer(const char* layerName){
        return std::find_if(m_layerProperties.begin(), m_layerProperties.end(),
                            [&](const VkLayerProperties& layer){ return std::strcmp(layerName, layer.layerName) == 0;}) != m_layerProperties.end();
    }

    bool hasExtension(const char* extensionName) {
        for (const auto& extensionProperties : m_perLayerExtensionProperties){
            if (std::find_if(extensionProperties.begin(), extensionProperties.end(),
                    [&](const VkExtensionProperties& extension){
                        return std::strcmp(extensionName, extension.extensionName) == 0;})
                    != extensionProperties.end())
            {
                return true;
            }
        }
        return false;
    }

    std::ostream& toStream(std::ostream& _os) const {
        _os << "Vulkan instance information:\n";

        _os << "\tVersion: " << vkVersionString(m_instanceVersion) <<"\n";

        if (!m_layerProperties.empty()) {
            _os << "\tAvailable layers: " << m_layerProperties.size() << "\n";
            for (const auto& layerProperties : m_layerProperties){
                _os << "\tLayer "  << layerProperties.layerName << ":\n";
                _os << "\t\tApi version: " << vkVersionString(layerProperties.specVersion) << "\n";
                _os << "\t\tLayer version: " << layerProperties.implementationVersion << "\n";
                _os << "\t\t" << layerProperties.description << "\n";
            }
        }

        if (!m_perLayerExtensionProperties[0].empty()) {
            _os << "\tAvailable extensions: " << m_perLayerExtensionProperties[0].size() << "\n";
            for (const auto& extensionProperties : m_perLayerExtensionProperties[0]) {
                _os << "\t\t" << extensionProperties.extensionName << " (ver. " <<
                        extensionProperties.specVersion << ")\n";
            }
        }

        for (std::ptrdiff_t i = 0; i < m_layerProperties.size(); ++i) {
            auto & layerExtensions = m_perLayerExtensionProperties[i+1];
            if (!layerExtensions.empty()) {
                _os << "\tLayer " << m_layerProperties[i].layerName << " available extensions: " <<
                        layerExtensions.size() << "\n";
                for (const auto& extensionProperties : layerExtensions) {
                    _os << "\t\t" << extensionProperties.extensionName << " (ver. " <<
                            extensionProperties.specVersion << ")\n";
                }
            }
        }

        return _os;
    }
};

std::ostream& operator<<(std::ostream& _os, const InstanceInfo& _instanceInfo){

    return _instanceInfo.toStream(_os);
}

struct PerFrameResources
{
    VkCommandPool m_pool;
    VkCommandBuffer m_commandBuffer;
    VkFence m_fence;
    VkSemaphore m_acquireSemaphore;
    VkSemaphore m_submitSemaphore;
};


class MainApplication::Pimpl{
public:
    Pimpl() {
        glfwInit();
        RESULT_CHECK(volkInitialize());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // don't support resize for now
        m_pWindow = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

        m_pInstanceInfo = std::make_unique<InstanceInfo>();

        std::cout << *m_pInstanceInfo;
        createInstance();

        createDevice();

        setupPerFrameResources();

        createSwapchain(800, 600);
    }

    void run(){
        while(!glfwWindowShouldClose(m_pWindow)) {
            glfwPollEvents();

            VkResult res = vkGetFenceStatus(m_device, m_perFrame[m_frameID].m_fence);
            if (res == VK_NOT_READY) {
                RESULT_CHECK(vkWaitForFences(m_device, 1, &m_perFrame[m_frameID].m_fence, VK_TRUE, UINT64_MAX));
            } else {
                RESULT_CHECK(res);
            }

            RESULT_CHECK(vkResetFences(m_device, 1, &m_perFrame[m_frameID].m_fence));

            RESULT_CHECK(vkResetCommandPool(m_device, m_perFrame[m_frameID].m_pool, 0));

            uint32_t imageIndex = 0;
            RESULT_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX,
                m_perFrame[m_frameID].m_acquireSemaphore, VK_NULL_HANDLE, &imageIndex));

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            RESULT_CHECK(vkBeginCommandBuffer(m_perFrame[m_frameID].m_commandBuffer, &beginInfo));


            VkImageMemoryBarrier imageBarrier{};
            imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageBarrier.srcAccessMask = 0;
            imageBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imageBarrier.image = m_swapchainImages[imageIndex];
            imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBarrier.subresourceRange.baseArrayLayer = 0;
            imageBarrier.subresourceRange.layerCount = 1;
            imageBarrier.subresourceRange.baseMipLevel = 0;
            imageBarrier.subresourceRange.levelCount = 1;


            vkCmdPipelineBarrier(m_perFrame[m_frameID].m_commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageBarrier);

            RESULT_CHECK(vkEndCommandBuffer(m_perFrame[m_frameID].m_commandBuffer));

            VkSubmitInfo submitInfo{};
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &m_perFrame[m_frameID].m_acquireSemaphore;

            VkPipelineStageFlags acquireWaitMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            submitInfo.pWaitDstStageMask = &acquireWaitMask;
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &m_perFrame[m_frameID].m_commandBuffer;
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &m_perFrame[m_frameID].m_submitSemaphore;
            RESULT_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_perFrame[m_frameID].m_fence));

            VkPresentInfoKHR presentInfo{};
            presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores = &m_perFrame[m_frameID].m_submitSemaphore;
            presentInfo.swapchainCount = 1;
            presentInfo.pSwapchains = &m_swapchain;
            presentInfo.pImageIndices = &imageIndex;
            RESULT_CHECK(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

            m_frameID = (m_frameID + 1) % m_perFrame.size();

        }
    }

    void cleanup(){
        vkQueueWaitIdle(m_graphicsQueue);

        destroySwapchain();

        tearDownPerFrameResources();

        vkDestroyDevice(m_device, nullptr);

        if (m_debugMessenger != VK_NULL_HANDLE)
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);

        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_pWindow);

        glfwTerminate();
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT){
            std::cerr << "Vulkan error: " << pCallbackData->pMessage << std::endl;
        } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT){
            std::cerr << "Vulkan warning: " << pCallbackData->pMessage << std::endl;
        } else {
            std::cout << "Vulkan info: " << pCallbackData->pMessage << std::endl;
        }

        return VK_FALSE;
    }

private:
    GLFWwindow* m_pWindow = nullptr;
    std::unique_ptr<InstanceInfo> m_pInstanceInfo;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueIndex = 0;
    std::array<PerFrameResources, 2> m_perFrame;
    uint32_t m_frameID = 0;

    void createInstance(){
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Mehetia";
        appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.pApplicationInfo = &appInfo;

        uint32_t glfwWSIExtensionCount{};
        const char** glfwWSIExtensions = glfwGetRequiredInstanceExtensions(&glfwWSIExtensionCount);

        std::vector<const char*> extensions(glfwWSIExtensions, glfwWSIExtensions + glfwWSIExtensionCount);

        bool enableDebugUtils = false;
#ifndef NDEBUG
        const char* validationLayer = "VK_LAYER_KHRONOS_validation";
        if (m_pInstanceInfo->hasLayer(validationLayer)) {
            instanceInfo.enabledLayerCount = 1;
            instanceInfo.ppEnabledLayerNames = &validationLayer;
            if (m_pInstanceInfo->hasExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)){
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                enableDebugUtils = true;
            }
            else
                std::cout << "Debug utils extension not present\n";
        } else {
            std::cout << "Validation layer not present\n";
        }

#endif

        instanceInfo.enabledExtensionCount = extensions.size();
        instanceInfo.ppEnabledExtensionNames = extensions.data();

        RESULT_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_instance));
        volkLoadInstance(m_instance);

        if (enableDebugUtils)
            setupDebugMessenger();
    }

    void createDevice(){
        uint32_t deviceCount = 0;
        RESULT_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        RESULT_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data()));

        std::vector<VkPhysicalDeviceProperties> properties(deviceCount);
        std::vector<std::vector<VkQueueFamilyProperties>> queueFamilyProperties(deviceCount);
        for (uint32_t i = 0; i < deviceCount; ++i){
            vkGetPhysicalDeviceProperties(physicalDevices[i], &properties[i]);
            uint32_t queueFamilyCount = 0;

            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount, nullptr);
            queueFamilyProperties[i].resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevices[i], &queueFamilyCount,
                queueFamilyProperties[i].data());
        }

        auto isDeviceBetter = [&](uint32_t test, uint32_t ref){
            if (properties[test].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                    && properties[ref].deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                return true;
            else if (properties[test].deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
                && properties[ref].deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                && properties[ref].deviceType != VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
                return true;
            else if (properties[test].deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
                && properties[ref].deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                && properties[ref].deviceType != VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU
                && properties[ref].deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                return true;
            else if (properties[test].deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU
                && properties[ref].deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER)
                return true;

            return false;
        };

        uint32_t selectedDevice = 0;
        for (uint32_t testDevice = 1; testDevice < deviceCount; ++testDevice){
            if (isDeviceBetter(testDevice, selectedDevice))
                selectedDevice = testDevice;
        }

        assert(isDeviceSuitable(properties[selectedDevice]));
        m_physicalDevice = physicalDevices[selectedDevice];

        for (uint32_t i = 0; i < queueFamilyProperties[selectedDevice].size(); ++i)
        {
            if (queueFamilyProperties[selectedDevice][i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                m_graphicsQueueIndex = i;
                break;
            }
        }

        float queuePriority = 0.0f;

        std::array<VkDeviceQueueCreateInfo, 1> queueInfo;
        queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[0].pNext = nullptr;
        queueInfo[0].flags = 0;
        queueInfo[0].queueFamilyIndex = m_graphicsQueueIndex;
        queueInfo[0].queueCount = 1;
        queueInfo[0].pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = queueInfo.data();
        deviceInfo.queueCreateInfoCount = queueInfo.size();

        std::vector<const char*> extensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        deviceInfo.enabledExtensionCount = extensions.size();
        deviceInfo.ppEnabledExtensionNames = extensions.data();

        RESULT_CHECK(vkCreateDevice(physicalDevices[selectedDevice], &deviceInfo, nullptr, &m_device));
        volkLoadDevice(m_device);

        vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_graphicsQueue);
    }

    void setupDebugMessenger() {
        VkDebugUtilsMessengerCreateInfoEXT debugUtilsInfo{};
        debugUtilsInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
        debugUtilsInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugUtilsInfo.pfnUserCallback = &debugCallback;

        vkCreateDebugUtilsMessengerEXT(m_instance, &debugUtilsInfo, nullptr, &m_debugMessenger);

        VkDebugUtilsMessengerCallbackDataEXT callbackData{};
        callbackData.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
        callbackData.pMessage = "Debug callback initialized";
        vkSubmitDebugUtilsMessageEXT(m_instance, VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &callbackData);
    }

    bool isDeviceSuitable(const VkPhysicalDeviceProperties& properties)
    {
        return true;
    }

    void setupPerFrameResources(){
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_graphicsQueueIndex;

        VkCommandBufferAllocateInfo cmdBufferInfo{};
        cmdBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufferInfo.commandBufferCount = 1;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for (PerFrameResources &resources : m_perFrame){
            RESULT_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &resources.m_pool));

            cmdBufferInfo.commandPool = resources.m_pool;
            RESULT_CHECK(vkAllocateCommandBuffers(m_device, &cmdBufferInfo, &resources.m_commandBuffer));

            RESULT_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &resources.m_fence));
            RESULT_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &resources.m_acquireSemaphore));
            RESULT_CHECK(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &resources.m_submitSemaphore));
        }
    }

    void tearDownPerFrameResources(){
        for (PerFrameResources &resources : m_perFrame){
            vkDestroySemaphore(m_device, resources.m_acquireSemaphore, nullptr);
            vkDestroySemaphore(m_device, resources.m_submitSemaphore, nullptr);
            vkDestroyFence(m_device, resources.m_fence, nullptr);

            vkDestroyCommandPool(m_device, resources.m_pool, nullptr);
        }
    }

    void createSwapchain(uint32_t width, uint32_t height){
        RESULT_CHECK(glfwCreateWindowSurface(m_instance, m_pWindow, nullptr, &m_windowSurface));

        VkBool32 bPresentSupported = VK_FALSE;
        RESULT_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueIndex,
            m_windowSurface, &bPresentSupported));

        assert(bPresentSupported);

        VkSurfaceCapabilitiesKHR surfaceCapabilities{};

        RESULT_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_windowSurface,
            &surfaceCapabilities));

        assert((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0);
        assert((surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) != 0);

        VkSwapchainCreateInfoKHR swapchainInfo{};
        swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainInfo.surface = m_windowSurface;
        swapchainInfo.minImageCount = 3;
        swapchainInfo.imageFormat = VK_FORMAT_B8G8R8A8_SRGB;
        swapchainInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        swapchainInfo.imageExtent.width = width;
        swapchainInfo.imageExtent.height = height;
        swapchainInfo.imageArrayLayers = 1;
        swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 1;
        swapchainInfo.pQueueFamilyIndices = &m_graphicsQueueIndex;
        swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;


        RESULT_CHECK(vkCreateSwapchainKHR(m_device, &swapchainInfo, nullptr, &m_swapchain));

        uint32_t imageCount = 0;
        RESULT_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr));

        assert(imageCount >= 2);
        m_swapchainImages.resize(imageCount);
        RESULT_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()));
    }

    void destroySwapchain(){
vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

        vkDestroySurfaceKHR(m_instance, m_windowSurface, nullptr);
    }

};


MainApplication::MainApplication() : m_pimpl(std::make_unique<Pimpl>()) {
}

MainApplication::~MainApplication() = default;

void MainApplication::run(){
    m_pimpl->run();
}

void MainApplication::cleanup(){
    m_pimpl->cleanup();
}


