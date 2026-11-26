export module game.world.biome;

import misc.string;
import misc.number;

export namespace craftbuild {
	struct Biome final {
		f32 base_noise = 0.0f;
		f32 base_height = 0.0f;
		f32 detail_noise = 0.0f;
		f32 detail_height = 0.0f;
		f32 temperature = 0.0f;
		i32 min_height = 0dw;
	};

	struct BiomeEntry final {
		std::string name;
		Biome biome;
	};

	namespace biome_registry {
		inline std::vector<BiomeEntry> registry;
		inline std::unordered_map<std::string, u64, string_hash, std::equal_to<>> name2id;

		auto register_biome(std::string_view name, Biome const& biome) -> void;
		auto get_biome(u64 biome_id) -> Biome&;
		auto get_name(u64 biome_id) -> std::string&;
		auto get_id(std::string_view biome_name) -> u64;
	};
}