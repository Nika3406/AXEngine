#include "runtime/AXAttackColliderSystem.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace ax {
namespace {

static std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static float degToRad(float v) { return v * 3.1415926535f / 180.0f; }

static float sqr(float v) { return v * v; }

static float readFloatAfter(const std::string& text, const std::string& key, float fallback) {
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return fallback;
    p = text.find(':', p);
    if (p == std::string::npos) return fallback;
    ++p;
    while (p < text.size() && (text[p] == ' ' || text[p] == '\t')) ++p;
    char* end = nullptr;
    float v = std::strtof(text.c_str() + p, &end);
    return end == text.c_str() + p ? fallback : v;
}

static bool readBoolAfter(const std::string& text, const std::string& key, bool fallback) {
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return fallback;
    p = text.find(':', p);
    if (p == std::string::npos) return fallback;
    const std::string tail = lowerCopy(text.substr(p + 1, 16));
    if (tail.find("true") != std::string::npos) return true;
    if (tail.find("false") != std::string::npos) return false;
    return fallback;
}

static std::string readStringAfter(const std::string& text, const std::string& key, const std::string& fallback) {
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return fallback;
    p = text.find(':', p);
    if (p == std::string::npos) return fallback;
    p = text.find('"', p + 1);
    if (p == std::string::npos) return fallback;
    size_t q = text.find('"', p + 1);
    if (q == std::string::npos) return fallback;
    return text.substr(p + 1, q - p - 1);
}

static AttackVec3 readVec3After(const std::string& text, const std::string& key, AttackVec3 fallback) {
    const std::string needle = "\"" + key + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return fallback;
    p = text.find('[', p);
    if (p == std::string::npos) return fallback;
    size_t q = text.find(']', p);
    if (q == std::string::npos) return fallback;
    std::string inside = text.substr(p + 1, q - p - 1);
    std::replace(inside.begin(), inside.end(), ',', ' ');
    std::istringstream is(inside);
    AttackVec3 v = fallback;
    is >> v.x >> v.y >> v.z;
    return v;
}

static void writeVec3(std::ostream& os, const AttackVec3& v) {
    os << "[" << v.x << ", " << v.y << ", " << v.z << "]";
}

static std::vector<std::string> objectBlocksInArray(const std::string& text, const std::string& arrayKey) {
    std::vector<std::string> out;
    const std::string needle = "\"" + arrayKey + "\"";
    size_t p = text.find(needle);
    if (p == std::string::npos) return out;
    p = text.find('[', p);
    if (p == std::string::npos) return out;
    int bracketDepth = 0;
    int braceDepth = 0;
    size_t blockStart = std::string::npos;
    for (size_t i = p; i < text.size(); ++i) {
        char c = text[i];
        if (c == '[') ++bracketDepth;
        else if (c == ']') {
            --bracketDepth;
            if (bracketDepth <= 0) break;
        } else if (c == '{') {
            if (braceDepth == 0) blockStart = i;
            ++braceDepth;
        } else if (c == '}') {
            --braceDepth;
            if (braceDepth == 0 && blockStart != std::string::npos) {
                out.push_back(text.substr(blockStart, i - blockStart + 1));
                blockStart = std::string::npos;
            }
        }
    }
    return out;
}

static AttackVec3 rotateYaw(const AttackVec3& v, float yawDegrees) {
    float y = degToRad(yawDegrees);
    float c = std::cos(y), s = std::sin(y);
    return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}

static AttackVec3 inverseRotateYaw(const AttackVec3& v, float yawDegrees) {
    return rotateYaw(v, -yawDegrees);
}

static AttackVec3 sub(const AttackVec3& a, const AttackVec3& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }

} // namespace

const char* AttackColliderSystem::shapeToString(AttackColliderShape shape) {
    switch (shape) {
        case AttackColliderShape::Sphere: return "Sphere";
        case AttackColliderShape::Capsule: return "Capsule";
        case AttackColliderShape::Cylinder: return "Cylinder";
        case AttackColliderShape::Box: default: return "Box";
    }
}

AttackColliderShape AttackColliderSystem::shapeFromString(const std::string& shape) {
    std::string s = lowerCopy(shape);
    if (s == "sphere") return AttackColliderShape::Sphere;
    if (s == "capsule") return AttackColliderShape::Capsule;
    if (s == "cylinder") return AttackColliderShape::Cylinder;
    return AttackColliderShape::Box;
}

