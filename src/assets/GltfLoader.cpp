#include "assets/GltfLoader.h"

#include <iostream>
#include <memory>

std::shared_ptr<GltfAsset> GltfLoader::loadFromFile(const std::string& path) {
    error_.clear();
    warning_.clear();

    tinygltf::TinyGLTF loader;
    auto asset = std::make_shared<GltfAsset>();
    asset->sourcePath = path;

    bool ok = false;

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".glb") {
        ok = loader.LoadBinaryFromFile(&asset->model, &error_, &warning_, path);
    } else if (path.size() >= 5 && path.substr(path.size() - 5) == ".gltf") {
        ok = loader.LoadASCIIFromFile(&asset->model, &error_, &warning_, path);
    } else {
        error_ = "Unsupported file type: " + path;
        return nullptr;
    }

    if (!warning_.empty()) {
        std::cout << "TinyGLTF warning: " << warning_ << "\n";
    }

    if (!ok) {
        if (error_.empty()) {
            error_ = "Unknown glTF loading error.";
        }
        return nullptr;
    }

    return asset;
}

const std::string& GltfLoader::getLastError() const {
    return error_;
}

const std::string& GltfLoader::getLastWarning() const {
    return warning_;
}
