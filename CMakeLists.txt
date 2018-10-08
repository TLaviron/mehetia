cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

set(MEHETIA_VERSION "0.0.1")
project(mehetia VERSION ${MEHETIA_VERSION})

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
if (IS_DIRECTORY $ENV{VULKAN_SDK})
    find_package(Vulkan)
    if (NOT ${VULKAN_FOUND})
        set(ENV{VULKAN_SDK} "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lunarG_vulkan_SDK_1.1.82.1/x86_64")
        find_package(Vulkan REQUIRED)
    endif()
else()
    set(ENV{VULKAN_SDK} "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lunarG_vulkan_SDK_1.1.82.1/x86_64")
    find_package(Vulkan REQUIRED)
endif()

add_subdirectory(third_party)

set(LIBRARIES ${Vulkan_LIBRARIES} glfw glm)

set(SOURCES src/mainTest.cpp)

add_executable(demo ${SOURCES})
target_link_libraries(demo PRIVATE ${LIBRARIES})

set_property(TARGET demo PROPERTY CXX_STANDARD 14)
set_property(TARGET demo PROPERTY CXX_STANDARD_REQUIRED YES)

