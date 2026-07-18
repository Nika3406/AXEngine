#include "AXEngine/AXVFXKit.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace ax::vfx {
namespace {

static String readAllText(const String& path) {
    std::ifstream in(path);
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static String lowerCopy(String s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

static String trim(String s) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && isSpace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

static String readStringKey(const String& text, const String& key, const String& fallback = {}) {
    const String quoted = '"' + key + '"';
    size_t pos = text.find(quoted);
    if (pos == String::npos) pos = text.find(key);
    if (pos == String::npos) return fallback;

    pos = text.find(':', pos);
    if (pos == String::npos) pos = text.find('=', text.find(key));
    if (pos == String::npos) return fallback;
    ++pos;

    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    if (pos >= text.size()) return fallback;

    if (text[pos] == '"' || text[pos] == '\'') {
        char quote = text[pos++];
        size_t end = text.find(quote, pos);
        if (end == String::npos) return fallback;
        return text.substr(pos, end - pos);
    }

    size_t end = pos;
    while (end < text.size() && text[end] != '\n' && text[end] != ',' && text[end] != '}') ++end;
    return trim(text.substr(pos, end - pos));
}

static float readFloatKey(const String& text, const String& key, float fallback) {
    String s = readStringKey(text, key, {});
    if (s.empty()) return fallback;
    try {
        return std::stof(s);
    } catch (...) {
        return fallback;
    }
}

static bool readBoolKey(const String& text, const String& key, bool fallback) {
    String s = lowerCopy(readStringKey(text, key, {}));
    if (s.empty()) return fallback;
    return s == "true" || s == "1" || s == "yes" || s == "on";
}

static Vec3 readVec3Key(const String& text, const String& key, Vec3 fallback) {
    size_t pos = text.find('"' + key + '"');
    if (pos == String::npos) pos = text.find(key);
    if (pos == String::npos) return fallback;
    pos = text.find('[', pos);
    if (pos == String::npos) return fallback;
    size_t end = text.find(']', pos);
    if (end == String::npos) return fallback;

    String inner = text.substr(pos + 1, end - pos - 1);
    std::replace(inner.begin(), inner.end(), ',', ' ');
    std::istringstream ss(inner);
    Vec3 out = fallback;
    ss >> out.x >> out.y >> out.z;
    return out;
}

static Color readColorKey(const String& text, const String& key, Color fallback) {
    size_t pos = text.find('"' + key + '"');
    if (pos == String::npos) pos = text.find(key);
    if (pos == String::npos) return fallback;
    pos = text.find('[', pos);
    if (pos == String::npos) return fallback;
    size_t end = text.find(']', pos);
    if (end == String::npos) return fallback;

    String inner = text.substr(pos + 1, end - pos - 1);
    std::replace(inner.begin(), inner.end(), ',', ' ');
    std::istringstream ss(inner);
    Color out = fallback;
    ss >> out.r >> out.g >> out.b;
    if (!(ss >> out.a)) out.a = fallback.a;
    return out;
}

static VFXKind parseKind(const String& raw) {
    const String s = lowerCopy(raw);
    if (s.find("mesh") != String::npos || s.find("blender") != String::npos || s.find("glb") != String::npos) return VFXKind::Mesh;
    if (s.find("sprite") != String::npos || s.find("image") != String::npos) return VFXKind::Sprite;
    if (s.find("trail") != String::npos) return VFXKind::Trail;
    if (s.find("particle") != String::npos || s.find("burst") != String::npos) return VFXKind::ParticleBurst;
    if (s.find("screen") != String::npos || s.find("flash") != String::npos) return VFXKind::ScreenFlash;
    if (s.find("ui") != String::npos) return VFXKind::UI;
    return VFXKind::DebugSpark;
}

static VFXFacingMode parseFacing(const String& raw) {
    const String s = lowerCopy(raw);
    if (s.find("billboard") != String::npos) return VFXFacingMode::BillboardCamera;
    if (s.find("socket") != String::npos || s.find("attach") != String::npos) return VFXFacingMode::AttachToSocket;
    if (s.find("world") != String::npos) return VFXFacingMode::World;
    return VFXFacingMode::FollowDirection;
}

static Vec3 mulVec3(Vec3 a, Vec3 b) {
    return Vec3{a.x * b.x, a.y * b.y, a.z * b.z};
}

static Vec3 mulVec3(Vec3 a, float s) {
    return Vec3{a.x * s, a.y * s, a.z * s};
}

static Color mulColor(Color a, Color b) {
    return Color{a.r * b.r, a.g * b.g, a.b * b.b, a.a * b.a};
}

static Vec3 normalize(Vec3 v) {
    const float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 0.0001f) return Vec3{0.0f, 0.0f, 1.0f};
    return Vec3{v.x / len, v.y / len, v.z / len};
}

static float yawFromDirection(Vec3 dir) {
    dir = normalize(dir);
    return std::atan2(dir.x, dir.z) * 57.2957795f;
}

} // namespace

Vec3 Curve3::sample(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t < 0.5f) {
        const float u = t * 2.0f;
        return Vec3{start.x + (mid.x - start.x) * u, start.y + (mid.y - start.y) * u, start.z + (mid.z - start.z) * u};
    }
    const float u = (t - 0.5f) * 2.0f;
    return Vec3{mid.x + (end.x - mid.x) * u, mid.y + (end.y - mid.y) * u, mid.z + (end.z - mid.z) * u};
}

