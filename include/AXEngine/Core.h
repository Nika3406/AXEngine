#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// AXEngine public core API.
// Purpose: give user scripts a stable Unity/Unreal-style include layer instead
// of forcing scripts to include low-level SDL/OpenGL/runtime implementation files.
namespace ax {

using String = std::string;

template <typename T>
using Array = std::vector<T>;

template <typename K, typename V>
using Map = std::unordered_map<K, V>;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float vx, float vy, float vz) : x(vx), y(vy), z(vz) {}
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Transform {
    Vec3 position{};
    Vec3 rotationDeg{};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

struct AssetRef {
    String id;
    String path;

    bool valid() const { return !id.empty() || !path.empty(); }
};

struct Time {
    static inline float deltaTime = 0.0f;
    static inline float time = 0.0f;
    static inline uint64_t frame = 0;
};

struct Logger {
    using Sink = std::function<void(const String&)>;
    static inline Sink sink = nullptr;

    static void log(const String& message) {
        if (sink) sink(message);
    }
};

} // namespace ax
