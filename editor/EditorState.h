#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <array>
#include <algorithm>


struct EditorProperty {
    std::string name;
    std::string value;
};

struct EditorComponent {
    std::string type = "Component";
    bool active = true;
    bool dynamic = false;
    std::vector<EditorProperty> properties;
};


struct EditorComponentPreset {
    std::string type;
    std::string category;
    std::string description;
};

struct EditorObject {
    std::string name;
    std::string type;
    std::string sourcePath;
    bool active = true;
    bool isStatic = false;
    int parentIndex = -1;
    std::vector<EditorComponent> components;
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3]    = {1.0f, 1.0f, 1.0f};

    // Snapshot of position/rotation/scale taken once when the object entered
    // the hierarchy (GLB bake time, or creation time for manually-added
    // objects). Used to work out how much an object (or its parent chain)
    // has been edited, so a parent's move/rotate/scale can be propagated
    // down to its children without re-baking anything.
    float basePosition[3] = {0.0f, 0.0f, 0.0f};
    float baseRotation[3] = {0.0f, 0.0f, 0.0f};
    float baseScale[3]    = {1.0f, 1.0f, 1.0f};
};

struct EditorLogLine {
    std::string text;
};

enum class EditorTransformTool {
    Translate,
    Rotate,
    Scale,
    LotRotScale
};

enum class EditorWorkspaceDocument {
    Scene,
    Asset,
    Attack
};

enum class EditorWorkspaceBottomPane {
    ProjectBrowser,
    Console
};

enum class EditorWorkspaceRightPane {
    Hierarchy,
    Inspector
};

enum class EditorWorkspaceLayoutMode {
    Auto,
    Compact,
    Standard,
    Wide
};

struct PreviewTransform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3]    = {1.0f, 1.0f, 1.0f};
};


struct EditorSnapshot {
    std::vector<EditorObject> hierarchy;
    int selectedObject = -1;
    int copiedObject = -1;
    std::filesystem::path selectedPath;

    std::vector<PreviewTransform> assetBoneTransforms;
    int selectedAssetBone = -1;

    std::string sceneGlbPath;
    std::filesystem::path currentScenePath;
    std::string sceneName;
    std::string sceneRenderProfile;
    bool sceneDirty = false;
    std::string assetGlbPath;
    std::string assetFilePath;
    std::string assetPreviewKind;
};

struct EditorState {
    std::filesystem::path projectRoot;
    std::filesystem::path selectedPath;
    int selectedObject = -1;
    int copiedObject = -1;

    // Multi-selection set (Ctrl/Shift-click in Hierarchy, or box/click-select
    // in Scene Preview). `selectedObject` remains the "primary"/last-clicked
    // object - used for the Inspector title, copy/duplicate, and everywhere
    // that only ever cared about a single object. `selectedObjects` is the
    // full set; single-select keeps it as a one-element mirror of
    // `selectedObject` so isSelected() below works uniformly either way.
    std::vector<int> selectedObjects;

    std::vector<EditorObject> hierarchy;

    // Read-only catalog of every node inside the currently-loaded GLB, for
    // browsing and picking things to Group. This is NEVER written to the
    // .axscene file - it exists purely so "Group Selected" has something to
    // select from without risking clobbering the real, authored scene
    // objects in `hierarchy` (StaticScene / PlayerStart / Enemy / NodeGroup
    // / NodeRef). Rebuilt wholesale every time the GLB is (re)loaded.
    std::vector<EditorObject> sceneBrowseNodes;
    std::vector<int> selectedBrowseNodes;
    std::vector<EditorLogLine> console;

    bool generateCppOnSave = true;
    bool showDemoScene = true;
    bool showGrid = true;
    bool snapEnabled = false;
    float snapStep = 0.25f;
    // User scale multiplies the operating system/display scale. A value of
    // 1.0 keeps the interface at the platform-recommended physical size.
    float uiScale = 1.0f;
    float displayScale = 1.0f;
    float readableFontPixels = 16.0f;
    bool followDisplayScale = true;

    bool showProjectBrowser = true;
    bool showHierarchy = true;
    bool showInspector = false;
    bool showSceneView = true;
    bool showAssetPreview = true;
    bool showConsole = true;
    bool showAttackColliderEditor = true;
    bool showCommandLauncher = false;
    bool showRenderMaterialEditor = false;
    bool showAnimationTimeline = false;
    bool showVFXEditor = false;
    bool showInputMapEditor = false;
    bool showAIEditor = false;
    bool showGameUIEditor = false;
    bool showAudioPreview = false;
    bool showDebugProfiler = false;
    bool showAssetRegistry = false;

