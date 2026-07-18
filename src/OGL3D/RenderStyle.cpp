#include "OGL3D/RenderStyle.h"

RenderStyle RenderStyle::DefaultStylized() {
    RenderStyle style;
    style.syncFeaturesFromLegacyFlags();
    return style;
}

RenderStyle RenderStyle::ShadowColossusLike() {
    return DefaultStylized();
}

RenderStyle RenderStyle::AsuraLike() {
    return DefaultStylized();
}

RenderStyle RenderStyle::DarkCelestialBrutalism() {
    return DefaultStylized();
}
