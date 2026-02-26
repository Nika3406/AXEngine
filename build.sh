#!/bin/bash
# build.sh - Complete build script for AXEngine

set -e  # Exit on error

echo "==================================="
echo "AXEngine Build Script"
echo "==================================="

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

# Step 1: Create shader directories
echo ""
echo "[1/4] Creating shader directories..."
mkdir -p shaders/src
mkdir -p shaders/compiled/spirv

# Step 2: Check for shader source files
echo ""
echo "[2/4] Checking shader files..."
if [ ! -f "shaders/src/varying.def.sc" ]; then
    echo "ERROR: Missing shaders/src/varying.def.sc"
    echo "Please copy the shader files to shaders/src/"
    exit 1
fi

if [ ! -f "shaders/src/vs_mesh.sc" ]; then
    echo "ERROR: Missing shaders/src/vs_mesh.sc"
    exit 1
fi

if [ ! -f "shaders/src/fs_mesh.sc" ]; then
    echo "ERROR: Missing shaders/src/fs_mesh.sc"
    exit 1
fi

echo "All shader source files found!"

# Step 3: Configure CMake
echo ""
echo "[3/4] Configuring CMake..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Step 4: Build everything (shaderc, shaders, and executable)
echo ""
echo "[4/4] Building everything (this may take a while)..."
make -j$(nproc)

echo ""
echo "==================================="
echo "Build complete!"
echo "==================================="
echo ""
echo "Checking shader files in build directory..."
if [ -f "shaders/compiled/spirv/vs_mesh.bin" ]; then
    echo "vs_mesh.bin found"
else
    echo "vs_mesh.bin MISSING"
fi

if [ -f "shaders/compiled/spirv/fs_mesh.bin" ]; then
    echo "fs_mesh.bin found"
else
    echo "fs_mesh.bin MISSING"
fi

echo ""
echo "To run the game:"
echo "  cd build"
echo "  ./AXEngine"
echo ""