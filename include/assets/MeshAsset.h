#pragma once

#include <vector>

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
};

struct MeshAsset {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};
