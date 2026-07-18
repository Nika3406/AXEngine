#include "AXEngine/AXSoundKit.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>

namespace ax::sound {
namespace {

std::string readAllText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string trimCopy(std::string s) {
    auto isWs = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!s.empty() && isWs(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && isWs(static_cast<unsigned char>(s.back()))) s.pop_back();
    return s;
}

std::string lowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string stripQuotes(std::string s) {
    s = trimCopy(std::move(s));
    if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
        return s.substr(1, s.size() - 2);
    }
    return s;
}

std::string valueAfterKeyLoose(const std::string& text, const std::string& key) {
    // Supports both:
    //   key = value
    //   "key": "value"
    std::string low = lowerCopy(text);
    std::string lkey = lowerCopy(key);

    std::size_t pos = low.find("\"" + lkey + "\"");
    if (pos == std::string::npos) pos = low.find(lkey);
    if (pos == std::string::npos) return {};

    std::size_t sep = text.find(':', pos);
    std::size_t eq = text.find('=', pos);
    if (sep == std::string::npos || (eq != std::string::npos && eq < sep)) sep = eq;
    if (sep == std::string::npos) return {};

    std::size_t start = sep + 1;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    if (start >= text.size()) return {};

    if (text[start] == '"' || text[start] == '\'') {
        const char quote = text[start];
        std::size_t end = text.find(quote, start + 1);
        if (end == std::string::npos) return {};
        return text.substr(start + 1, end - start - 1);
    }

    std::size_t end = start;
    while (end < text.size() && text[end] != ',' && text[end] != '\n' && text[end] != '\r' && text[end] != '}') ++end;
    return stripQuotes(text.substr(start, end - start));
}

float readFloat(const std::string& text, const std::string& key, float fallback) {
    std::string v = valueAfterKeyLoose(text, key);
    if (v.empty()) return fallback;
    try { return std::stof(v); } catch (...) { return fallback; }
}

bool readBool(const std::string& text, const std::string& key, bool fallback) {
    std::string v = lowerCopy(valueAfterKeyLoose(text, key));
    if (v.empty()) return fallback;
    if (v == "true" || v == "1" || v == "yes" || v == "on") return true;
    if (v == "false" || v == "0" || v == "no" || v == "off") return false;
    return fallback;
}

std::vector<std::string> splitList(std::string v) {
    std::vector<std::string> out;
    v = trimCopy(std::move(v));
    if (v.empty()) return out;
    if (!v.empty() && v.front() == '[') v.erase(v.begin());
    if (!v.empty() && v.back() == ']') v.pop_back();
    std::stringstream ss(v);
    std::string item;
    while (std::getline(ss, item, ',')) {
        item = stripQuotes(item);
        if (!item.empty()) out.push_back(item);
    }
    if (out.empty()) {
        v = stripQuotes(v);
        if (!v.empty()) out.push_back(v);
    }
    return out;
}

std::vector<std::string> readClips(const std::string& text) {
    std::vector<std::string> clips;

    std::string list = valueAfterKeyLoose(text, "clips");
    if (!list.empty()) {
        // Loose parser may stop at the first comma. Recover array manually when possible.
        std::string low = lowerCopy(text);
        std::size_t k = low.find("clips");
        if (k != std::string::npos) {
            std::size_t lb = text.find('[', k);
            std::size_t rb = text.find(']', lb == std::string::npos ? k : lb);
            if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                list = text.substr(lb + 1, rb - lb - 1);
            }
        }
        clips = splitList(list);
    }

    const std::string low = lowerCopy(text);
    if (low.find("\"clip\"") != std::string::npos) {
        std::string single = valueAfterKeyLoose(text, "clip");
        if (!single.empty()) clips.push_back(single);
    }

    std::string path = valueAfterKeyLoose(text, "path");
    if (!path.empty()) clips.push_back(path);

    // De-duplicate while preserving order.
    std::vector<std::string> unique;
    for (const std::string& c : clips) {
        if (!c.empty() && std::find(unique.begin(), unique.end(), c) == unique.end()) unique.push_back(c);
    }
    return unique;
}

