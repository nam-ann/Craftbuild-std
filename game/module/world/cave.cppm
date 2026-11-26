export module game.world.cave;

import std;

import misc.string;
import misc.number;

export namespace craftbuild {
    enum class CaveType { CHEESE, SPAGHETTI, NOODLE };

    struct Cave final {
        CaveType cave_type;
        f32 threshold;
        f32 frequency;
    };

    struct CaveEntry final {
        std::string name;
        Cave cave;
    };

    namespace cave_registry {
        inline std::vector<CaveEntry> registry;
        inline std::unordered_map<std::string, u64, string_hash, std::equal_to<>> name2id;

        auto register_cave(std::string_view name, Cave cave) -> void;
        auto get_cave(u64 cave_id) -> Cave&;
        auto get_name(u64 cave_id) -> std::string&;
        auto get_id(std::string_view cave_name) -> u64;
    };
}