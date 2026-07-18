#pragma once

#include <string>
#include <unordered_map>

class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    // Load and compile from source files
    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);

    // Reload from the same files (hot-reload)
    bool reload();

    void bind() const;
    void unbind() const;

    bool isValid() const { return programID_ != 0; }
    unsigned int id() const { return programID_; }

    // Uniform setters — user calls these from scripts if needed
    void setInt  (const std::string& name, int value)                   const;
    void setFloat(const std::string& name, float value)                 const;
    void setVec3 (const std::string& name, float x, float y, float z)   const;
    void setMat4 (const std::string& name, const float* mat)            const;

    bool loadFromSource(const std::string& vertSource, const std::string& fragSource);
    void destroy();

    void setVec4(const std::string& name, float x, float y, float z, float w) const;

private:
    unsigned int programID_ = 0;

    std::string vertPath_;
    std::string fragPath_;

    // Uniform location cache so we don't call glGetUniformLocation every frame
    mutable std::unordered_map<std::string, int> uniformCache_;

    int getUniformLocation(const std::string& name) const;

    static unsigned int compileShader(const std::string& path, unsigned int type);
    static std::string  readFile(const std::string& path);
};
