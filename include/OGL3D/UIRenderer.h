#pragma once

#include "OGL3D/ShaderProgram.h"
#include "OGL3D/Texture2D.h"

#include <string>

class UIRenderer {
public:
    bool init(int width, int height);
    void begin(int width, int height);

    void drawRect(float x, float y, float w, float h,
                  float r, float g, float b, float a = 1.0f);

    void drawImage(Texture2D& texture,
                   float x, float y, float w, float h,
                   float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    void drawImageRegion(Texture2D& tex,
                         float x, float y, float w, float h,
                         float u0, float v0, float u1, float v1,
                         float r = 1.0f, float g = 1.0f, float b = 1.0f, float a = 1.0f);

    void drawText(const std::string& text,
                  float x, float y,
                  float scale,
                  float r, float g, float b, float a = 1.0f);

    void shutdown();

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;

    ShaderProgram shader_;
    Texture2D whiteTexture_;

    int width_ = 1280;
    int height_ = 720;

    void makeOrtho(float* out, float width, float height);
};