    std::string commandSearch;
    std::string activeToolContext = "Scene";

    bool layoutInitialized = false;
    int windowWidth = 1500;
    int windowHeight = 900;

    // The adaptive workspace keeps one primary document, one right-side pane,
    // and one bottom pane visible. This avoids stacked/floating windows on
    // small laptops while preserving a manual free-window mode for power users.
    bool adaptiveWorkspace = true;
    EditorWorkspaceDocument workspaceDocument = EditorWorkspaceDocument::Scene;
    EditorWorkspaceBottomPane workspaceBottomPane = EditorWorkspaceBottomPane::ProjectBrowser;
    EditorWorkspaceRightPane workspaceRightPane = EditorWorkspaceRightPane::Hierarchy;
    EditorWorkspaceLayoutMode workspaceLayoutMode = EditorWorkspaceLayoutMode::Auto;

    // Manual/fallback layout ratios. These remain available when adaptive
    // workspace is disabled.
    float layoutLeftWidthRatio = 0.16f;
    float layoutRightWidthRatio = 0.23f;
    float layoutTopHeightRatio = 0.66f;
    float layoutSceneAssetSplitRatio = 0.56f;
    float layoutGap = 6.0f;
    bool layoutLocked = true;

    EditorTransformTool transformTool = EditorTransformTool::LotRotScale;
    bool detailsAdvancedMode = true;
    bool detailsShowGeneratedCppPreview = true;
    bool componentAuthoringMode = true;
    bool sceneAutoGenerateCode = true;
    bool sceneNeedsPreviewRebuild = true;
    bool sceneCompositionMode = true;
    bool scenePreviewLit = true;
    bool scenePreviewCel = false;
    bool scenePreviewRim = true;
    bool scenePreviewFog = true;
    bool scenePreviewBloom = true;
    bool scenePreviewWireframe = false;
    bool scenePreviewShowSelectedOnly = false;

    std::vector<EditorComponentPreset> componentPresets = {
        {"StaticMeshRenderer", "Rendering", "Draw a static GLB/mesh asset."},
        {"SkinnedMeshRenderer", "Rendering", "Draw a rigged GLB and expose animation clips/skeleton."},
        {"Camera", "Rendering", "Camera component."},
        {"DirectionalLight", "Rendering", "Sun/parallel light component."},
        {"BoxCollider", "Physics", "Box collider volume."},
        {"MeshCollider", "Physics", "Mesh collider, often from COLLIDER_* import."},
        {"TriggerVolume", "Gameplay", "Non-solid volume for events or encounters."},
        {"EnemyAI", "Gameplay", "Enemy behavior and stats."},
        {"GunnerAim", "Gameplay", "Procedural upper-body/gun aim metadata."},
        {"ProjectileEmitter", "Gameplay", "Projectile fire point/cooldown/speed."},
        {"ScriptComponent", "Gameplay", "User-owned script hook; editor never overwrites script code."},
        {"SocketBinding", "Animation", "Named socket/helper bound to a bone."},
        {"SwordTrail", "Animation", "DMC-style trail between two sockets/helper bones."},
        {"ProceduralAnimator", "Animation", "Aim/look/recoil/additive procedural animation layer."},
        {"HitboxTimeline", "Combat", "Hit frames, cancel windows, invulnerability and events."},
        {"AttackCollider", "Combat", "Authored big attack volume for one move: box/sphere/capsule/cylinder, direction, size, timing, damage."},
        {"Hurtbox", "Combat", "Target volume that receives attacks."},
        {"CombatActor", "Combat", "Actor combat state: faction, stagger, invulnerability, hit memory."},
        {"ComboMove", "Combat", "Move definition used by combo/action system."},
        {"LODGroup", "Optimization", "Switch meshes/settings by camera distance."},
        {"VoxelTerrain", "Voxel", "Chunked procedural voxel terrain settings."},
        {"ProceduralGenerator", "Procedural", "Seed/rule-based scene generation component."},
        {"NavRegion", "AI", "AI navigation/patrol/encounter region."}
    };



