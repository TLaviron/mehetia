#include "vulkan_mehetia/utilities.h"
#include <string>


namespace vulkan_mehetia {

VkResult resultCheck(VkResult _result, std::string_view _sourceFile, int32_t _line) {
    std::string errorMessage(_sourceFile);
    errorMessage += ":" + std::to_string(_line) + " ";

    switch (_result) {
    case VK_SUCCESS:
        errorMessage += "Success";
        break;
    case VK_NOT_READY:
        errorMessage += "Not ready";
        break;
    case VK_TIMEOUT:
        errorMessage += "Timeout";
        break;
    case VK_EVENT_SET:
        errorMessage += "Event set";
        break;
    case VK_EVENT_RESET:
        errorMessage += "Event reset";
        break;
    case VK_INCOMPLETE:
        errorMessage += "Incomplete";
        break;
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        errorMessage += "Out of host memory";
        break;
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        errorMessage += "Out of device memory";
        break;
    case VK_ERROR_INITIALIZATION_FAILED:
        errorMessage += "Initialization failed";
        break;
    case VK_ERROR_DEVICE_LOST:
        errorMessage += "Device lost";
        break;
    case VK_ERROR_MEMORY_MAP_FAILED:
        errorMessage += "Memory map failed";
        break;
    case VK_ERROR_LAYER_NOT_PRESENT:
        errorMessage += "Layer not present";
        break;
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        errorMessage += "Extension not present";
        break;
    case VK_ERROR_FEATURE_NOT_PRESENT:
        errorMessage += "Feature not present";
        break;
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        errorMessage += "Not ready";
        break;
    case VK_ERROR_TOO_MANY_OBJECTS:
        errorMessage += "Too many objects";
        break;
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        errorMessage += "Format not supported";
        break;
    case VK_ERROR_FRAGMENTED_POOL:
        errorMessage += "Fragmented pool";
        break;
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        errorMessage += "Out of pool memory";
        break;
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        errorMessage += "Invalid external handle";
        break;
    default:
        errorMessage += "Undocumented error (" + std::to_string(_result) + ")";
        break;
    }

    if (_result < 0) {
        throw VulkanError(errorMessage);
    }

    return _result;
}

}  // namespace vulkan_mehetia
