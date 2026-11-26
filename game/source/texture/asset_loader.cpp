module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>
ENABLE_WARNING

module game.texture.asset_loader;

namespace craftbuild {
    namespace asset_loader {
        auto load_block_texture(std::string_view path_suffix, FaceCount face_count) -> Ref<Texture2D> {
            if (path_suffix.empty()) return Ref<Texture2D>();

            auto full_path = std::string(base_path);

            switch (face_count) {
            case FaceCount::ONE:   full_path += "1f/"; break;
            case FaceCount::THREE: full_path += "3f/"; break;
            case FaceCount::SIX:   full_path += "6f/"; break;
            }

            full_path += path_suffix;

            auto tex = ResourceLoader::get_singleton()->load(to_gstring(full_path));
            if (tex.is_valid()) {
                log<LogType::VERBOSE>("Loaded: \"{}\""f(full_path));
                return tex;
            }
            else log<LogType::ERROR>("Failed to load: \"{}\""f(full_path));
            return Ref<Texture2D>();
        }

        auto load_block_model(std::string_view path_suffix) -> Ref<PackedScene> {
            if (path_suffix.empty()) return Ref<PackedScene>();

            auto const full_path = "{}dynamic/{}"f(base_path, path_suffix);

            auto model = ResourceLoader::get_singleton()->load(to_gstring(full_path));
            if (model.is_valid()) {
                log<LogType::VERBOSE>("Loaded: \"{}\""f(full_path));
                return model;
            }
            else log<LogType::ERROR>("Failed to load: \"{}\""f(full_path));

            return Ref<PackedScene>();
        }
    }
}