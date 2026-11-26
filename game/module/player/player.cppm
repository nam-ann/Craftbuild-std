module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
ENABLE_WARNING

export module game.player;

import std;

import misc.pos;
import misc.string;
import misc.number;
import game.core;
import game.block;
import game.logger;
import game.world.chunk;
import game.texture.atlas_texture;

using namespace godot;

export namespace craftbuild {
    enum class Gamemode : u8 { SURVIVAL, CREATIVE, ADVENTURE, SPECTATOR };

    class Player final : public CharacterBody3D {
        GDCLASS(Player, CharacterBody3D)

    public:
        // Movement
        fsize speed = 4.0fz;
        fsize gravity = 24.0fz;
        fsize jump_velocity = 8.0fz;
        bool is_grounded = false;
        bool can_fly = false;
        bool jump_was_pressed = false;
        bool running = false;
        Gamemode gamemode = Gamemode::SURVIVAL;

        // Camera
        Camera3D* camera = nullptr;
        f32 sensitivity = 0.0043f;
        f32 mouse_pitch = 0.0f;
        
        // Gameplay
        inline static constexpr auto HOTBAR_SIZE = 9ub;
        std::array<u32, HOTBAR_SIZE> hotbar = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        u8 selected_slot = 0;
        std::unordered_map<std::string, i32> inventory;
        i8 hp = 20;

        // World
        void* world_ptr = nullptr;
        Dictionary hit;
        MeshInstance3D* selection_box;

    public:
        auto _ready() -> void override;
        auto _process(f64 delta) -> void override;
        auto _physics_process(f64 delta) -> void override;
        auto _input(Ref<InputEvent> const& event) -> void override;

        auto would_collide_with_player(Pos3D<i32> const& block_pos) const -> bool;
        auto create_selection_box_material() -> Ref<ShaderMaterial>;
        auto raycast_block(fsize max_distance = 5.0f) -> Dictionary;
        auto get_face(Pos3D<fsize> const& n) -> Face;

        auto cycle_hotbar(i16 dir) -> void;
        auto select_slot(u8 slot) -> void;
        auto get_selected_block_id() const -> u32;

        auto save_data(std::ostream& os) -> void;
        auto load_data(std::istream& is) -> void;

        static auto _bind_methods() -> void;
    };
}
