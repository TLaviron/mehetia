option(GLM_TEST_ENABLE "Build unit tests" OFF)

add_subdirectory(glm)

option(GLFW_BUILD_EXAMPLES "Build the GLFW example programs" OFF)
option(GLFW_BUILD_TESTS "Build the GLFW test programs" OFF)
option(GLFW_INSTALL "Generate installation target" OFF)

add_subdirectory(glfw)
