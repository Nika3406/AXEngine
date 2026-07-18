#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class EditorPreviewRenderer {
public:
    EditorPreviewRenderer() = default;
    ~EditorPreviewRenderer();

    bool initialize();
    bool loadGlb(const std::filesystem::path& path, std::string* errorOut = nullptr);
    void clearModel();

    struct SceneMarker {
        std::string name;
        std::string type;
        std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> rotation = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};
    };

    struct PreviewNode {
        int index = -1;
        int parentIndex = -1;
        int objectIndex = -1;
        std::string name;
        std::string type = "Node";
        bool hasMesh = false;
        bool isJoint = false;
        bool isMarker = false;
        std::array<float, 3> position = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> rotation = {0.0f, 0.0f, 0.0f};
        std::array<float, 3> scale = {1.0f, 1.0f, 1.0f};
    };

    struct SceneInstanceDesc {
        std::string name;
        std::string type;
        std::filesystem::path path;
        bool active = true;
        bool selected = false;
        float position[3] = {0.0f, 0.0f, 0.0f};
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        float scale[3] = {1.0f, 1.0f, 1.0f};
    };

    bool loadSceneInstances(const std::vector<SceneInstanceDesc>& instances,
                            const std::filesystem::path& projectRoot,
                            std::string* errorOut = nullptr);

    // Live transform override for a node, looked up by node name (matches
    // EditorObject::name / PrimitiveGL::nodeName). Call this every frame with
    // the Inspector's current Position/Rotation/Scale for each hierarchy
    // object *before* calling renderToTexture(). Values are applied as a
    // delta against the transform the node was originally baked/loaded with,
    // so unedited nodes render exactly as before (zero regression) and
    // edited nodes move/rotate/scale live without re-parsing the GLB.
    // Only Y-axis rotation is supported, matching the rotation the
    // Inspector/hierarchy already track for GLB nodes.
    void setInstanceTransform(const std::string& nodeName,
                               const float position[3],
                               const float rotation[3],
                               const float scale[3]);
    void clearInstanceTransforms() { instanceTransforms_.clear(); }

    unsigned int renderToTexture(int width, int height, float yawDeg, float pitchDeg, float distance,
                                 bool showGrid, bool showSky, bool showSkeleton, bool frameModel,
                                 const float* focusOffset = nullptr,
                                 bool showObjects = true, bool showColliders = true,
                                 bool showMatObjects = true, bool showBloomObjects = true,
                                 bool lit = true, bool cel = false, bool rim = false,
                                 bool fog = false, bool bloomPreview = false,
                                 bool wireframe = false, bool selectedOnly = false);

    bool loadImage(const std::filesystem::path& path, std::string* errorOut = nullptr);
    unsigned int imageTexture() const { return imageTexture_; }
    int imageWidth() const { return imageWidth_; }
    int imageHeight() const { return imageHeight_; }
    bool imageLoaded() const { return imageLoaded_; }

    bool worldToScreen(float worldX, float worldY, float worldZ,
                       int width, int height, float yawDeg, float pitchDeg, float distance,
                       bool frameModel, float& outX, float& outY, const float* focusOffset = nullptr) const;

    float suggestedCameraDistance(float fovYDeg = 58.0f) const;
    void framedCameraPosition(float yawDeg, float pitchDeg, float distance, bool frameModel, float* outPosition) const;

    bool loaded() const { return loaded_; }
    const std::filesystem::path& loadedPath() const { return loadedPath_; }
    const std::string& loadedName() const { return loadedName_; }
    const std::vector<std::string>& nodeNames() const { return nodeNames_; }
    const std::vector<std::string>& materialNames() const { return materialNames_; }
    const std::vector<SceneMarker>& sceneMarkers() const { return sceneMarkers_; }
    const std::vector<PreviewNode>& previewNodes() const { return nodes_; }
    int triangleCount() const { return triangleCount_; }
    int primitiveCount() const { return primitiveCount_; }
    int nodeCount() const { return static_cast<int>(nodes_.size()); }
    int boneCount() const;

