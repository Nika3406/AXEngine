// Example only. This shows the intended future user-script style.
// The script asks engine kits for behavior instead of owning SDL/collision/combo internals.

#include "AXEngine/AXActionKit.h"

class MinimalActionPlayerController : public ax::PlayerController {
public:
    void onAction(const ax::InputActionEvent& e) override {
        if (e.action == "Attack" && e.phase == ax::InputPhase::Started) {
            ax::Logger::log("User script requested next attack combo.");
            // Future target:
            // combat.requestNextCombo();
        }

        if (e.action == "Dash" && e.phase == ax::InputPhase::Started) {
            ax::Logger::log("User script requested dash.");
            // Future target:
            // movement.dash();
        }
    }
};
