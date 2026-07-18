#include "OGL3D/ShaderLibrary.h"
#include <iostream>

ShaderProgram* ShaderLibrary::load(const std::string& name) {
    auto it = shaders_.find(name);
    if (it != shaders_.end()) return it->second.get();

    std::string vertPath = shaderRoot_ + name + ".vert";
    std::string fragPath = shaderRoot_ + name + ".frag";

    auto program = std::make_unique<ShaderProgram>();
    if (!program->loadFromFiles(vertPath, fragPath)) {
        std::cerr << "[ShaderLibrary] Failed to load shader: " << name << "\n";
        return nullptr;
    }

    ShaderProgram* raw = program.get();
    shaders_[name] = std::move(program);
    return raw;
}

ShaderProgram* ShaderLibrary::get(const std::string& name) const {
    auto it = shaders_.find(name);
    if (it != shaders_.end()) return it->second.get();
    return nullptr;
}

void ShaderLibrary::reloadAll() {
    std::cout << "[ShaderLibrary] Reloading all shaders...\n";
    for (auto& [name, program] : shaders_) {
        if (!program->reload()) {
            std::cerr << "[ShaderLibrary] Reload failed for: " << name
            << " (keeping old program)\n";
        }
    }
}
