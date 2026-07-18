#pragma once

#include "OGL3D/ShaderProgram.h"
#include <memory>
#include <string>
#include <unordered_map>

class ShaderLibrary {
public:
    // Load a shader by name — looks in the staged shaders/<name>.vert + .frag directory
    // Returns nullptr on failure, cached on subsequent calls
    ShaderProgram* load(const std::string& name);

    // Get a previously loaded shader (nullptr if not loaded)
    ShaderProgram* get(const std::string& name) const;

    // Reload all shaders from disk — call this when files change
    void reloadAll();

    // Set the root directory shaders are loaded from (default: "shaders/")
    void setShaderRoot(const std::string& root) { shaderRoot_ = root; }

private:
    std::unordered_map<std::string, std::unique_ptr<ShaderProgram>> shaders_;
    std::string shaderRoot_ = "shaders/";
};
