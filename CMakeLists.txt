cmake_minimum_required(VERSION 3.20)
project(AXEngine LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AX_BUILD_EDITOR "Build the AXEditor Dear ImGui tool" ON)
option(AX_BUILD_TESTS "Build AXEngine smoke tests" ON)
option(AX_USE_VENDORED_SDL3 "Use third_party/SDL when it is present" ON)

# --- SDL3 dependency ---------------------------------------------------------
# Preferred reproducible setup: add SDL as a submodule at third_party/SDL.
# A system SDL3 package remains supported for Linux distributions and CI images.
if(AX_USE_VENDORED_SDL3 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL/CMakeLists.txt")
    message(STATUS "[AXEngine] Using vendored SDL3 from third_party/SDL")
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
else()
    find_package(SDL3 3.2 QUIET CONFIG COMPONENTS SDL3)
    if(NOT SDL3_FOUND)
        message(FATAL_ERROR
            "SDL3 was not found. Use one of these supported setups:\n"
            "  1. Add SDL as a submodule at third_party/SDL, then configure again.\n"
            "  2. Install SDL3 3.2+ development files that provide SDL3Config.cmake.\n"
            "You can set SDL3_DIR explicitly when the package is installed in a custom prefix."
        )
    endif()
endif()

# --- AX platform detection ---------------------------------------------------
if(WIN32)
    set(AX_PLATFORM_NAME "Windows")
elseif(APPLE)
    set(AX_PLATFORM_NAME "macOS")
elseif(UNIX)
    set(AX_PLATFORM_NAME "Linux")
else()
    set(AX_PLATFORM_NAME "Unknown")
endif()
message(STATUS "[AXEngine] Host platform: ${AX_PLATFORM_NAME}")
add_compile_definitions(AX_PLATFORM_NAME="${AX_PLATFORM_NAME}")

# --- glad --------------------------------------------------------------------
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
)

# --- Reusable engine library -------------------------------------------------
file(GLOB_RECURSE AX_ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

# createGame() belongs to an executable, never to the reusable engine library.
set(AX_DEFAULT_GAME_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/AXDefaultGame.cpp")
list(REMOVE_ITEM AX_ENGINE_SOURCES "${AX_DEFAULT_GAME_SOURCE}")

add_library(AXEngine STATIC ${AX_ENGINE_SOURCES})
add_library(AXEngine::AXEngine ALIAS AXEngine)

target_include_directories(AXEngine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tinygltf
)

target_link_libraries(AXEngine
    PUBLIC
        SDL3::SDL3
        glad
)

if(UNIX AND NOT APPLE)
    target_link_libraries(AXEngine PUBLIC dl)
endif()

# Reusable engine sources must never reach into project/user scripts.
foreach(AX_SOURCE IN LISTS AX_ENGINE_SOURCES)
    file(READ "${AX_SOURCE}" AX_SOURCE_TEXT)
    if(AX_SOURCE_TEXT MATCHES "scripts/Player\\.h|scripts/Environment\\.h")
        message(FATAL_ERROR
            "Reusable engine source depends on project scripts: ${AX_SOURCE}\n"
            "Move that implementation into scripts/ or introduce an engine interface."
        )
    endif()
endforeach()

# --- Game executable ---------------------------------------------------------
file(GLOB_RECURSE AX_GAME_SCRIPT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.cpp"
)

set(AX_GENERATED_SOURCES)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/AXGeneratedRegistries.cpp")
    list(APPEND AX_GENERATED_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/generated/AXGeneratedRegistries.cpp"
    )
endif()

if(AX_GAME_SCRIPT_SOURCES)
    message(STATUS "[AXEngine] Building project gameplay from scripts/.")
    set(AX_CREATE_GAME_SOURCE ${AX_GAME_SCRIPT_SOURCES})
else()
    message(STATUS "[AXEngine] No user scripts found; using the built-in empty game.")
    set(AX_CREATE_GAME_SOURCE ${AX_DEFAULT_GAME_SOURCE})
endif()

add_executable(Game
    main.cpp
    ${AX_CREATE_GAME_SOURCE}
    ${AX_GENERATED_SOURCES}
)

target_include_directories(Game PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/scripts
)

target_link_libraries(Game PRIVATE AXEngine::AXEngine)

set_target_properties(Game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# --- Optional project-content sync -------------------------------------------
# A newly downloaded AXEngine project is allowed to contain no assets/, scripts/,
# generated/, or project.axproject yet. The build-time CMake script checks every
# source path before copying, so deleting optional project content cannot break
# an otherwise valid engine build.
add_custom_target(SyncRuntimeAssets
    COMMAND ${CMAKE_COMMAND}
        -DAX_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}
        -DAX_BINARY_DIR:PATH=${CMAKE_BINARY_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SyncRuntimeAssets.cmake
    COMMENT "Refreshing optional AX project content"
    VERBATIM
)

add_dependencies(Game SyncRuntimeAssets)

# --- Asset validation --------------------------------------------------------
find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_custom_target(ValidateAssets
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_assets.py
            ${CMAKE_CURRENT_SOURCE_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Validating AX asset references"
    )
endif()

# --- Optional smoke tests ----------------------------------------------------
if(AX_BUILD_TESTS)
    enable_testing()

    add_executable(AXCoreTypeSmoke tests/CoreTypeSmoke.cpp)
    target_link_libraries(AXCoreTypeSmoke PRIVATE AXEngine::AXEngine)
    add_test(NAME AXCoreTypeSmoke COMMAND AXCoreTypeSmoke)

    add_executable(AXAssetParserSmoke tests/AssetParserSmoke.cpp)
    target_link_libraries(AXAssetParserSmoke PRIVATE AXEngine::AXEngine)
    add_test(NAME AXAssetParserSmoke COMMAND AXAssetParserSmoke)
    set_tests_properties(AXAssetParserSmoke PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# --- Optional AXEditor target ------------------------------------------------
if(AX_BUILD_EDITOR)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)

    if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
        message(FATAL_ERROR
            "AX_BUILD_EDITOR=ON but third_party/imgui was not found.\n"
            "Install it with:\n"
            "  git clone https://github.com/ocornut/imgui third_party/imgui\n"
        )
    endif()

    file(GLOB_RECURSE AX_EDITOR_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/editor/*.cpp"
    )

    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
        ${IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
    )

    add_executable(AXEditor
        ${AX_EDITOR_SOURCES}
        ${IMGUI_SOURCES}
    )

    target_include_directories(AXEditor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/editor
        ${IMGUI_DIR}
        ${IMGUI_DIR}/backends
    )

    target_link_libraries(AXEditor PRIVATE AXEngine::AXEngine)

    set_target_properties(AXEditor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endif()
