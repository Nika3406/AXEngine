#include "OGL3D/ShaderProgram.h"

#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <sstream>

ShaderProgram::~ShaderProgram() {
    destroy();
}

std::string ShaderProgram::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[ShaderProgram] Cannot open: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int ShaderProgram::compileShader(const std::string& path, unsigned int type) {
    std::string src = readFile(path);
    if (src.empty()) return 0;

    const char* cstr = src.c_str();
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    int ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "[ShaderProgram] Compile error in " << path << ":\n" << log << "\n";
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool ShaderProgram::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    vertPath_ = vertPath;
    fragPath_ = fragPath;
    return reload();
}

bool ShaderProgram::reload() {
    unsigned int vs = compileShader(vertPath_, GL_VERTEX_SHADER);
    unsigned int fs = compileShader(fragPath_, GL_FRAGMENT_SHADER);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    unsigned int newProgram = glCreateProgram();
    glAttachShader(newProgram, vs);
    glAttachShader(newProgram, fs);
    glLinkProgram(newProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok;
    glGetProgramiv(newProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(newProgram, 1024, nullptr, log);
        std::cerr << "[ShaderProgram] Link error:\n" << log << "\n";
        glDeleteProgram(newProgram);
        return false;
    }

    // Only swap in the new program if everything succeeded —
    // old program keeps running if reload fails
    if (programID_) glDeleteProgram(programID_);
    programID_ = newProgram;
    uniformCache_.clear();

    std::cout << "[ShaderProgram] Loaded: " << vertPath_ << " + " << fragPath_ << "\n";
    return true;
}

void ShaderProgram::bind()   const { glUseProgram(programID_); }
void ShaderProgram::unbind() const { glUseProgram(0); }

int ShaderProgram::getUniformLocation(const std::string& name) const {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    int loc = glGetUniformLocation(programID_, name.c_str());
    uniformCache_[name] = loc;
    return loc;
}

static unsigned int compileShaderFromSource(const std::string& source, unsigned int type) {
    const char* cstr = source.c_str();

    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &cstr, nullptr);
    glCompileShader(shader);

    int ok;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, log);
        std::cerr << "[ShaderProgram] Source compile error:\n" << log << "\n";
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool ShaderProgram::loadFromSource(const std::string& vertSource, const std::string& fragSource) {
    unsigned int vs = compileShaderFromSource(vertSource, GL_VERTEX_SHADER);
    unsigned int fs = compileShaderFromSource(fragSource, GL_FRAGMENT_SHADER);

    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }

    unsigned int newProgram = glCreateProgram();
    glAttachShader(newProgram, vs);
    glAttachShader(newProgram, fs);
    glLinkProgram(newProgram);

    glDeleteShader(vs);
    glDeleteShader(fs);

    int ok;
    glGetProgramiv(newProgram, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(newProgram, 1024, nullptr, log);
        std::cerr << "[ShaderProgram] Source link error:\n" << log << "\n";
        glDeleteProgram(newProgram);
        return false;
    }

    destroy();
    programID_ = newProgram;
    uniformCache_.clear();

    return true;
}

void ShaderProgram::destroy() {
    if (programID_) {
        glDeleteProgram(programID_);
        programID_ = 0;
    }
    uniformCache_.clear();
}

void ShaderProgram::setVec4(const std::string& name, float x, float y, float z, float w) const {
    glUniform4f(getUniformLocation(name), x, y, z, w);
}

void ShaderProgram::setInt(const std::string& name, int value) const {
    glUniform1i(getUniformLocation(name), value);
}

void ShaderProgram::setFloat(const std::string& name, float value) const {
    glUniform1f(getUniformLocation(name), value);
}

void ShaderProgram::setVec3(const std::string& name, float x, float y, float z) const {
    glUniform3f(getUniformLocation(name), x, y, z);
}

void ShaderProgram::setMat4(const std::string& name, const float* mat) const {
    glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, mat);
}
