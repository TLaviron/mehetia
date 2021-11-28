
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


class MainApplication::Pimpl{
public:
    Pimpl() : m_pWindow(nullptr), m_instance{} {
        glfwInit();
        RESULT_CHECK(volkInitialize());

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // don't support resize for now
        m_pWindow = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

        m_pInstanceInfo = std::make_unique<InstanceInfo>();

        std::cout << *m_pInstanceInfo;
        createInstance();

        createDevice();
    }

    void run(){
        while(!glfwWindowShouldClose(m_pWindow)) {
            glfwPollEvents();
        }
    }

    void cleanup(){
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
    GLFWwindow* m_pWindow;
    std::unique_ptr<InstanceInfo> m_pInstanceInfo;
    VkInstance m_instance;
    VkDevice m_device;
    VkDebugUtilsMessengerEXT m_debugMessenger;


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

        uint32_t graphicsQueueIndex = 0;
        for (uint32_t i = 0; i < queueFamilyProperties[selectedDevice].size(); ++i)
        {
            if (queueFamilyProperties[selectedDevice][i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsQueueIndex = i;
                break;
            }
        }

        float queuePriority = 0.0f;

        std::array<VkDeviceQueueCreateInfo, 1> queueInfo;
        queueInfo[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo[0].pNext = nullptr;
        queueInfo[0].flags = 0;
        queueInfo[0].queueFamilyIndex = graphicsQueueIndex;
        queueInfo[0].queueCount = 1;
        queueInfo[0].pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = queueInfo.data();
        deviceInfo.queueCreateInfoCount = queueInfo.size();

        RESULT_CHECK(vkCreateDevice(physicalDevices[selectedDevice], &deviceInfo, nullptr, &m_device));
        volkLoadDevice(m_device);
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


