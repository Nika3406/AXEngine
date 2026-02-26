cmake_minimum_required(VERSION 3.16)
project(AXEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------
# Options
# ---------------------------
option(AX_UPDATE_SUBMODULES "Update git submodules during configure" ON)
option(AX_BUILD_SANDBOX     "Build the sandbox/game executable" ON)
option(AX_UNITY_BUILD       "Enable unity builds (faster builds)" OFF)

option(AX_ENABLE_WARNINGS   "Enable extra warnings" ON)
option(AX_ENABLE_ASAN       "Enable Address/UB sanitizers" OFF)

# ---------------------------
# Submodules (optional update)
# ---------------------------
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
    if(AX_UPDATE_SUBMODULES)
        message(STATUS "Updating git submodules...")
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            RESULT_VARIABLE GIT_SUBMOD_RESULT
        )
        if(NOT GIT_SUBMOD_RESULT EQUAL 0)
        message(FATAL_ERROR "git submodule update --init --recursive failed (${GIT_SUBMOD_RESULT})")
        endif()
    endif()
endif()

# ---------------------------
# Paths
# ---------------------------
set(SUBMODS_DIR "${PROJECT_SOURCE_DIR}/submods")
set(BX_DIR      "${SUBMODS_DIR}/bx")
set(BIMG_DIR    "${SUBMODS_DIR}/bimg")
set(BGFX_DIR    "${SUBMODS_DIR}/bgfx")
set(ASSIMP_DIR  "${SUBMODS_DIR}/assimp")
set(ENTT_DIR    "${SUBMODS_DIR}/entt")

function(require_submodule name path)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR
            "Missing submodule '${name}' at: ${path}\n"
            "Run: git submodule update --init --recursive"
        )
    endif()
endfunction()

require_submodule("bx"     "${BX_DIR}")
require_submodule("bimg"   "${BIMG_DIR}")
require_submodule("bgfx"   "${BGFX_DIR}")
require_submodule("assimp" "${ASSIMP_DIR}")
require_submodule("entt"   "${ENTT_DIR}")

# ---------------------------
# Platform deps (Linux)
# ---------------------------
if(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(X11 REQUIRED)
else()
    message(FATAL_ERROR "This CMakeLists currently targets Linux (UNIX non-Apple).")
endif()

# ---------------------------
# EnTT (header-only)
# ---------------------------
add_library(entt INTERFACE)
add_library(EnTT::EnTT ALIAS entt)
target_include_directories(entt INTERFACE "${ENTT_DIR}/single_include")

# ---------------------------
# Assimp
# ---------------------------
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_WERROR OFF CACHE BOOL "" FORCE)

add_subdirectory("${ASSIMP_DIR}" "${CMAKE_BINARY_DIR}/_deps_assimp" EXCLUDE_FROM_ALL)

if(TARGET assimp::assimp)
    set(ASSIMP_TARGET assimp::assimp)
elseif(TARGET assimp)
    set(ASSIMP_TARGET assimp)
else()
    message(FATAL_ERROR "Assimp target not found after add_subdirectory().")
endif()

# ---------------------------
# bgfx/bimg/bx via bgfx.cmake
# ---------------------------
include(FetchContent)

option(USE_LOCAL_BGFX_CMAKE "Use submods/bgfx.cmake instead of downloading" OFF)

# Force bgfx.cmake to use your local submodules for bgfx/bimg/bx
set(BGFX_DIR "${BGFX_DIR}" CACHE PATH "" FORCE)
set(BIMG_DIR "${BIMG_DIR}" CACHE PATH "" FORCE)
set(BX_DIR   "${BX_DIR}"   CACHE PATH "" FORCE)

set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BGFX_BUILD_TOOLS    ON  CACHE BOOL "" FORCE) # shaderc
set(BGFX_INSTALL        OFF CACHE BOOL "" FORCE)