private:
    struct MaterialGL {
        std::string name;
        float baseColor[4] = {0.75f, 0.75f, 0.75f, 1.0f};
        unsigned int texture = 0;
        bool hasTexture = false;
    };

    struct PrimitiveGL {
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int indexCount = 0;
        int vertexCount = 0;
        int materialIndex = 0;
        int objectIndex = -1;
        bool indexed = false;
        bool selected = false;
        std::string objectName;
        std::string nodeName;
        std::string meshName;
        std::string materialName;
        std::string collection = "Object";

        // Baseline transform this primitive's vertices were baked with (world
        // space, at load time). Live edits are applied as deltas against
        // these baselines in drawModel().
        float baseWorldPos[3] = {0.0f, 0.0f, 0.0f};
        float baseYawDeg = 0.0f;
        float baseWorldScale[3] = {1.0f, 1.0f, 1.0f};

        // Baseline transform of the owning scene/asset object. This is the
        // important distinction for parent scaling: Scene1 / Environment is
        // the GLB-instance root, while nodeName is a child mesh node inside
        // that GLB. Scaling the root must apply once around the root pivot,
        // not once per child node pivot.
        float objectBasePos[3] = {0.0f, 0.0f, 0.0f};
        float objectBaseYawDeg = 0.0f;
        float objectBaseScale[3] = {1.0f, 1.0f, 1.0f};
    };

    struct InstanceTransform {
        float position[3] = {0.0f, 0.0f, 0.0f};
        float rotation[3] = {0.0f, 0.0f, 0.0f};
        float scale[3]    = {1.0f, 1.0f, 1.0f};
    };

    bool makeProgram();
    bool makeFramebuffer(int width, int height);
    void destroyFramebuffer();
    void destroyModel();
    void destroyImage();
    void drawGrid(const float* vp);
    void drawModel(const float* vp, bool selectedOnly);
    void drawSkeleton(const float* vp);
    void drawSky(int width, int height, bool showSky);
    void buildViewProjection(int width, int height, float yawDeg, float pitchDeg, float distance, bool frameModel, float* outVP, const float* focusOffset = nullptr) const;
    bool appendGlb(const std::filesystem::path& path, const SceneInstanceDesc* instance, int objectIndex, std::string* errorOut);

    unsigned int program_ = 0;
    unsigned int gridProgram_ = 0;
    unsigned int fbo_ = 0;
    unsigned int colorTex_ = 0;
    unsigned int depthRbo_ = 0;
    int fbWidth_ = 0;
    int fbHeight_ = 0;
    unsigned int imageTexture_ = 0;
    int imageWidth_ = 0;
    int imageHeight_ = 0;
    bool imageLoaded_ = false;

    bool showObjects_ = true;
    bool showColliders_ = true;
    bool showMatObjects_ = true;
    bool showBloomObjects_ = true;
    bool lit_ = true;
    bool cel_ = false;
    bool rim_ = false;
    bool fog_ = false;
    bool bloomPreview_ = false;

    bool initialized_ = false;
    bool loaded_ = false;
    std::filesystem::path loadedPath_;
    std::string loadedName_;
    std::vector<PrimitiveGL> primitives_;
    std::vector<MaterialGL> materials_;
    std::vector<std::string> nodeNames_;
    std::vector<std::string> materialNames_;
    std::vector<SceneMarker> sceneMarkers_;
    std::vector<PreviewNode> nodes_;

    // Live per-node transform overrides, keyed by node name. Populated by
    // setInstanceTransform() from the Inspector/hierarchy every frame.
    std::unordered_map<std::string, InstanceTransform> instanceTransforms_;

    float minBounds_[3] = {-1.0f, -1.0f, -1.0f};
    float maxBounds_[3] = { 1.0f,  1.0f,  1.0f};
    int triangleCount_ = 0;
    int primitiveCount_ = 0;
};
