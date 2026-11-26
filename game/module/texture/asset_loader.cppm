module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
ENABLE_WARNING

export module game.texture.asset_loader;

import misc.string;
import misc.number;
import game.core;
import game.logger;

using namespace godot;

export namespace craftbuild {
    enum class FaceCount : u8 { ONE = 1, THREE = 3, SIX = 6 };

    namespace asset_loader {
        inline constexpr auto base_path = "res://assets/textures/block/"fs;
        auto load_block_texture(std::string_view path_suffix, FaceCount face_count) -> Ref<Texture2D>;
        auto load_block_model(std::string_view path_suffix) -> Ref<PackedScene>;
    };
}