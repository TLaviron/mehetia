
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

#include "mainApplication.h"

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
    Pimpl() : m_pWindow(nullptr) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // don't support resize for now
        m_pWindow = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

        m_pInstanceInfo = std::make_unique<InstanceInfo>();

        std::cout << *m_pInstanceInfo;
    }

    void run(){
        while(!glfwWindowShouldClose(m_pWindow)) {
            glfwPollEvents();
        }
    }

    void cleanup(){
        glfwDestroyWindow(m_pWindow);

        glfwTerminate();
    }

private:
    GLFWwindow* m_pWindow;
    std::unique_ptr<InstanceInfo> m_pInstanceInfo;

    void createInstance();
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


