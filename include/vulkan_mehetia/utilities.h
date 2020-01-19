#ifndef VULKAN_MEHETIA_UTILITIES_H
#define VULKAN_MEHETIA_UTILITIES_H

#include <stdexcept>
#include <string_view>
#include <vulkan/vulkan.h>

namespace vulkan_mehetia {

VkResult resultCheck(VkResult _result, std::string_view _sourceFile, int32_t _line);

#define RESULT_CHECK(expr)  ::vulkan_mehetia::resultCheck((expr), __FILE__, __LINE__)


class VulkanError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


}  // namespace vulkan_mehetia

#endif // VULKAN_MEHETIA_UTILITIES_H