float Curve1::sample(float t) const {
    t = std::clamp(t, 0.0f, 1.0f);
    if (t < 0.5f) {
        const float u = t * 2.0f;
        return start + (mid - start) * u;
    }
    const float u = (t - 0.5f) * 2.0f;
    return mid + (end - mid) * u;
}

bool VFXRuntime::loadDefinition(const String& path, VFXDefinition& outDefinition) const {
    const String text = readAllText(path);
    if (text.empty()) return false;

    VFXDefinition def;
    def.name = readStringKey(text, "name", path);
    def.kind = parseKind(readStringKey(text, "kind", readStringKey(text, "type", "Mesh")));
    def.meshPath = readStringKey(text, "mesh", readStringKey(text, "meshPath", {}));
    def.spritePath = readStringKey(text, "sprite", readStringKey(text, "spritePath", {}));
    def.materialPath = readStringKey(text, "material", readStringKey(text, "materialPath", {}));
    def.lifetime = readFloatKey(text, "lifetime", readFloatKey(text, "duration", 0.25f));
    def.loop = readBoolKey(text, "loop", false);
    def.spawnOnce = readBoolKey(text, "spawnOnce", true);
    def.debugFallback = readBoolKey(text, "debugFallback", true);
    def.facing = parseFacing(readStringKey(text, "facing", readStringKey(text, "rotationMode", "FollowDirection")));
    def.attachTo = readStringKey(text, "attachTo", readStringKey(text, "socket", {}));
    def.localOffset = readVec3Key(text, "offset", def.localOffset);
    def.localRotationDeg = readVec3Key(text, "rotation", def.localRotationDeg);
    def.baseScale = readVec3Key(text, "scale", def.baseScale);
    def.tint = readColorKey(text, "tint", def.tint);
    def.scaleOverLife.start = readVec3Key(text, "scaleStart", def.scaleOverLife.start);
    def.scaleOverLife.mid = readVec3Key(text, "scaleMid", def.scaleOverLife.mid);
    def.scaleOverLife.end = readVec3Key(text, "scaleEnd", def.scaleOverLife.end);
    def.alphaOverLife.start = readFloatKey(text, "alphaStart", def.alphaOverLife.start);
    def.alphaOverLife.mid = readFloatKey(text, "alphaMid", def.alphaOverLife.mid);
    def.alphaOverLife.end = readFloatKey(text, "alphaEnd", def.alphaOverLife.end);

    if (def.lifetime <= 0.001f) def.lifetime = 0.25f;
    if (def.kind == VFXKind::Mesh && def.meshPath.empty()) def.kind = VFXKind::DebugSpark;
    if (def.kind == VFXKind::Sprite && def.spritePath.empty()) def.kind = VFXKind::DebugSpark;

    outDefinition = def;
    return true;
}

VFXDefinition VFXRuntime::fallbackDefinition(const SpawnRequest& request) const {
    VFXDefinition def;
    def.name = request.name.empty() ? "HitSpark" : request.name;
    def.kind = VFXKind::DebugSpark;
    def.lifetime = 0.22f;
    def.tint = request.tint;
    def.baseScale = Vec3{request.scale.x, request.scale.y, request.scale.z};
    def.debugFallback = true;
    def.alphaOverLife = Curve1{1.0f, 1.0f, 0.0f};
    def.scaleOverLife = Curve3{Vec3{0.8f, 0.8f, 0.8f}, Vec3{1.15f, 1.15f, 1.15f}, Vec3{0.2f, 0.2f, 0.2f}};
    return def;
}

uint64_t VFXRuntime::spawn(const SpawnRequest& request) {
    VFXDefinition def;
    bool loaded = false;
    if (!request.asset.empty()) {
        loaded = loadDefinition(request.asset, def);
    }
    if (!loaded) {
        def = fallbackDefinition(request);
    }

    VFXInstance inst;
    inst.id = nextId_++;
    inst.definition = def;
    inst.ownerId = request.ownerId;
    inst.socket = request.socket.empty() ? def.attachTo : request.socket;
    inst.position = request.position;
    inst.direction = normalize(request.direction);
    inst.rotationDeg = request.rotationDeg;
    if (def.facing == VFXFacingMode::FollowDirection) {
        inst.rotationDeg.y = yawFromDirection(inst.direction);
    }
    inst.requestScale = request.scale;
    inst.requestTint = request.tint;
    inst.intensity = request.intensity;
    inst.alive = true;
    instances_.push_back(inst);
    return inst.id;
}

