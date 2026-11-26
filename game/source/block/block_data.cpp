module game.block.block_data;

namespace craftbuild {
    namespace meta_registry {
        auto register_metadata(std::string_view name) -> void {
            registry.emplace_back(name);
            name2id[std::string(name)] = u32(registry.size() - 1);
        }

        auto get_metadata(u32 meta_id) -> std::string& {
            if (registry.size() <= meta_id) return registry[get_id("Air")];
            return registry[meta_id];
        }

        auto get_id(std::string_view meta_name) -> u32 {
            if (not name2id.contains(meta_name)) return 0;
            return name2id[std::string(meta_name)];
        }

        auto has_metadata(std::string_view meta_name) -> bool { return name2id.contains(meta_name); }
    }

    namespace tag_registry {
        auto register_tag(std::string_view name) -> void {
            registry.emplace_back(name);
            name2id[std::string(name)] = u32(registry.size() - 1);
        }

        auto get_tag(u32 tag_id) -> std::string& {
            if (registry.size() <= tag_id) return registry[get_id("Air")];
            return registry[tag_id];
        }

        auto get_id(std::string_view tag_name) -> u32 {
            if (not name2id.contains(tag_name)) return 0;
            return name2id[std::string(tag_name)];
        }

        bool has_tag(std::string_view tag_name) { return name2id.contains(tag_name); }
    }
}