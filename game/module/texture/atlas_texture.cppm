module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/texture2d_array.hpp>
ENABLE_WARNING

export module game.texture.atlas_texture;

import misc.string;
import misc.number;
import game.block;
import game.logger;

using namespace godot;

export namespace craftbuild {
    inline constexpr auto IMAGE_SIZE = 16ub;

	namespace atlas_texture {
		inline auto atlas_texture = Ref<Texture2DArray>();
		auto build_texture_array() -> void;
	};
}