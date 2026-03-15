# 1 Set verison and project name
cmake_minimum_required(VERSION 3.22)
project(Flare)
set(CMAKE_CXX_STANDARD 17)

# 2 Copy assets
file(GLOB ASSETS 
"./assets" 
"./resource/shaders" 
"./resource/frame_graph"
"./third_party/assimp/bin/assimp/assimp-vc143-mtd.dll")
file(COPY ${ASSETS} DESTINATION ${CMAKE_BINARY_DIR})

# 3 Third party
# 3.1 Include third party .h
include_directories(
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/assimp/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glm/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ktx/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nvtx/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/include
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan/Include
)

# 3.2 Link third party .lib
link_directories(
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/assimp/lib
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw/lib
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ktx/lib
SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/third_party/vulkan/Lib
)

# 4 Add subdirectory .cpp
add_subdirectory(application)
add_subdirectory(function)
add_subdirectory(platform)
add_subdirectory(external)

# 5 Main directory
# 5.1 Collect all .cpp and .c  to variable
aux_source_directory(. DIRSRCS)

# 5.2 Compile variable to .exe
add_executable(flare ${DIRSRCS})

# 6 Link subdirectory .cpp and third party .lib
target_link_libraries(flare
    applicationLib 
    functionLib 
    platformLib 
    externalLib				  
    vulkan-1.lib                  
    glfw3.lib                                       
    zlibstaticd.lib   
    assimp-vc143-mtd.lib
    ktx.lib
)

