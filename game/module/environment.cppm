module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/world_environment.hpp>
ENABLE_WARNING

export module game.environment;

import misc.number;
import game.core;

using namespace godot;

export namespace craftbuild {
    class Sun final : public DirectionalLight3D {
        GDCLASS(Sun, DirectionalLight3D)

        f32 day_angle = -0.8f;
        f32 day_speed = 0.0005f;

    public:
        auto _ready() -> void override;
        auto _process(f64 delta) -> void override;

        static auto _bind_methods() -> void {};
    };

    class CraftSky final : public WorldEnvironment {
        GDCLASS(CraftSky, WorldEnvironment)

        Ref<Environment> environment;
        Ref<ProceduralSkyMaterial> sky_material;

    public:
        auto _ready() -> void override;

        static auto _bind_methods() -> void {};
    };
}