module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/classes/texture2d.hpp>
ENABLE_WARNING

module game.block;

namespace craftbuild {
    Block::~Block() = default;
    auto Block::init_tags() -> std::unordered_set<u32> { return {}; }
    auto Block::init_metadatas() -> std::unordered_map<u32, std::string> { return {}; }

    auto Block1F::get_texture_layer(Face face) const -> i32 {
        return base_texture_layer;
    }

    auto Block3F::get_texture_layer(Face face) const -> i32 {
        switch (face) {
        case Face::TOP:    return base_texture_layer;
        case Face::BOTTOM: return base_texture_layer + 1;
        }
        return base_texture_layer + 2;
    }

    auto Block6F::get_texture_layer(Face face) const -> i32 {
        switch (face) {
        case Face::TOP:    return base_texture_layer;
        case Face::BOTTOM: return base_texture_layer + 1;
        case Face::LEFT:   return base_texture_layer + 2;
        case Face::RIGHT:  return base_texture_layer + 3;
        case Face::FRONT:  return base_texture_layer + 4;
        case Face::BACK:   return base_texture_layer + 5;
        }
        return base_texture_layer;
    }

    auto ComplexBlock::get_texture_layer(Face face) const -> i32 { return -1; }

    BlockEntry::BlockEntry(std::unique_ptr<Block> b, std::string_view n, Ref<Texture2D> const& t, Ref<Mesh> const& m) : block(std::move(b)), name(n), texture(t), mesh(m) {}

    namespace block_registry {
        auto get_block(u32 block_id) -> Block& {
            if (registry.size() <= block_id) return *registry[get_id("Air")].block;
            return *registry[block_id].block;
        }

        auto get_name(u32 block_id) -> std::string& {
            if (registry.size() <= block_id) return registry[0].name;
            return registry[block_id].name;
        }

        auto get_id(std::string_view block_name) -> u32 {
            if (not name2id.contains(block_name)) return 0;
            return name2id[std::string(block_name)];
        }

        auto has_block(std::string_view block_name) -> bool { return name2id.contains(block_name); }
    }
}