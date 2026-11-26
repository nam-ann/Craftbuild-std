module game.world.cave;

namespace craftbuild {
    namespace cave_registry {
        auto register_cave(std::string_view name, Cave cave) -> void {
            auto string_name = std::string(name);

            name2id[string_name] = registry.size();
            registry.emplace_back(string_name, cave);
        }

        auto get_cave(u64 cave_id) -> Cave& {
            if (registry.size() <= cave_id) return registry[0].cave;
            return registry[cave_id].cave;
        }

        auto get_name(u64 cave_id) -> std::string& {
            if (registry.size() <= cave_id) return registry[0].name;
            return registry[cave_id].name;
        }

        auto get_id(std::string_view cave_name) -> u64 {
            if (name2id.contains(cave_name)) return 0;
            return name2id[std::string(cave_name)];
        }
    }
}