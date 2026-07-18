#pragma once

struct RenderSettings {
    bool hdrEnabled = false;
    float exposure = 1.0f;
    float gamma = 2.2f;

    bool lightingEnabled = true;
    float ambientStrength = 0.25f;

    float lightDirection[3] = {-0.4f, -1.0f, -0.3f};
    float lightColor[3] = {1.0f, 1.0f, 1.0f};
    float lightIntensity = 1.0f;
};