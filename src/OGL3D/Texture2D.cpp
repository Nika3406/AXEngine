#include "OGL3D/Texture2D.h"

#include <glad/glad.h>
#include <stb_image.h>
#include <iostream>

Texture2D::~Texture2D() {
    destroy();
}

bool Texture2D::loadFromFile(const std::string& path, bool flipVertical) {
    destroy();

    stbi_set_flip_vertically_on_load(flipVertical);

    unsigned char* data = stbi_load(path.c_str(), &width_, &height_, &channels_, 0);
    if (!data) {
        std::cerr << "[Texture2D] Failed to load: " << path << "\n";
        return false;
    }

    GLenum format = GL_RGB;
    GLenum internalFormat = GL_RGB;

    // Texture2D is currently used for regular color textures. Keep the shader's
    // lighting math linear by asking OpenGL to sRGB-decode 3/4 channel images.
    if (channels_ == 1) { format = GL_RED;  internalFormat = GL_RED; }
    else if (channels_ == 3) { format = GL_RGB;  internalFormat = GL_SRGB8; }
    else if (channels_ == 4) { format = GL_RGBA; internalFormat = GL_SRGB8_ALPHA8; }

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internalFormat,
        width_,
        height_,
        0,
        format,
        GL_UNSIGNED_BYTE,
        data
    );

    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return true;
}

bool Texture2D::createWhite() {
    destroy();

    unsigned char pixel[] = {255, 255, 255, 255};
    width_ = 1;
    height_ = 1;
    channels_ = 4;

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        1,
        1,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixel
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return true;
}

void Texture2D::bind(unsigned int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture2D::destroy() {
    if (id_) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
}