if(USE_LOCAL_BGFX_CMAKE)
    if(NOT EXISTS "${SUBMODS_DIR}/bgfx.cmake/CMakeLists.txt")
        message(FATAL_ERROR
        "USE_LOCAL_BGFX_CMAKE=ON but submods/bgfx.cmake is missing.\n"
        "Either add bgfx.cmake as a submodule at submods/bgfx.cmake, or set USE_LOCAL_BGFX_CMAKE=OFF."
        )
    endif()
    add_subdirectory("${SUBMODS_DIR}/bgfx.cmake" "${CMAKE_BINARY_DIR}/_deps_bgfx_cmake")
    else()
    FetchContent_Declare(
        bgfx_cmake
        GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
        GIT_TAG        master
    )
    FetchContent_MakeAvailable(bgfx_cmake)
endif()

if(NOT TARGET bgfx OR NOT TARGET bimg OR NOT TARGET bx)
    message(FATAL_ERROR "Expected bgfx/bimg/bx targets not created. bgfx.cmake mismatch.")
endif()

# ---------------------------
# Linux: shaderc HLSL workaround
# ---------------------------
if(UNIX AND NOT APPLE AND TARGET shaderc)
    get_target_property(_shaderc_sources shaderc SOURCES)
    if(_shaderc_sources)
        list(FILTER _shaderc_sources EXCLUDE REGEX "shaderc_hlsl\\.cpp$")
        set_property(TARGET shaderc PROPERTY SOURCES "${_shaderc_sources}")
    endif()

    set(SHADERC_HLSL_STUB "${CMAKE_BINARY_DIR}/shaderc_hlsl_stub.cpp")
    file(WRITE "${SHADERC_HLSL_STUB}"
    "#include <string>\n"
    "namespace bx { struct WriterI; }\n"
    "namespace bgfx {\n"
    "  struct Options;\n"
    "  bool compileHLSLShader(const Options&, unsigned int, const std::string&, bx::WriterI*, bx::WriterI*) { return false; }\n"
    "}\n"
    )
    target_sources(shaderc PRIVATE "${SHADERC_HLSL_STUB}")
endif()

# ---------------------------
# AXEngine library (engine backbone)
# ---------------------------
set(AXENGINE_SOURCES
    src/core/App.cpp
    src/core/Time.cpp
    src/ecs/Components.cpp
    src/ecs/Systems.cpp
    src/assets/Mesh.cpp
    src/assets/AssetSystem.cpp
    src/render/Renderer.cpp
    src/render/BgfxHelpers.cpp
)

add_library(AXEngine STATIC ${AXENGINE_SOURCES})
add_library(AX::Engine ALIAS AXEngine)

target_include_directories(AXEngine
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${BGFX_DIR}/include"
        "${BX_DIR}/include"
        "${BIMG_DIR}/include"
    PRIVATE
        "${BGFX_DIR}/3rdparty"
        "${BGFX_DIR}/examples/common"
)

target_link_libraries(AXEngine
    PUBLIC
        EnTT::EnTT
    PRIVATE
        bgfx bimg bx bimg_decode
        ${ASSIMP_TARGET}
        OpenGL::GL
        X11::X11
        ${CMAKE_DL_LIBS}
        Threads::Threads
)

set_target_properties(AXEngine PROPERTIES
    UNITY_BUILD ${AX_UNITY_BUILD}
)

