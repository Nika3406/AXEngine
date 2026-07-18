# Build-time synchronization for optional AXEngine project content.
# This script intentionally succeeds when a new project has no assets,
# generated sources, scripts, or project descriptor yet.

if(NOT DEFINED AX_SOURCE_DIR OR AX_SOURCE_DIR STREQUAL "")
    message(FATAL_ERROR "AX_SOURCE_DIR was not provided")
endif()

if(NOT DEFINED AX_BINARY_DIR OR AX_BINARY_DIR STREQUAL "")
    message(FATAL_ERROR "AX_BINARY_DIR was not provided")
endif()

function(ax_reset_directory path)
    file(REMOVE_RECURSE "${path}")
    file(MAKE_DIRECTORY "${path}")
endfunction()

# Project assets are optional. Always clear the staged directory so deleted or
# renamed source assets cannot survive as stale runtime files.
ax_reset_directory("${AX_BINARY_DIR}/assets")
if(EXISTS "${AX_SOURCE_DIR}/assets")
    file(COPY "${AX_SOURCE_DIR}/assets/" DESTINATION "${AX_BINARY_DIR}/assets")
    message(STATUS "[AXEngine] Staged project assets")
else()
    message(STATUS "[AXEngine] No assets/ directory; staged an empty asset root")
endif()

# The current renderer resolves named shader files from build/shaders/. This
# directory is optional for an empty project because the renderer also owns a
# compiled-in fallback shader.
ax_reset_directory("${AX_BINARY_DIR}/shaders")
if(EXISTS "${AX_SOURCE_DIR}/assets/shaders")
    file(COPY "${AX_SOURCE_DIR}/assets/shaders/" DESTINATION "${AX_BINARY_DIR}/shaders")
    message(STATUS "[AXEngine] Staged project shaders")
else()
    message(STATUS "[AXEngine] No project shaders; using built-in fallback support")
endif()

# Generated files are optional, but staging them is useful for editor/runtime
# inspection. They are not required by the empty-game fallback executable.
ax_reset_directory("${AX_BINARY_DIR}/generated")
if(EXISTS "${AX_SOURCE_DIR}/generated")
    file(COPY "${AX_SOURCE_DIR}/generated/" DESTINATION "${AX_BINARY_DIR}/generated")
endif()

set(AX_PROJECT_SOURCE "${AX_SOURCE_DIR}/project.axproject")
set(AX_PROJECT_DESTINATION "${AX_BINARY_DIR}/project.axproject")
if(EXISTS "${AX_PROJECT_SOURCE}")
    configure_file("${AX_PROJECT_SOURCE}" "${AX_PROJECT_DESTINATION}" COPYONLY)
elseif(EXISTS "${AX_PROJECT_DESTINATION}")
    file(REMOVE "${AX_PROJECT_DESTINATION}")
endif()
