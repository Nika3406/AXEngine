#pragma once

#include <string>

class Texture2D {
public:
    Texture2D() = default;
    ~Texture2D();

    Texture2D(const Texture2D&) = delete;
    Texture2D& operator=(const Texture2D&) = delete;

    bool loadFromFile(const std::string& path, bool flipVertical = true);
    bool createWhite();

    void bind(unsigned int slot = 0) const;
    void destroy();

    bool isValid() const { return id_ != 0; }
    unsigned int id() const { return id_; }

    int width() const { return width_; }
    int height() const { return height_; }

private:
    unsigned int id_ = 0;
    int width_ = 0;
    int height_ = 0;
    int channels_ = 0;
};