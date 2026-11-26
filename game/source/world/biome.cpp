module;

#include <defs.hpp>

module game.world.biome;

namespace craftbuild {
	namespace biome_registry {
		auto register_biome(std::string_view name, Biome const& biome) -> void {
			auto string_name = std::string(name);

			name2id[string_name] = registry.size();
			registry.emplace_back(string_name, biome);
		}

		auto get_biome(u64 biome_id) -> Biome& {
			if (registry.size() <= biome_id) return registry[0].biome;
			return registry[biome_id].biome;
		}

		auto get_name(u64 biome_id) -> std::string& {
			if (registry.size() <= biome_id) return registry[0].name;
			return registry[biome_id].name;
		}

		auto get_id(std::string_view biome_name) -> u64 {
			if (name2id.contains(biome_name)) return 0;
			return name2id[std::string(biome_name)];
		}
	}
}