std::string basenameNoExt(const std::string& path) {
    std::filesystem::path p(path);
    return p.stem().string();
}

} // namespace

SoundSpace SoundRuntime::parseSpace(const String& value) const {
    const std::string v = lowerCopy(value);
    if (v == "ui" || v == "ui2d" || v == "2d") return SoundSpace::UI2D;
    if (v == "world" || v == "world3d" || v == "3d" || v == "spatial") return SoundSpace::World3D;
    if (v == "music" || v == "loopingmusic") return SoundSpace::Music;
    return SoundSpace::Auto;
}

bool SoundRuntime::loadSoundFile(const String& path) {
    const std::string text = readAllText(path);
    if (text.empty()) {
        debug("AXSound load failed: " + path);
        return false;
    }

    SoundDefinition def;
    def.name = valueAfterKeyLoose(text, "name");
    if (def.name.empty()) def.name = basenameNoExt(path);

    for (const std::string& c : readClips(text)) {
        def.clips.push_back(SoundClip{c, 1.0f});
    }

    def.volume = readFloat(text, "volume", def.volume);
    def.pitch = readFloat(text, "pitch", def.pitch);
    def.volumeRandomMin = readFloat(text, "volumeRandomMin", readFloat(text, "volumeMin", def.volumeRandomMin));
    def.volumeRandomMax = readFloat(text, "volumeRandomMax", readFloat(text, "volumeMax", def.volumeRandomMax));
    def.pitchRandomMin = readFloat(text, "pitchRandomMin", readFloat(text, "pitchMin", def.pitchRandomMin));
    def.pitchRandomMax = readFloat(text, "pitchRandomMax", readFloat(text, "pitchMax", def.pitchRandomMax));
    def.loop = readBool(text, "loop", def.loop);
    def.spatial = readBool(text, "spatial", def.spatial);
    def.minDistance = readFloat(text, "minDistance", def.minDistance);
    def.maxDistance = readFloat(text, "maxDistance", def.maxDistance);
    def.fadeIn = readFloat(text, "fadeIn", def.fadeIn);
    def.fadeOut = readFloat(text, "fadeOut", def.fadeOut);

    std::string category = valueAfterKeyLoose(text, "category");
    if (!category.empty()) def.category = category;

    std::string space = valueAfterKeyLoose(text, "space");
    def.space = parseSpace(space);
    if (def.space == SoundSpace::World3D) def.spatial = true;
    if (def.space == SoundSpace::Music) def.loop = true;

    definitions_[def.name] = def;
    definitions_[path] = def;
    debug("Loaded AXSound: " + def.name + " clips=" + std::to_string(def.clips.size()));
    return true;
}

int SoundRuntime::loadDirectory(const String& directory) {
    int count = 0;
    std::error_code ec;
    if (!std::filesystem::exists(directory, ec)) return 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() == ".axsound") {
            if (loadSoundFile(entry.path().string())) ++count;
        }
    }
    return count;
}

const SoundDefinition* SoundRuntime::find(const String& nameOrPath) const {
    auto it = definitions_.find(nameOrPath);
    if (it != definitions_.end()) return &it->second;
    return nullptr;
}

const SoundDefinition* SoundRuntime::loadOrFind(const String& nameOrPath) {
    if (const SoundDefinition* found = find(nameOrPath)) return found;
    if (nameOrPath.find(".axsound") != std::string::npos) {
        if (loadSoundFile(nameOrPath)) return find(nameOrPath);
    }
    return nullptr;
}

float SoundRuntime::randomRange(float mn, float mx) {
    if (mx < mn) std::swap(mx, mn);
    if (std::abs(mx - mn) < 0.0001f) return mn;
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist(mn, mx);
    return dist(rng);
}

String SoundRuntime::chooseClip(const SoundDefinition& def) {
    if (def.clips.empty()) return {};
    if (def.clips.size() == 1) return def.clips.front().path;

    float total = 0.0f;
    for (const auto& c : def.clips) total += std::max(0.001f, c.weight);
    float pick = randomRange(0.0f, total);
    for (const auto& c : def.clips) {
        pick -= std::max(0.001f, c.weight);
        if (pick <= 0.0f) return c.path;
    }
    return def.clips.back().path;
}

