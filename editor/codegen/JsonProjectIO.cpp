#include "JsonProjectIO.h"
#include <filesystem>
#include <fstream>

static void ensureParentDirectory(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
}

bool JsonProjectIO::writeScene(const std::filesystem::path& outputPath, const std::vector<EditorObject>& objects) {
    ensureParentDirectory(outputPath);
    std::ofstream out(outputPath);
    if (!out) return false;

    out << "{\n";
    out << "  \"scene\": \"Scene1\",\n";
    out << "  \"sourceGlb\": \"assets/Scene1.glb\",\n";
    out << "  \"renderProfile\": \"StylizedAction\",\n";
    out << "  \"objects\": [\n";
    for (size_t i = 0; i < objects.size(); ++i) {
        const auto& o = objects[i];
        out << "    {\n";
        out << "      \"name\": \"" << o.name << "\",\n";
        out << "      \"type\": \"" << o.type << "\",\n";
        out << "      \"sourcePath\": \"" << o.sourcePath << "\",\n";
        out << "      \"position\": [" << o.position[0] << ", " << o.position[1] << ", " << o.position[2] << "],\n";
        out << "      \"rotation\": [" << o.rotation[0] << ", " << o.rotation[1] << ", " << o.rotation[2] << "],\n";
        out << "      \"scale\": [" << o.scale[0] << ", " << o.scale[1] << ", " << o.scale[2] << "]\n";
        out << "    }" << (i + 1 < objects.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

bool JsonProjectIO::writeDefaultGunner(const std::filesystem::path& outputPath) {
    ensureParentDirectory(outputPath);
    std::ofstream out(outputPath);
    if (!out) return false;
    out << "{\n";
    out << "  \"type\": \"AXEnemy\",\n";
    out << "  \"assetId\": \"enemy-gunner-default\",\n";
    out << "  \"version\": 2,\n";
    out << "  \"archetype\": \"Gunner\",\n";
    out << "  \"model\": \"assets/Man1_Cinema.glb\",\n";
    out << "  \"hp\": 160,\n";
    out << "  \"radius\": 0.48,\n";
    out << "  \"height\": 1.85,\n";
    out << "  \"detectionRange\": 18.0,\n";
    out << "  \"preferredRange\": 6.5,\n";
    out << "  \"attackCooldown\": 1.4,\n";
    out << "  \"moveSpeed\": 2.65,\n";
    out << "  \"states\": [\"Idle\", \"Chase\", \"Strafe\", \"Attack\", \"Recover\", \"Stagger\"],\n";
    out << "  \"attacks\": [\n";
    out << "    {\"name\":\"BasicPunch\", \"range\":1.55, \"cooldown\":1.25, \"windup\":0.32, \"activeEnd\":0.52, \"damage\":10}\n";
    out << "  ]\n";
    out << "}\n";
    return true;
}

bool JsonProjectIO::writeDefaultBloomBlade(const std::filesystem::path& outputPath) {
    ensureParentDirectory(outputPath);
    std::ofstream out(outputPath);
    if (!out) return false;
    out << "{\n";
    out << "  \"type\": \"AXMaterial\",\n";
    out << "  \"assetId\": \"mat-bloom-blade\",\n";
    out << "  \"version\": 1,\n";
    out << "  \"renderMode\": \"Stylized\",\n";
    out << "  \"vertexShader\": \"assets/shaders/stylized_lit.vert\",\n";
    out << "  \"fragmentShader\": \"assets/shaders/stylized_lit.frag\",\n";
    out << "  \"baseColor\": [1.0, 1.0, 1.0, 1.0],\n";
    out << "  \"roughness\": 0.35,\n";
    out << "  \"metallic\": 0.15,\n";
    out << "  \"emission\": [1.0, 1.0, 1.0],\n";
    out << "  \"cel\": true,\n";
    out << "  \"outline\": false,\n";
    out << "  \"bloom\": true\n";
    out << "}\n";
    return true;
}
