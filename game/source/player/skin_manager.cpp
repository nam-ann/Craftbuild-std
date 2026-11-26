module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
ENABLE_WARNING

module game.player.skin_manager;

namespace craftbuild {
    namespace skin_manager {
        auto load_skin(Player& player, std::string_view path) -> bool {
            auto skin_tex = ResourceLoader::get_singleton()->load(to_gstring(path));
            if (skin_tex.is_null()) {
                log<LogType::ERROR>("Failed to load skin: {}"f(path));
                return false;
            }

            log<LogType::INFO>("Skin loaded: {}"f(path));

            auto player_model = player.get_node<MeshInstance3D>("Mesh");
            if (player_model) apply_skin_to_model(player_model, skin_tex);
            else log<LogType::WARNING>("Player model not found. Create a MeshInstance3D named 'Model'");

            return true;
        }

        auto create_skin_material(Ref<Texture2D> texture) -> Ref<StandardMaterial3D> {
            auto mat = Ref<StandardMaterial3D>();
            mat.instantiate();
            mat->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, texture);
            mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_TEXTURE_FORCE_SRGB, true);
            mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS); // Support layer 2
            return mat;
        }

        auto apply_skin_to_model(MeshInstance3D* model, Ref<Texture2D> texture) -> void {
            if (not model) return;

            auto mat = create_skin_material(texture);
            model->set_material_override(mat);
            log<LogType::VERBOSE>("Skin applied to model");
        }
    }
}