    // Stage 2.4 focused-engine tool state. AXEngine remains GLB-first and
    // editor/runtime-focused; Blender and DAW tools stay outside the engine.
    std::string materialEditorPath = "assets/materials/Default_Realistic.axmat";
    std::string materialShaderVert = "assets/shaders/stylized_lit.vert";
    std::string materialShaderFrag = "assets/shaders/stylized_lit.frag";
    std::string materialRenderMode = "Stylized";
    bool materialUseCel = false;
    bool materialUseOutline = false;
    bool materialUseBloom = true;
    float materialBaseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    float materialEmission[3] = {0.0f, 0.0f, 0.0f};
    float materialRoughness = 0.55f;
    float materialMetallic = 0.0f;

    std::string animationTimelineClip = "Attack_01";
    float animationTimelineFps = 60.0f;
    float animationTimelineDuration = 1.2f;
    float animationTimelineScrub = 0.0f;
    bool animationTimelineLoop = true;

    std::string vfxEditorPath = "assets/vfx/Slash_WideArc.axvfx";
    std::string vfxMesh = "assets/vfx/slash_arc.glb";
    std::string vfxMaterial = "assets/materials/Slash_Additive.axmat";
    float vfxLifetime = 0.22f;
    float vfxScale[3] = {2.8f, 1.2f, 1.0f};
    bool vfxBillboard = false;
    bool vfxBloom = true;

    std::string inputMapPath = "assets/input/Default.axinput";
    bool inputBufferEnabled = true;
    float inputBufferSeconds = 0.18f;
    bool inputControllerEnabled = true;

    std::string aiEditorPath = "assets/enemies/Gunner.axenemy";
    float aiDetectionRange = 18.0f;
    float aiPreferredRange = 9.0f;
    float aiAttackCooldown = 1.4f;

    std::string gameUiPath = "assets/ui/HUD.axui";
    bool gameUiShowHealth = true;
    bool gameUiShowStyle = true;
    bool gameUiShowLockOn = true;

    std::string audioPreviewPath = "assets/audio/attack_swing.wav";
    bool audioPreviewLoop = false;
    float audioPreviewVolume = 0.8f;

    bool debugShowColliders = true;
    bool debugShowHurtboxes = true;
    bool debugShowInputBuffer = true;
    bool debugShowAiState = true;
    bool debugShowFrameProfiler = true;

    // Scene Preview interaction state. This is not a fake placeholder anymore:
    // it behaves like a tiny editor viewport while Stage 2 wires in real FBO rendering.
    float sceneZoom = 22.0f;
    float scenePan[2] = {0.0f, 0.0f};
    bool sceneShowObjects = true;
    bool sceneShowGizmos = true;
    bool sceneShowLighting = true;
    bool sceneShowCollectionObjects = true;
    bool sceneShowCollectionColliders = false;
    bool sceneShowCollectionMat = true;
    bool sceneShowCollectionBloom = true;
    bool sceneShowSelectionOutline = true;
    bool sceneShowAxisGizmo = true;
    float sceneCameraFocus[3] = {0.0f, 0.0f, 0.0f};
    float sceneFlySpeed = 7.5f;
    bool sceneMouseDraggingObject = false;
    bool sceneUseRealRender = true;
    bool sceneShowSky = true;
    bool sceneFrameRealScene = true;
    bool sceneSyncMarkersFromGlb = true;
    bool sceneDrawGlbMarkersOnly = false;
    bool sceneUseLoadedGlbNodesAsHierarchy = true;
    bool sceneAutoOpenSelectedSceneGlb = true;
    int sceneSelectedPreviewNode = -1;
    std::string sceneMarkerSourceGlb;
    float sceneCameraYaw = -35.0f;
    float sceneCameraPitch = 24.0f;
    float sceneCameraDistance = 80.0f;
    std::string sceneGlbPath = "assets/Scene1.glb";
    std::filesystem::path currentScenePath = "assets/scenes/Scene1.axscene";
    std::string sceneName = "Scene1";
    std::string sceneRenderProfile = "StylizedAction";
    float sceneGravity = -24.0f;
    float sceneUnitScale = 1.0f;
    bool sceneDirty = false;


    // Attack Collider Editor: edits assets/combat/attacks/*.axattack.
    std::filesystem::path attackEditorPath = "assets/combat/attacks/SwordAttack1.axattack";
    std::string attackEditorName = "SwordAttack1";
    int attackEditorSelectedCollider = 0;
    bool attackEditorDirty = false;
    bool attackEditorPreviewActiveOnly = false;
    float attackEditorScrubTime = 0.18f;
    float attackEditorDuration = 0.62f;
    std::string attackEditorStatus = "Ready";

