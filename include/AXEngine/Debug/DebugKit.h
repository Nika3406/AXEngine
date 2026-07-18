#pragma once

#include "AXEngine/Core.h"

namespace ax::debug {

struct Counter {
    String name;
    float value = 0.0f;
    String unit;
};

class ProfilerFrame {
public:
    void set(const String& name, float value, String unit = {}) {
        counters_[name] = Counter{name, value, std::move(unit)};
    }

    const Map<String, Counter>& counters() const { return counters_; }

private:
    Map<String, Counter> counters_;
};

namespace Events {
    inline constexpr const char* DebugCommand = "OnDebugCommand";
    inline constexpr const char* RegistryDump = "OnRegistryDump";
}

} // namespace ax::debug