if(AX_ENABLE_WARNINGS)
    target_compile_options(AXEngine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AX_ENABLE_ASAN)
    target_compile_options(AXEngine PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(AXEngine PRIVATE -fsanitize=address,undefined)
endif()

# ---------------------------
# bx header requirement: BX_CONFIG_DEBUG must be defined
# (Robust for single-config generators: Makefiles/Ninja)
# ---------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(AX_BX_DEBUG 1)
else()
    set(AX_BX_DEBUG 0)
endif()

# Export to anything including engine headers
target_compile_definitions(AXEngine PUBLIC BX_CONFIG_DEBUG=${AX_BX_DEBUG})

# ---------------------------
# bgfx shader compilation
# ---------------------------
set(BGFX_SHADER_PLATFORM linux)
set(BGFX_SHADER_PROFILE spirv)
set(BGFX_SHADER_TYPE spirv)

set(SHADER_SRC_DIR "${PROJECT_SOURCE_DIR}/shaders/src")
set(SHADER_BIN_DIR "${PROJECT_SOURCE_DIR}/shaders/compiled/${BGFX_SHADER_TYPE}")
file(MAKE_DIRECTORY "${SHADER_BIN_DIR}")

if(NOT TARGET shaderc)
    message(FATAL_ERROR "shaderc target not found. BGFX_BUILD_TOOLS must be ON.")
endif()

function(bgfx_compile_shader INPUT OUTPUT TYPE)
    add_custom_command(
        OUTPUT "${OUTPUT}"
        COMMAND $<TARGET_FILE:shaderc>
            -f "${INPUT}"
            -o "${OUTPUT}"
            --type ${TYPE}
            --platform ${BGFX_SHADER_PLATFORM}
            --profile ${BGFX_SHADER_PROFILE}
            --varyingdef "${SHADER_SRC_DIR}/varying.def.sc"
            -i "${BGFX_DIR}/src"
            -i "${SHADER_SRC_DIR}"
        DEPENDS "${INPUT}" "${SHADER_SRC_DIR}/varying.def.sc"
        COMMENT "Compiling bgfx shader: ${INPUT}"
        VERBATIM
    )
endfunction()

set(SHADERS
    vs_mesh
    fs_mesh
)

set(SHADER_OUTPUTS)
foreach(SHADER ${SHADERS})
    if(SHADER MATCHES "^vs_")
        set(TYPE v)
    elseif(SHADER MATCHES "^fs_")
        set(TYPE f)
    else()
        message(FATAL_ERROR "Shader ${SHADER} must start with vs_ or fs_")
    endif()

    set(SRC "${SHADER_SRC_DIR}/${SHADER}.sc")
    set(OUT "${SHADER_BIN_DIR}/${SHADER}.bin")

    bgfx_compile_shader("${SRC}" "${OUT}" "${TYPE}")
    list(APPEND SHADER_OUTPUTS "${OUT}")
endforeach()

add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})

# ---------------------------
# Optional: copy bgfx runtime (if you still want it)
# ---------------------------
file(COPY
    "${BGFX_DIR}/examples/runtime/"
    DESTINATION "${CMAKE_BINARY_DIR}/runtime"
)

# ---------------------------
# Sandbox executable + user scripts
# ---------------------------
if(AX_BUILD_SANDBOX)

    # Engine-owned main that calls CreateGame() from scripts/
    add_executable(AXSandbox game/main.cpp)

    # Compile everything in scripts/ into a library
    file(GLOB_RECURSE AX_SCRIPT_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/scripts/*.cpp"
        "${PROJECT_SOURCE_DIR}/scripts/*.cxx"
    )

    add_library(AXScripts STATIC ${AX_SCRIPT_SOURCES})

    target_include_directories(AXScripts PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/scripts"
    )

    # Guarantee bx headers compile in scripts too (belt + suspenders)
    target_compile_definitions(AXScripts PRIVATE BX_CONFIG_DEBUG=${AX_BX_DEBUG})

    target_link_libraries(AXScripts PRIVATE AX::Engine)

    target_link_libraries(AXSandbox
        PRIVATE
        AX::Engine
        AXScripts
    )

    # Build shaders before sandbox
    add_dependencies(AXSandbox shaders)

    # Copy compiled shaders beside the executable
    add_custom_command(TARGET AXSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:AXSandbox>/shaders/compiled/${BGFX_SHADER_TYPE}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${SHADER_BIN_DIR}"
            "$<TARGET_FILE_DIR:AXSandbox>/shaders/compiled/${BGFX_SHADER_TYPE}"
        COMMENT "Copying compiled shaders to sandbox output"
        VERBATIM
    )

    # Copy user-space folders beside the executable
    add_custom_command(TARGET AXSandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:AXSandbox>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${PROJECT_SOURCE_DIR}/scripts"
            "$<TARGET_FILE_DIR:AXSandbox>/scripts"
        COMMENT "Copying user-space assets/scripts to sandbox output"
        VERBATIM
    )

    # Debug info file
    file(WRITE "${CMAKE_BINARY_DIR}/shader_paths.txt"
    "Shader source directory: ${SHADER_SRC_DIR}\n"
    "Shader output directory: ${SHADER_BIN_DIR}\n"
    "Build directory: ${CMAKE_BINARY_DIR}\n"
    "Executable directory: $<TARGET_FILE_DIR:AXSandbox>\n"
    )

endif()
