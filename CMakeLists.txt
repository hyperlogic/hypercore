cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(hypercore LANGUAGES CXX)

# Necessary to fetch dependencies.
include(FetchContent)

# stfu about DOWNLOAD_EXTRACT_TIMESTAMP
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# opengl
find_package(OpenGL REQUIRED)

# sdl2
# Configure SDL2 build options (optional, set before FetchContent_Declare)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

# Use source relase of sdl2.
FetchContent_Declare(
    SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.28.5/SDL2-2.28.5.tar.gz
)

# Make SDL2 available
FetchContent_MakeAvailable(SDL2)

# glew
# Configure GLEW build options (optional, set before FetchContent_Declare)
set(BUILD_UTILS OFF CACHE BOOL "" FORCE)
set(GLEW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLEW_REGAL OFF CACHE BOOL "" FORCE)
set(GLEW_OSMESA OFF CACHE BOOL "" FORCE)
set(GLEW_EGL OFF CACHE BOOL "" FORCE)
set(glew-cmake_BUILD_SHARED OFF CACHE BOOL "" FORCE)  # Don't build shared lib
set(glew-cmake_BUILD_STATIC ON CACHE BOOL "" FORCE)   # Build static lib

# Use pre-built release archive
FetchContent_Declare(
    glew
    URL https://github.com/nigels-com/glew/releases/download/glew-2.2.0/glew-2.2.0.tgz
    SOURCE_SUBDIR build/cmake
)

# Make GLEW available
FetchContent_MakeAvailable(glew)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_LIBRARY ON CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# glm
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

# stb_image
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

FetchContent_MakeAvailable(stb)

# nlohmann-json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

# Dear ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.2  # Using a stable release
)
FetchContent_MakeAvailable(imgui)

# assimp
# Configure Assimp build options (optional, set before FetchContent_Declare)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)

# Declare Assimp dependency
FetchContent_Declare(
   assimp
   GIT_REPOSITORY https://github.com/assimp/assimp.git
   GIT_TAG v6.0.2  # Use a specific version tag for reproducibility
   PATCH_COMMAND patch -p1 --forward --reject-file=- < ${CMAKE_CURRENT_SOURCE_DIR}/assimp-macos.patch || true
)
FetchContent_MakeAvailable(assimp)

# Create ImGui library target
add_library(imgui_lib STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_lib PUBLIC
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
)

# Create hypercore engine library
add_library(hypercore STATIC
    src/anim.cpp
    src/appbase.cpp
    src/binaryattribute.cpp
    src/bonemesh.cpp
    src/checkerfloor.cpp
    src/debugrenderer.cpp
    src/flycam.cpp
    src/framebuffer.cpp
    src/image.cpp
    src/import.cpp
    src/inputbuddy.cpp
    src/log.cpp
    src/node.cpp
    src/material.cpp
    src/mesh.cpp
    src/program.cpp
    src/sdl_main.cpp
    src/texture.cpp
    src/util.cpp
    src/vertexbuffer.cpp
    src/textrenderer.cpp
)

target_include_directories(hypercore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${glew_SOURCE_DIR}/include
    ${stb_SOURCE_DIR}
)

target_compile_features(hypercore PUBLIC cxx_std_17)

target_link_libraries(hypercore PUBLIC
    ${OPENGL_LIBRARIES}
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    glew_s
    glm
    nlohmann_json::nlohmann_json
    assimp
    imgui_lib
)
