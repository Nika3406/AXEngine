#pragma once

#include "EditorState.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace axed {

inline std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

inline bool containsNoCase(const std::string& s, const std::string& q) {
    return lower(s).find(lower(q)) != std::string::npos;
}

inline std::filesystem::path resolvePath(const EditorState& state, const std::string& path) {
    std::filesystem::path p(path);
    if (!p.is_absolute()) p = state.projectRoot / p;
    return p;
}

inline std::string relativePath(const EditorState& state, const std::filesystem::path& p) {
    std::error_code ec;
    auto rel = std::filesystem::relative(p, state.projectRoot, ec);
    return ec ? p.string() : rel.generic_string();
}

inline bool readTextFile(const std::filesystem::path& path, std::string& out, std::string* err = nullptr) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "Could not read " + path.string(); return false; }
    std::ostringstream ss; ss << f.rdbuf(); out = ss.str(); return true;
}

inline bool writeTextFile(const std::filesystem::path& path, const std::string& text, std::string* err = nullptr) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream f(path, std::ios::binary);
    if (!f) { if (err) *err = "Could not write " + path.string(); return false; }
    f << text; return true;
}

inline std::string escapeJson(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') {}
        else o += c;
    }
    return o;
}

inline std::string getString(const std::string& text, const std::string& key, const std::string& fallback = "") {
    std::string marker = "\"" + key + "\"";
    size_t p = text.find(marker); if (p == std::string::npos) return fallback;
    p = text.find(':', p); if (p == std::string::npos) return fallback;
    p = text.find('"', p); if (p == std::string::npos) return fallback;
    size_t e = text.find('"', p + 1); if (e == std::string::npos) return fallback;
    return text.substr(p + 1, e - p - 1);
}

inline float getFloat(const std::string& text, const std::string& key, float fallback = 0.0f) {
    std::string marker = "\"" + key + "\"";
    size_t p = text.find(marker); if (p == std::string::npos) return fallback;
    p = text.find(':', p); if (p == std::string::npos) return fallback;
    ++p; while (p < text.size() && std::isspace((unsigned char)text[p])) ++p;
    char* end = nullptr; float v = std::strtof(text.c_str() + p, &end);
    return end == text.c_str() + p ? fallback : v;
}

inline bool getBool(const std::string& text, const std::string& key, bool fallback = false) {
    std::string marker = "\"" + key + "\"";
    size_t p = text.find(marker); if (p == std::string::npos) return fallback;
    p = text.find(':', p); if (p == std::string::npos) return fallback;
    ++p; while (p < text.size() && std::isspace((unsigned char)text[p])) ++p;
    if (text.compare(p, 4, "true") == 0) return true;
    if (text.compare(p, 5, "false") == 0) return false;
    return fallback;
}

inline void getFloat3(const std::string& text, const std::string& key, float* out) {
    std::string marker = "\"" + key + "\"";
    size_t p = text.find(marker); if (p == std::string::npos) return;
    p = text.find('[', p); if (p == std::string::npos) return;
    for (int i=0;i<3;i++) {
        ++p; while (p < text.size() && (std::isspace((unsigned char)text[p]) || text[p] == ',')) ++p;
        char* end = nullptr; float v = std::strtof(text.c_str() + p, &end);
        if (end == text.c_str() + p) return;
        out[i] = v; p = (size_t)(end - text.c_str());
    }
}

inline void getFloat4(const std::string& text, const std::string& key, float* out) {
    std::string marker = "\"" + key + "\"";
    size_t p = text.find(marker); if (p == std::string::npos) return;
    p = text.find('[', p); if (p == std::string::npos) return;
    for (int i=0;i<4;i++) {
        ++p; while (p < text.size() && (std::isspace((unsigned char)text[p]) || text[p] == ',')) ++p;
        char* end = nullptr; float v = std::strtof(text.c_str() + p, &end);
        if (end == text.c_str() + p) return;
        out[i] = v; p = (size_t)(end - text.c_str());
    }
}

inline std::vector<std::filesystem::path> scanFiles(const EditorState& state, const std::vector<std::string>& exts) {
    std::vector<std::filesystem::path> out;
    std::error_code ec;
    if (!std::filesystem::exists(state.projectRoot, ec)) return out;
    for (auto it = std::filesystem::recursive_directory_iterator(state.projectRoot, ec); it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec || !it->is_regular_file(ec)) continue;
        std::string e = lower(it->path().extension().string());
        for (auto x : exts) if (e == lower(x)) { out.push_back(it->path()); break; }
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline void openInOs(const std::filesystem::path& path) {
#if defined(_WIN32)
    std::string cmd = "start \"\" \"" + path.string() + "\"";
#elif defined(__APPLE__)
    std::string cmd = "open \"" + path.string() + "\"";
#else
    std::string cmd = "xdg-open \"" + path.string() + "\" >/dev/null 2>&1 &";
#endif
    std::system(cmd.c_str());
}

inline std::string stableIdFor(const std::string& s) {
    unsigned long long h = 1469598103934665603ull;
    for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
    char buf[64]; std::snprintf(buf, sizeof(buf), "AX-%08llX-%08llX", (h >> 32) & 0xffffffffull, h & 0xffffffffull);
    return buf;
}

} // namespace axed