void VFXRuntime::update(float dt) {
    for (VFXInstance& inst : instances_) {
        if (!inst.alive) continue;
        inst.age += std::max(0.0f, dt);
        if (!inst.definition.loop && inst.age >= inst.definition.lifetime) {
            inst.alive = false;
        } else if (inst.definition.loop && inst.definition.lifetime > 0.001f) {
            while (inst.age >= inst.definition.lifetime) inst.age -= inst.definition.lifetime;
        }
    }

    instances_.erase(
        std::remove_if(instances_.begin(), instances_.end(), [](const VFXInstance& inst) { return !inst.alive; }),
        instances_.end()
    );
}

void VFXRuntime::clear() {
    instances_.clear();
}

Array<VFXRenderCommand> VFXRuntime::renderCommands() const {
    Array<VFXRenderCommand> out;
    out.reserve(instances_.size());

    for (const VFXInstance& inst : instances_) {
        const VFXDefinition& def = inst.definition;
        const float t = def.lifetime > 0.001f ? std::clamp(inst.age / def.lifetime, 0.0f, 1.0f) : 1.0f;
        VFXRenderCommand cmd;
        cmd.id = inst.id;
        cmd.name = def.name;
        cmd.meshPath = def.meshPath;
        cmd.spritePath = def.spritePath;
        cmd.materialPath = def.materialPath;
        cmd.meshBacked = def.kind == VFXKind::Mesh && !def.meshPath.empty();
        cmd.spriteBacked = def.kind == VFXKind::Sprite && !def.spritePath.empty();
        cmd.transform.position = Vec3{inst.position.x + def.localOffset.x, inst.position.y + def.localOffset.y, inst.position.z + def.localOffset.z};
        cmd.transform.rotationDeg = Vec3{inst.rotationDeg.x + def.localRotationDeg.x, inst.rotationDeg.y + def.localRotationDeg.y, inst.rotationDeg.z + def.localRotationDeg.z};
        cmd.transform.scale = mulVec3(mulVec3(def.baseScale, inst.requestScale), def.scaleOverLife.sample(t));
        cmd.tint = mulColor(def.tint, inst.requestTint);
        cmd.tint.a *= def.alphaOverLife.sample(t);
        cmd.normalizedTime = t;
        out.push_back(cmd);
    }

    return out;
}

Array<VFXDebugLine> VFXRuntime::debugLines() const {
    Array<VFXDebugLine> out;
    const Array<VFXRenderCommand> commands = renderCommands();
    for (const VFXRenderCommand& cmd : commands) {
        if (!cmd.meshBacked && !cmd.spriteBacked && cmd.name.empty()) continue;
        const Vec3 p = cmd.transform.position;
        const float sx = std::max(0.05f, cmd.transform.scale.x);
        const float sy = std::max(0.05f, cmd.transform.scale.y);
        const float sz = std::max(0.05f, cmd.transform.scale.z);
        const Color c = cmd.tint;
        out.push_back(VFXDebugLine{Vec3{p.x - sx, p.y, p.z}, Vec3{p.x + sx, p.y, p.z}, c});
        out.push_back(VFXDebugLine{Vec3{p.x, p.y - sy, p.z}, Vec3{p.x, p.y + sy, p.z}, c});
        out.push_back(VFXDebugLine{Vec3{p.x, p.y, p.z - sz}, Vec3{p.x, p.y, p.z + sz}, c});
    }
    return out;
}

VFXRuntime& runtime() {
    static VFXRuntime rt;
    return rt;
}

uint64_t spawnMeshVFX(const String& axvfxPath, const Vec3& position, const Vec3& direction, float scale) {
    SpawnRequest req;
    req.asset = axvfxPath;
    req.position = position;
    req.direction = direction;
    req.scale = Vec3{scale, scale, scale};
    return runtime().spawn(req);
}

uint64_t spawnHitSpark(const Vec3& position, const Vec3& direction, const Color& tint, float radius) {
    SpawnRequest req;
    req.asset = "assets/vfx/HitSpark_Mesh.axvfx";
    req.name = "HitSpark";
    req.position = position;
    req.direction = direction;
    req.scale = Vec3{radius, radius, radius};
    req.tint = tint;
    return runtime().spawn(req);
}

} // namespace ax::vfx
