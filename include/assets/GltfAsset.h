#pragma once

#include <string>
#include <memory>
#include <tiny_gltf.h>

struct GltfAsset {
    std::string sourcePath;
    tinygltf::Model model;
};
