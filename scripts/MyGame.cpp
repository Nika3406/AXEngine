// scripts/MyGame.cpp
#include "core/App.hpp"
#include "core/GameEntry.hpp"
#include "ecs/Components.hpp"
#include "ecs/Systems.hpp"
#include "ecs/EntityBuilder.hpp"

#include <iostream>
#include <cmath>
#include <memory>
#include <algorithm>

// ============================================================================
// My Game - Clean, Minimal API Example
// ============================================================================
class MyGame : public core::App {
public:
    MyGame()
    : App([]{
        core::App::Config cfg("My Game Engine - FPS Demo", 1280, 720, true, false);
        cfg.fullscreen = true;
        return cfg;
    }()) {}

    void onInit() override {
        std::cout << "=== Initializing Game ===\n";

        auto& assets = getAssets();

        // Load shaders
        m_program = assets.loadProgram(
            "mesh_shader",
            "shaders/compiled/spirv/vs_mesh.bin",
            "shaders/compiled/spirv/fs_mesh.bin"
        );

        if (!bgfx::isValid(m_program)) {
            std::cerr << "CRITICAL ERROR: Shaders failed to load!\n";
            return;
        }

        // ====================================================================
        // Load Meshes (from OBJ, FBX, BLEND, etc.)
        // ====================================================================
        // Example: assets.loadMesh("house", "assets/models/house.obj");
        // Example: assets.loadMesh("tree", "assets/models/tree.fbx");
        // Example: assets.loadMesh("car", "assets/models/car.blend");
        
        // For demo purposes, still create a couple primitives
        assets.createCubeMesh("cube", 1.0f);
        assets.createPlaneMesh("ground", 300.0f, 300.0f);

        // ====================================================================
        // Scene Setup with Builder Pattern
        // ====================================================================
        ecs::Scene scene(getRegistry(), assets, m_program);

        // Player with camera and capsule collider (better for FPS/TPS)
        m_player = scene.create()
            .tag("Player")
            .position(0.0f, 1.7f, 8.0f)
            .capsuleCollider(0.35f, 1.7f)  // radius, height - smooth stair climbing!
            .camera(75.0f, 0.05f, 2000.0f)
            .lookAt(0.0f, 0.0f, -1.0f)
            .get();

        // Ground plane
        scene.create()
            .tag("Ground")
            .position(0.0f, -0.5f, 0.0f)
            .mesh("ground")
            .color(0.25f, 0.75f, 0.25f)
            .boxCollider(150.0f, 0.5f, 150.0f)
            .get();

        // ====================================================================
        // Spawn Objects - Full Control with Builder Pattern
        // ====================================================================

        // Cube #1 - custom position, scale, rotation, color
        scene.create()
            .tag("RedCube")
            .mesh("cube")
            .position(0.0f, 0.5f, 0.0f)
            .scale(1.0f)
            .rotation(0.0f, 0.785f, 0.0f)  // 45 degrees Y rotation
            .color(1.0f, 0.2f, 0.2f)
            .autoBoxCollider()  // Auto-generate from mesh bounds
            .get();

        // Cube #2 - different scale
        scene.create()
            .tag("BlueCube")
            .mesh("cube")
            .position(3.0f, 0.75f, -5.0f)
            .scale(1.5f, 0.5f, 1.5f)  // Non-uniform scale
            .color(0.2f, 0.6f, 1.0f)
            .autoBoxCollider()
            .get();

        // Cube #3 - with custom collider (larger than visual)
        scene.create()
            .tag("YellowCube")
            .mesh("cube")
            .position(-4.0f, 0.5f, -8.0f)
            .scale(1.0f)
            .color(1.0f, 1.0f, 0.2f)
            .boxCollider(0.75f, 0.75f, 0.75f)  // Custom collider size
            .get();

        // Cube #4 - sphere collider instead of box
        scene.create()
            .tag("PurpleCube")
            .mesh("cube")
            .position(6.0f, 0.5f, -2.0f)
            .scale(1.2f)
            .color(0.9f, 0.4f, 1.0f)
            .sphereCollider(0.8f)  // Use sphere collider
            .get();

        // ====================================================================
        // Example: Load and spawn custom mesh from file
        // ====================================================================
        /*
        // Load your custom model
        assets.loadMesh("myModel", "assets/models/myModel.obj");
        
        // Spawn it in the scene
        scene.create()
            .tag("CustomObject")
            .mesh("myModel")
            .position(10.0f, 0.0f, -10.0f)
            .scale(2.0f)
            .rotation(0.0f, 1.57f, 0.0f)  // 90 degrees
            .color(0.8f, 0.8f, 0.8f)
            .autoBoxCollider()  // Or use .boxCollider() / .sphereCollider()
            .get();
        */

        // Grab mouse for FPS controls
        setMouseGrabbed(true);

        std::cout << "Controls: WASD move | Mouse look | Q/E up/down | R reset | ESC quit\n";
        std::cout << "Collision: capsule player vs world AABBs (smooth character movement!)\n";
        std::cout << "\nTo add custom meshes:\n";
        std::cout << "  assets.loadMesh(\"name\", \"path/to/model.obj\");\n";
        std::cout << "  scene.create().mesh(\"name\").position(x,y,z).color(r,g,b).autoBoxCollider();\n";
    }