const char* AttackColliderSystem::directionToString(AttackColliderDirection direction) {
    switch (direction) {
        case AttackColliderDirection::Up: return "Up";
        case AttackColliderDirection::Down: return "Down";
        case AttackColliderDirection::Left: return "Left";
        case AttackColliderDirection::Right: return "Right";
        case AttackColliderDirection::Back: return "Back";
        case AttackColliderDirection::Custom: return "Custom";
        case AttackColliderDirection::Forward: default: return "Forward";
    }
}

AttackColliderDirection AttackColliderSystem::directionFromString(const std::string& direction) {
    std::string s = lowerCopy(direction);
    if (s == "up") return AttackColliderDirection::Up;
    if (s == "down") return AttackColliderDirection::Down;
    if (s == "left") return AttackColliderDirection::Left;
    if (s == "right") return AttackColliderDirection::Right;
    if (s == "back") return AttackColliderDirection::Back;
    if (s == "custom") return AttackColliderDirection::Custom;
    return AttackColliderDirection::Forward;
}

bool AttackColliderSystem::loadAttackFile(const std::string& path, AttackDefinition& outAttack, std::string* error) const {
    std::ifstream in(path);
    if (!in) {
        if (error) *error = "Could not open attack file: " + path;
        return false;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string text = buffer.str();

    outAttack = AttackDefinition{};
    outAttack.name = readStringAfter(text, "name", outAttack.name);
    outAttack.duration = readFloatAfter(text, "duration", outAttack.duration);

    auto blocks = objectBlocksInArray(text, "colliders");
    for (const auto& b : blocks) {
        AttackColliderVolume c;
        c.name = readStringAfter(b, "name", c.name);
        c.shape = shapeFromString(readStringAfter(b, "shape", shapeToString(c.shape)));
        c.direction = directionFromString(readStringAfter(b, "direction", directionToString(c.direction)));
        c.offset = readVec3After(b, "offset", c.offset);
        c.rotation = readVec3After(b, "rotation", c.rotation);
        c.size = readVec3After(b, "size", c.size);
        c.radius = readFloatAfter(b, "radius", c.radius);
        c.height = readFloatAfter(b, "height", c.height);
        c.activeFrom = readFloatAfter(b, "activeFrom", c.activeFrom);
        c.activeTo = readFloatAfter(b, "activeTo", c.activeTo);
        c.damage = readFloatAfter(b, "damage", c.damage);
        c.hitstop = readFloatAfter(b, "hitstop", c.hitstop);
        c.knockback = readVec3After(b, "knockback", c.knockback);
        c.launch = readBoolAfter(b, "launch", c.launch);
        c.knockdown = readBoolAfter(b, "knockdown", c.knockdown);
        c.hitOncePerTarget = readBoolAfter(b, "hitOncePerTarget", c.hitOncePerTarget);
        outAttack.colliders.push_back(c);
    }

    if (outAttack.colliders.empty()) {
        outAttack.colliders.push_back(AttackColliderVolume{});
    }
    return true;
}

bool AttackColliderSystem::saveAttackFile(const std::string& path, const AttackDefinition& attack, std::string* error) const {
    std::ofstream out(path);
    if (!out) {
        if (error) *error = "Could not write attack file: " + path;
        return false;
    }
    out << "{\n";
    out << "  \"format\": \"AXAttack\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"name\": \"" << attack.name << "\",\n";
    out << "  \"duration\": " << attack.duration << ",\n";
    out << "  \"colliders\": [\n";
    for (size_t i = 0; i < attack.colliders.size(); ++i) {
        const auto& c = attack.colliders[i];
        out << "    {\n";
        out << "      \"name\": \"" << c.name << "\",\n";
        out << "      \"shape\": \"" << shapeToString(c.shape) << "\",\n";
        out << "      \"direction\": \"" << directionToString(c.direction) << "\",\n";
        out << "      \"offset\": "; writeVec3(out, c.offset); out << ",\n";
        out << "      \"rotation\": "; writeVec3(out, c.rotation); out << ",\n";
        out << "      \"size\": "; writeVec3(out, c.size); out << ",\n";
        out << "      \"radius\": " << c.radius << ",\n";
        out << "      \"height\": " << c.height << ",\n";
        out << "      \"activeFrom\": " << c.activeFrom << ",\n";
        out << "      \"activeTo\": " << c.activeTo << ",\n";
        out << "      \"damage\": " << c.damage << ",\n";
        out << "      \"hitstop\": " << c.hitstop << ",\n";
        out << "      \"knockback\": "; writeVec3(out, c.knockback); out << ",\n";
        out << "      \"launch\": " << (c.launch ? "true" : "false") << ",\n";
        out << "      \"knockdown\": " << (c.knockdown ? "true" : "false") << ",\n";
        out << "      \"hitOncePerTarget\": " << (c.hitOncePerTarget ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < attack.colliders.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

std::vector<const AttackColliderVolume*> AttackColliderSystem::activeColliders(const AttackDefinition& attack, float timeSeconds) const {
    std::vector<const AttackColliderVolume*> out;
    for (const auto& c : attack.colliders) {
        if (timeSeconds >= c.activeFrom && timeSeconds <= c.activeTo) out.push_back(&c);
    }
    return out;
}

std::vector<AttackHitResult> AttackColliderSystem::evaluate(
    const AttackDefinition& attack,
    float timeSeconds,
    const AttackVec3& attackerPosition,
    float attackerYawDegrees,
    const std::vector<Hurtbox>& hurtboxes
) {
    std::vector<AttackHitResult> hits;
    auto active = activeColliders(attack, timeSeconds);
    for (const AttackColliderVolume* c : active) {
        for (const auto& h : hurtboxes) {
            if (!h.active) continue;
            if (!pointInsideAttackVolume(*c, attackerPosition, attackerYawDegrees, h.center)) continue;
            std::string key = attack.name + ":" + c->name + ":" + h.targetId;
            if (c->hitOncePerTarget && hitMemory_.find(key) != hitMemory_.end()) continue;
            hitMemory_.insert(key);
            AttackHitResult r;
            r.attackName = attack.name;
            r.colliderName = c->name;
            r.targetId = h.targetId;
            r.damage = c->damage;
            r.hitstop = c->hitstop;
            r.knockback = c->knockback;
            r.launch = c->launch;
            r.knockdown = c->knockdown;
            hits.push_back(r);
        }
    }
    return hits;
}

void AttackColliderSystem::resetHitMemory() { hitMemory_.clear(); }

AttackVec3 authoredOffsetToWorld(const AttackVec3& attackerPosition, float attackerYawDegrees, const AttackVec3& localOffset) {
    AttackVec3 r = rotateYaw(localOffset, attackerYawDegrees);
    return {attackerPosition.x + r.x, attackerPosition.y + r.y, attackerPosition.z + r.z};
}

bool pointInsideAttackVolume(const AttackColliderVolume& collider, const AttackVec3& attackerPosition, float attackerYawDegrees, const AttackVec3& point) {
    AttackVec3 center = authoredOffsetToWorld(attackerPosition, attackerYawDegrees, collider.offset);
    AttackVec3 local = inverseRotateYaw(sub(point, center), attackerYawDegrees + collider.rotation.y);

    if (collider.shape == AttackColliderShape::Sphere) {
        return sqr(local.x) + sqr(local.y) + sqr(local.z) <= sqr(collider.radius);
    }

    if (collider.shape == AttackColliderShape::Cylinder) {
        float halfH = collider.height * 0.5f;
        return std::abs(local.y) <= halfH && (sqr(local.x) + sqr(local.z) <= sqr(collider.radius));
    }

    if (collider.shape == AttackColliderShape::Capsule) {
        float halfLine = std::max(0.0f, collider.height * 0.5f - collider.radius);
        float clampedY = std::clamp(local.y, -halfLine, halfLine);
        return sqr(local.x) + sqr(local.y - clampedY) + sqr(local.z) <= sqr(collider.radius);
    }

    AttackVec3 half = {collider.size.x * 0.5f, collider.size.y * 0.5f, collider.size.z * 0.5f};
    return std::abs(local.x) <= half.x && std::abs(local.y) <= half.y && std::abs(local.z) <= half.z;
}

} // namespace ax
