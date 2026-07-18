#include "AXEngine/AXActionKit.h"
#include "AXEngine/AXCharacterKit.h"
#include "AXEngine/Gameplay/Events.h"
#include "AXEngine/GameFramework/PlayerController.h"

// This is the direction user code should move toward: gameplay decisions use
// public AXEngine modules, while low-level SDL/collision/render details stay in
// engine runtime kits.
class ModularActionPlayerController : public ax::PlayerController {
public:
    ax::character::CharacterControllerSettings movement;
    ax::character::ThirdPersonCameraSettings camera;
    ax::action::ComboRuntime combos;

    void onAction(const ax::InputActionEvent& e) override {
        if (e.action == "Attack") {
            ax::Logger::log(std::string("User script requests combat action via ") + ax::events::AttackStarted);
        }
        if (e.action == "Dash") {
            ax::Logger::log("User script requests dash");
        }
    }
};
