module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
ENABLE_WARNING

export module game.player.skin_manager;

import std;

import misc.string;
import misc.number;
import game.core;
import game.logger;
import game.player;

using namespace godot;

export namespace craftbuild {
    namespace skin_manager {
        auto load_skin(Player& player, std::string_view path) -> bool;
        auto create_skin_material(Ref<Texture2D> texture) -> Ref<StandardMaterial3D>;
        auto apply_skin_to_model(MeshInstance3D* model, Ref<Texture2D> texture) -> void;
    };
}