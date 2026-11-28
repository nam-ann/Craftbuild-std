module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/vector3.hpp>
ENABLE_WARNING

module game.environment;

namespace craftbuild {
    static constexpr auto SUN_YAW = -0.75f;
    static constexpr auto SUN_ENERGY = 1.0f;
    static constexpr auto SUN_ANGLE_STEP = 0.002f;

    static constexpr auto SKY_ENERGY = 1.0f;
    static constexpr auto BG_ENERGY = 1.0f;

    static constexpr auto SHADOW_DISTANCE = 292.0f;
    static constexpr auto FOG_BEGIN_RATIO = 8.65f;
    static constexpr auto FOG_END_RATIO = 16.95f;

    auto CraftSky::_ready() -> void {
        sky_material.instantiate();
        sky_material->set_sky_curve(0.35f);
        sky_material->set_sky_energy_multiplier(SKY_ENERGY);
        sky_material->set_ground_curve(0.1f);
        sky_material->set_ground_energy_multiplier(1.0f);
        sky_material->set_sun_angle_max(14.0f);
        sky_material->set_sun_curve(0.08f);
        sky_material->set_use_debanding(true);

        auto sky = Ref<Sky>();
        sky.instantiate();
        sky->set_radiance_size(Sky::RADIANCE_SIZE_256);
        sky->set_process_mode(Sky::PROCESS_MODE_REALTIME);
        sky->set_material(sky_material);

        environment.instantiate();
        environment->set_background(Environment::BG_SKY);
        environment->set_sky(sky);
        environment->set_bg_energy_multiplier(BG_ENERGY);
        environment->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
        environment->set_ambient_light_color(Color(0.18f, 0.22f, 0.35f));
        environment->set_reflection_source(Environment::REFLECTION_SOURCE_SKY);
        environment->set_tonemapper(Environment::TONE_MAPPER_LINEAR);
        environment->set_tonemap_exposure(1.0f);

        environment->set_adjustment_enabled(true);
        environment->set_adjustment_saturation(1.08f);
        environment->set_adjustment_contrast(1.0f);

        environment->set_glow_enabled(true);
        environment->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_SOFTLIGHT);
        environment->set_glow_intensity(0.4f);
        environment->set_glow_bloom(0.05f);
        environment->set_glow_hdr_bleed_threshold(1.0f);

        environment->set_fog_enabled(true);
        environment->set_fog_mode(Environment::FOG_MODE_DEPTH);
        environment->set_fog_density(1.0f);
        environment->set_fog_light_energy(SKY_ENERGY * BG_ENERGY);
        environment->set_fog_sky_affect(0.0f);

        reset_fog_distance();
        set_time_of_day(1.0f);
        set_environment(environment);
    }

    auto CraftSky::reset_fog_distance() -> void {
        environment->set_fog_depth_begin(render_distance * FOG_BEGIN_RATIO);
        environment->set_fog_depth_end(render_distance * FOG_END_RATIO);
    }

    auto CraftSky::set_time_of_day(f32 sun_height) -> void {
        if (sky_material.is_null() or environment.is_null()) return;

        const auto day = f32(Math::smoothstep(-0.15f, 0.25f, sun_height));
        const auto dusk = 1.0f - f32(Math::smoothstep(0.0f, 0.35f, f32(Math::abs(sun_height))));

        const auto top = Color(0.02f, 0.04f, 0.11f).lerp(Color(0.40f, 0.69f, 1.00f), day).lerp(Color(0.50f, 0.52f, 0.84f), dusk * 0.6f);
        const auto horizon = Color(0.05f, 0.09f, 0.18f).lerp(Color(0.80f, 0.94f, 1.00f), day).lerp(Color(1.00f, 0.66f, 0.48f), dusk * 0.8f);
        const auto ground = Color(0.02f, 0.03f, 0.06f).lerp(Color(0.52f, 0.64f, 0.72f), day);

        sky_material->set_sky_top_color(top);
        sky_material->set_sky_horizon_color(horizon);
        sky_material->set_ground_horizon_color(horizon);
        sky_material->set_ground_bottom_color(ground);

        environment->set_fog_light_color(horizon);
        environment->set_ambient_light_sky_contribution(Math::lerp(0.4f, 1.0f, day));
        environment->set_ambient_light_energy(Math::lerp(0.6f, 0.4f, day));
    }

    auto Sun::_ready() -> void {
        set_param(Light3D::PARAM_INDIRECT_ENERGY, 0.55f);
        set_sky_mode(DirectionalLight3D::SKY_MODE_LIGHT_AND_SKY);

        set_shadow(true);
        set_shadow_mode(DirectionalLight3D::SHADOW_PARALLEL_4_SPLITS);
        set_blend_splits(true);
        set_param(Light3D::PARAM_SHADOW_MAX_DISTANCE, SHADOW_DISTANCE);
        set_param(Light3D::PARAM_SHADOW_SPLIT_1_OFFSET, 0.08f);
        set_param(Light3D::PARAM_SHADOW_SPLIT_2_OFFSET, 0.2f);
        set_param(Light3D::PARAM_SHADOW_SPLIT_3_OFFSET, 0.5f);
        set_param(Light3D::PARAM_SHADOW_FADE_START, 0.85f);
        set_param(Light3D::PARAM_SHADOW_BIAS, 0.05f);
        set_param(Light3D::PARAM_SHADOW_NORMAL_BIAS, 1.2f);
    }

    auto Sun::bind_sky(CraftSky* sky) -> void {
        craft_sky = sky;
        applied_angle = -1.0f;
    }

    auto Sun::_process(f64 delta) -> void {
        day_angle = Math::fposmod(day_angle + day_speed * f32(delta), f32(MATH_TAU));

        const auto snapped = f32(Math::snapped(day_angle, SUN_ANGLE_STEP));
        if (snapped == applied_angle) return;

        applied_angle = snapped;
        apply_time();
    }

    auto Sun::apply_time() -> void {
        set_rotation(Vector3(applied_angle, SUN_YAW, 0.0f));

        const auto height = f32(-Math::sin(applied_angle));
        const auto strength = f32(Math::smoothstep(-0.02f, 0.15f, height));
        const auto noon = f32(Math::smoothstep(0.0f, 0.45f, height));

        set_color(Color(1.0f, 0.55f, 0.28f).lerp(Color(1.0f, 0.96f, 0.87f), noon));
        set_param(Light3D::PARAM_ENERGY, SUN_ENERGY * strength);
        set_shadow(strength > 0.001f);

        if (craft_sky) craft_sky->set_time_of_day(height);
    }
}