std::uint64_t SoundRuntime::play(const PlayRequest& request) {
    const SoundDefinition* def = loadOrFind(request.sound);

    ActiveSound voice;
    voice.id = nextId_++;
    voice.name = request.sound;
    voice.clip = request.clipOverride;
    voice.position = request.position;
    voice.ownerId = request.ownerId;
    voice.eventName = request.eventName;
    voice.loop = request.forceLoop;

    if (def) {
        voice.name = def->name;
        if (voice.clip.empty()) voice.clip = chooseClip(*def);
        voice.category = def->category;
        voice.volume = def->volume * randomRange(def->volumeRandomMin, def->volumeRandomMax) * request.volumeScale;
        voice.pitch = def->pitch * randomRange(def->pitchRandomMin, def->pitchRandomMax) * request.pitchScale;
        voice.loop = request.forceLoop || def->loop;
        voice.spatial = def->spatial;
    } else {
        voice.category = "SFX";
        voice.volume = request.volumeScale;
        voice.pitch = request.pitchScale;
    }

    if (request.stopSameNamedSound) stopNamed(voice.name);

    voice.volume *= categoryVolume(voice.category);
    active_.push_back(voice);

    debug("Play AXSound: " + voice.name + (voice.clip.empty() ? "" : " clip=" + voice.clip) + " volume=" + std::to_string(voice.volume));
    return voice.id;
}

std::uint64_t SoundRuntime::play(const String& soundNameOrPath, const Vec3& position, float volumeScale) {
    PlayRequest req;
    req.sound = soundNameOrPath;
    req.position = position;
    req.volumeScale = volumeScale;
    return play(req);
}

void SoundRuntime::stop(std::uint64_t id) {
    active_.erase(std::remove_if(active_.begin(), active_.end(), [&](const ActiveSound& s) { return s.id == id; }), active_.end());
    debug("Stop AXSound id=" + std::to_string(id));
}

void SoundRuntime::stopNamed(const String& soundName) {
    active_.erase(std::remove_if(active_.begin(), active_.end(), [&](const ActiveSound& s) { return s.name == soundName; }), active_.end());
}

void SoundRuntime::stopCategory(const String& category) {
    active_.erase(std::remove_if(active_.begin(), active_.end(), [&](const ActiveSound& s) { return s.category == category; }), active_.end());
}

void SoundRuntime::stopAll() {
    active_.clear();
    debug("Stop all AXSounds");
}

void SoundRuntime::update(float dt) {
    for (ActiveSound& s : active_) s.age += dt;

    // Without a concrete backend there is no true clip duration. Non-looping
    // requests expire after a conservative debug lifetime so the inspector stays clean.
    active_.erase(std::remove_if(active_.begin(), active_.end(), [](const ActiveSound& s) {
        return !s.loop && s.age > 2.5f;
    }), active_.end());

    trimDebug(dt);
}

void SoundRuntime::setCategoryVolume(const String& category, float volume) {
    categoryVolumes_[category] = std::clamp(volume, 0.0f, 2.0f);
}

float SoundRuntime::categoryVolume(const String& category) const {
    auto it = categoryVolumes_.find(category);
    if (it != categoryVolumes_.end()) return it->second;
    return 1.0f;
}

void SoundRuntime::debug(const String& message) {
    debugEvents_.push_back(SoundDebugEvent{message, 0.0f});
    if (echoToConsole_) std::cout << "[AXSound] " << message << '\n';
}

void SoundRuntime::trimDebug(float dt) {
    for (SoundDebugEvent& e : debugEvents_) e.age += dt;
    debugEvents_.erase(std::remove_if(debugEvents_.begin(), debugEvents_.end(), [](const SoundDebugEvent& e) {
        return e.age > 4.0f;
    }), debugEvents_.end());
    if (debugEvents_.size() > 32) debugEvents_.erase(debugEvents_.begin(), debugEvents_.begin() + static_cast<std::ptrdiff_t>(debugEvents_.size() - 32));
}

SoundRuntime& runtime() {
    static SoundRuntime r;
    return r;
}

} // namespace ax::sound