    void onUpdate(float dt) override {
        auto& reg = getRegistry();

        // Mouse look
        int mouseDx, mouseDy;
        getMouseDelta(mouseDx, mouseDy);
        
        // Debug: Print mouse delta (remove this after testing)
        if (mouseDx != 0 || mouseDy != 0) {
            std::cout << "Mouse delta: dx=" << mouseDx << " dy=" << mouseDy << "\n";
        }
        
        m_yawDeg   += mouseDx * m_mouseSensitivity;
        m_pitchDeg -= mouseDy * m_mouseSensitivity;
        m_pitchDeg = std::clamp(m_pitchDeg, -89.0f, 89.0f);

        // Calculate direction vectors
        bx::Vec3 forward = computeForward();
        bx::Vec3 right   = computeRight(forward);

        // Build movement vector from continuous key states
        bx::Vec3 moveVec{0.0f, 0.0f, 0.0f};
        
        if (isKeyDown('w') || isKeyDown('W')) { moveVec.x += forward.x; moveVec.z += forward.z; }
        if (isKeyDown('s') || isKeyDown('S')) { moveVec.x -= forward.x; moveVec.z -= forward.z; }
        if (isKeyDown('d') || isKeyDown('D')) { moveVec.x -= right.x;   moveVec.z -= right.z;   }
        if (isKeyDown('a') || isKeyDown('A')) { moveVec.x += right.x;   moveVec.z += right.z;   }
        if (isKeyDown('q') || isKeyDown('Q')) moveVec.y -= 1.0f;
        if (isKeyDown('e') || isKeyDown('E')) moveVec.y += 1.0f;

        // Normalize movement vector
        float len = std::sqrt(moveVec.x*moveVec.x + moveVec.y*moveVec.y + moveVec.z*moveVec.z);
        if (len > 0.0001f) {
            float move = m_moveSpeed * dt;
            moveVec.x /= len; moveVec.y /= len; moveVec.z /= len;
            m_pos.x += moveVec.x * move;
            m_pos.y += moveVec.y * move;
            m_pos.z += moveVec.z * move;
        }

        // Update player transform and camera
        if (reg.valid(m_player)) {
            auto& t = reg.get<ecs::Transform>(m_player);
            t.position = m_pos;

            auto& cam = reg.get<ecs::Camera>(m_player);
            cam.target = forward;
        }

        // Collision resolution (player vs world)
        ecs::CollisionSystem::resolvePlayerVsWorld(reg, m_player);

        // Sync position back after collision
        if (reg.valid(m_player)) {
            m_pos = reg.get<ecs::Transform>(m_player).position;
        }

        // Update camera system
        ecs::CameraSystem::update(reg, getRenderer().getAspect());
    }

    void onRender() override {
        float view[16], proj[16];
        auto& reg = getRegistry();
        auto& renderer = getRenderer();

        if (ecs::CameraSystem::getActiveCameraMatrices(reg, renderer.getAspect(), view, proj)) {
            ecs::RenderSystem::render(reg, view, proj, 0);
        }
    }

    void onKeyPress(int key) override {
        // Only handle special actions
        if (key == 'r' || key == 'R') {
            m_yawDeg = 180.0f;
            m_pitchDeg = 0.0f;
        }
    }

private:    bx::Vec3 computeForward() const {
        const float yaw   = degToRad(m_yawDeg);
        const float pitch = degToRad(m_pitchDeg);

        bx::Vec3 f{0.0f, 0.0f, 0.0f};
        f.x = std::cos(pitch) * std::sin(yaw);
        f.y = std::sin(pitch);
        f.z = std::cos(pitch) * std::cos(yaw);

        float len = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
        if (len > 0.0001f) { f.x/=len; f.y/=len; f.z/=len; }
        return f;
    }

    bx::Vec3 computeRight(const bx::Vec3& forward) const {
        const bx::Vec3 up{0.0f, 1.0f, 0.0f};
        bx::Vec3 r{0.0f, 0.0f, 0.0f};
        r.x = forward.y * up.z - forward.z * up.y;
        r.y = forward.z * up.x - forward.x * up.z;
        r.z = forward.x * up.y - forward.y * up.x;

        float len = std::sqrt(r.x*r.x + r.y*r.y + r.z*r.z);
        if (len > 0.0001f) { r.x/=len; r.y/=len; r.z/=len; }
        return r;
    }

    static float degToRad(float d) { return d * 3.1415926535f / 180.0f; }

private:
    bgfx::ProgramHandle m_program = BGFX_INVALID_HANDLE;
    entt::entity m_player{ entt::null };

    bx::Vec3 m_pos{0.0f, 1.7f, 8.0f};
    float m_yawDeg   = 180.0f;
    float m_pitchDeg = 0.0f;

    const float m_moveSpeed = 7.5f;
    const float m_mouseSensitivity = 0.1f;  // Increased from 0.15
};

// User entrypoint
std::unique_ptr<core::App> CreateGame() {
    return std::make_unique<MyGame>();
}