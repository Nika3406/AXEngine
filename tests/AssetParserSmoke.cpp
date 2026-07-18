#include "AXEngine/Audio/SoundKit.h"
#include "AXEngine/VFX/VFXKit.h"

#include <iostream>

int main() {
    ax::sound::SoundRuntime sound;
    sound.setEchoToConsole(false);
    if (!sound.loadSoundFile("assets/audio/SwordHit.axsound")) {
        std::cerr << "Could not load SwordHit.axsound\n";
        return 1;
    }
    const auto* swordHit = sound.find("SwordHit");
    if (!swordHit || swordHit->clips.size() != 3) {
        std::cerr << "Expected exactly three SwordHit clips\n";
        return 2;
    }

    ax::vfx::VFXRuntime vfx;
    ax::vfx::VFXDefinition slash;
    if (!vfx.loadDefinition("assets/vfx/Slash_WideArc.axvfx", slash)) {
        std::cerr << "Could not load Slash_WideArc.axvfx\n";
        return 3;
    }
    if (slash.kind != ax::vfx::VFXKind::Mesh) {
        std::cerr << "Slash_WideArc must resolve to mesh VFX\n";
        return 4;
    }
    return 0;
}