    // Asset Preview interaction state. This is for one GLB/mesh/rig target,
    // separate from the full scene viewport.
    PreviewTransform assetPreviewTransform;
    float assetZoom = 1.0f;
    float assetPan[2] = {0.0f, 0.0f};
    bool assetShowMesh = true;
    bool assetShowSkeleton = true;
    bool assetShowSockets = true;
    bool assetMouseDraggingBone = false;
    bool assetUseRealRender = true;
    bool assetShowSky = true;
    bool assetFrameModel = true;
    float assetCameraYaw = -20.0f;
    float assetCameraPitch = 12.0f;
    float assetCameraDistance = 6.5f;
    std::string assetGlbPath = "assets/Man1_Cinema.glb";
    std::string assetFilePath = "assets/Man1_Cinema.glb";
    std::string assetPreviewKind = "GLB"; // GLB, Image, Code, Unknown
    bool assetShowSelectionOutline = true;
    bool assetShowAxisGizmo = true;
    float assetCameraFocus[3] = {0.0f, 0.0f, 0.0f};
    float assetFlySpeed = 2.5f;
    int selectedAssetBone = -1;
    int assetSelectedPreviewNode = -1;
    bool assetAutoOpenSelectedGlb = true;

    std::vector<std::string> assetPreviewBones = {
        "Root", "Hips", "Spine", "Chest", "Neck", "Head",
        "RightUpperArm", "RightForeArm", "RightHand", "Gun", "Muzzle",
        "LeftUpperArm", "LeftForeArm", "LeftHand",
        "RightThigh", "RightShin", "RightFoot",
        "LeftThigh", "LeftShin", "LeftFoot"
    };
    std::vector<PreviewTransform> assetBoneTransforms;

    std::vector<EditorSnapshot> undoStack;
    std::vector<EditorSnapshot> redoStack;
    std::string lastCommittedEditSignature;
    std::string editDragStartSignature;
    EditorSnapshot editDragStartSnapshot;
    EditorSnapshot lastCommittedSnapshot;
    bool editDragInProgress = false;
    int maxUndoSteps = 64;

    void log(const std::string& message) {
        if (!console.empty() && console.back().text == message) return;
        console.push_back({message});
    }
};

// --- Multi-selection helpers -------------------------------------------
// Kept as free functions (rather than EditorState methods) so every panel
// that touches selection - Hierarchy, Scene Preview, Inspector - goes
// through the same rules instead of hand-rolling index bookkeeping.

inline bool isSelected(const EditorState& state, int index) {
    for (int i : state.selectedObjects) if (i == index) return true;
    return false;
}

inline void selectSingle(EditorState& state, int index) {
    state.selectedObject = index;
    state.selectedObjects.clear();
    if (index >= 0) state.selectedObjects.push_back(index);
}

inline void clearSelection(EditorState& state) {
    state.selectedObject = -1;
    state.selectedObjects.clear();
}

inline void toggleSelect(EditorState& state, int index) {
    if (index < 0) return;
    auto it = std::find(state.selectedObjects.begin(), state.selectedObjects.end(), index);
    if (it != state.selectedObjects.end()) {
        state.selectedObjects.erase(it);
        state.selectedObject = state.selectedObjects.empty() ? -1 : state.selectedObjects.back();
    } else {
        state.selectedObjects.push_back(index);
        state.selectedObject = index;
    }
}

// Shift-click range select. Range is taken over hierarchy array order (not
// visual tree order/depth, which would be ambiguous once nesting is
// involved) - matches how most tools resolve this in practice.
inline void rangeSelect(EditorState& state, int anchorIndex, int toIndex) {
    if (anchorIndex < 0) { selectSingle(state, toIndex); return; }
    int a = std::min(anchorIndex, toIndex), b = std::max(anchorIndex, toIndex);
    for (int i = a; i <= b; ++i) {
        if (!isSelected(state, i)) state.selectedObjects.push_back(i);
    }
    state.selectedObject = toIndex;
}

// Called wherever an index is being removed from state.hierarchy, so the
// selection set doesn't end up pointing at the wrong (shifted) objects.
inline void fixupSelectionAfterErase(EditorState& state, int erasedIndex) {
    for (auto it = state.selectedObjects.begin(); it != state.selectedObjects.end(); ) {
        if (*it == erasedIndex) it = state.selectedObjects.erase(it);
        else { if (*it > erasedIndex) *it -= 1; ++it; }
    }
    if (state.selectedObject == erasedIndex) {
        state.selectedObject = state.selectedObjects.empty() ? -1 : state.selectedObjects.back();
    } else if (state.selectedObject > erasedIndex) {
        state.selectedObject -= 1;
    }
}
