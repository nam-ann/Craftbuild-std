module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/texture2d_array.hpp>
ENABLE_WARNING

module game.texture.atlas_texture;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    namespace atlas_texture {
        auto build_texture_array() -> void {
            auto images = Array();
            auto current_layer = 0;

            for (auto const& block : block_registry::registry) {
                if (block.texture.is_null()) continue;

                auto original_img = block.texture->get_image();
                if (original_img.is_null()) continue;

                block.block->base_texture_layer = current_layer;
                log<LogType::VERBOSE>("{} assigned layer: {}"f(block.name, current_layer));

                auto width = original_img->get_width();
                auto face_count = width / IMAGE_SIZE;

                for (auto i : std::views::iota(0, face_count)) {
                    auto tile = original_img->get_region(Rect2i(i * IMAGE_SIZE, 0, IMAGE_SIZE, IMAGE_SIZE));

                    tile->convert(Image::FORMAT_RGBA8);
                    tile->fix_alpha_edges();
                    tile->generate_mipmaps();

                    images.push_back(tile);
                    ++current_layer;
                }
            }

            if (images.size() == 0) return;

            atlas_texture.instantiate();
            auto err = atlas_texture->create_from_images(images);

            if (err != OK) log<LogType::ERROR>("Error: create_from_images failed code: {}"f(i32(err)));
            else log<LogType::VERBOSE>("TextureArray build success: {} layers"f(current_layer));
        }
    }
}