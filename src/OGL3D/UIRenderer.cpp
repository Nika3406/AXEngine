#include "OGL3D/UIRenderer.h"

#include <glad/glad.h>
#include <iostream>
#include <string>
#include <cctype>

void UIRenderer::makeOrtho(float* m, float width, float height) {
    for (int i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = 2.0f / width;
    m[5] = -2.0f / height;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] = 1.0f;
    m[15] = 1.0f;
}

bool UIRenderer::init(int width, int height) {
    width_ = width;
    height_ = height;

    const char* vert = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUV;
        uniform mat4 uProjection;
        out vec2 vUV;
        void main() {
            vUV = aUV;
            gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* frag = R"(
        #version 330 core
        in vec2 vUV;
        uniform sampler2D uTexture;
        uniform vec4 uColor;
        out vec4 FragColor;
        void main() {
            FragColor = texture(uTexture, vUV) * uColor;
        }
    )";

    if (!shader_.loadFromSource(vert, frag)) {
        std::cerr << "[UIRenderer] Failed to create UI shader.\n";
        return false;
    }

    whiteTexture_.createWhite();

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    return true;
}

void UIRenderer::begin(int width, int height) {
    width_ = width;
    height_ = height;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.bind();

    float proj[16];
    makeOrtho(proj, (float)width_, (float)height_);
    shader_.setMat4("uProjection", proj);
    shader_.setInt("uTexture", 0);
}

void UIRenderer::drawRect(float x, float y, float w, float h,
                          float r, float g, float b, float a) {
    drawImage(whiteTexture_, x, y, w, h, r, g, b, a);
}

void UIRenderer::drawImage(Texture2D& texture,
                           float x, float y, float w, float h,
                           float r, float g, float b, float a) {
    float vertices[] = {
        x,     y,     0.0f, 0.0f,
        x + w, y,     1.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y,     0.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y + h, 0.0f, 1.0f
    };

    shader_.bind();
    shader_.setVec4("uColor", r, g, b, a);
    texture.bind(0);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void UIRenderer::drawImageRegion(Texture2D& tex,
                                 float x, float y, float w, float h,
                                 float u0, float v0, float u1, float v1,
                                 float r, float g, float b, float a) {
    if (!vao_ || !vbo_) return;

    float vertices[] = {
        x,     y,     u0, v0,
        x + w, y,     u1, v0,
        x + w, y + h, u1, v1,
        x,     y,     u0, v0,
        x + w, y + h, u1, v1,
        x,     y + h, u0, v1
    };

    shader_.bind();
    shader_.setVec4("uColor", r, g, b, a);
    shader_.setInt("uTexture", 0);
    tex.bind(0);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

static const char* glyph5x7(char c) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'A': return "01110100011000111111100011000110001";
        case 'B': return "11110100011000111110100011000111110";
        case 'C': return "01111100001000010000100001000001111";
        case 'D': return "11110100011000110001100011000111110";
        case 'E': return "11111100001000011110100001000011111";
        case 'F': return "11111100001000011110100001000010000";
        case 'G': return "01111100001000010011100011000101111";
        case 'H': return "10001100011000111111100011000110001";
        case 'I': return "11111001000010000100001000010011111";
        case 'J': return "00111000100001000010100101001001100";
        case 'K': return "10001100101010011000101001001010001";
        case 'L': return "10000100001000010000100001000011111";
        case 'M': return "10001110111010110101100011000110001";
        case 'N': return "10001110011010110011100011000110001";
        case 'O': return "01110100011000110001100011000101110";
        case 'P': return "11110100011000111110100001000010000";
        case 'Q': return "01110100011000110001101011001001101";
        case 'R': return "11110100011000111110101001001010001";
        case 'S': return "01111100001000001110000010000111110";
        case 'T': return "11111001000010000100001000010000100";
        case 'U': return "10001100011000110001100011000101110";
        case 'V': return "10001100011000110001100010101000100";
        case 'W': return "10001100011000110101101011010101010";
        case 'X': return "10001100010101000100010101000110001";
        case 'Y': return "10001100010101000100001000010000100";
        case 'Z': return "11111000010001000100010001000011111";
        case '0': return "01110100011001110101110011000101110";
        case '1': return "00100011000010000100001000010001110";
        case '2': return "01110100010000100010001000100011111";
        case '3': return "11110000010000101110000010000111110";
        case '4': return "00010001100101010010111110001000010";
        case '5': return "11111100001000011110000010000111110";
        case '6': return "01110100001000011110100011000101110";
        case '7': return "11111000010001000100010000100001000";
        case '8': return "01110100011000101110100011000101110";
        case '9': return "01110100011000101111000010000101110";
        case '.': return "00000000000000000000000000110001100";
        case ',': return "00000000000000000000011000010001000";
        case ':': return "00000011000110000000011000110000000";
        case '-': return "00000000000000011111000000000000000";
        case '+': return "00000001000010011111001000010000000";
        case '/': return "00001000100001000100010000100010000";
        case '(': return "00010001000100001000010000010000010";
        case ')': return "01000001000001000010000100010001000";
        case '[': return "01110010000100001000010000100001110";
        case ']': return "01110000100001000010000100001001110";
        case '=': return "00000111110000011111000000000000000";
        case '_': return "00000000000000000000000000000011111";
        case '%': return "11001110100001000100010000101110011";
        case '!': return "00100001000010000100001000000000100";
        case '?': return "01110100010000100010001000000000100";
        case ' ': return "00000000000000000000000000000000000";
        default: return "1111110001000100010000000001000";
    }
}

void UIRenderer::drawText(const std::string& text,
                          float x, float y,
                          float scale,
                          float r, float g, float b, float a) {
    float cursorX = x;
    float cursorY = y;

    const float pixel = scale;
    const float charW = 5.0f * pixel;
    const float charH = 7.0f * pixel;
    const float spacing = 1.0f * pixel;
    const float lineSpacing = 2.0f * pixel;

    for (char c : text) {
        if (c == '\n') {
            cursorX = x;
            cursorY += charH + lineSpacing;
            continue;
        }

        const char* glyph = glyph5x7(c);

        for (int gy = 0; gy < 7; ++gy) {
            for (int gx = 0; gx < 5; ++gx) {
                if (glyph[gy * 5 + gx] == '1') {
                    drawRect(
                        cursorX + static_cast<float>(gx) * pixel,
                        cursorY + static_cast<float>(gy) * pixel,
                        pixel,
                        pixel,
                        r,
                        g,
                        b,
                        a
                    );
                }
            }
        }

        cursorX += charW + spacing;
    }
}

void UIRenderer::shutdown() {
    whiteTexture_.destroy();

    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);

    vbo_ = 0;
    vao_ = 0;

    shader_.destroy();
}
