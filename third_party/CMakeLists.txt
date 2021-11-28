include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY  https://github.com/g-truc/glm
    GIT_TAG         bf71a834948186f4097caa076cd2663c69a10e1e # release 0.9.9.8
    )

FetchContent_Declare(
    glfw
    GIT_REPOSITORY  https://github.com/glfw/glfw
    GIT_TAG         0a49ef0a00baa3ab520ddc452f0e3b1e099c5589 # release 3.3.2
    )

FetchContent_Declare(
    volk
    GIT_REPOSITORY  https://github.com/zeux/volk
    GIT_TAG         760a782f295a66de7391d6ed573d65e3fb1c8450 # release 1.2.190
    )

fetchContent_MakeAvailable(glm glfw volk)
