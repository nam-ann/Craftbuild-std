module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
ENABLE_WARNING

export module game.block;

import std;

import misc.string;
import misc.number;
import game.core;
import game.logger;
import game.block.block_data;
import game.texture.asset_loader;

using namespace godot;

namespace craftbuild::atlas_texture { void build_texture_array(); }

export namespace craftbuild {
    inline constexpr auto FACE_LEN = 6ub;
    enum class Face : u8 { TOP, BOTTOM, RIGHT, LEFT, FRONT, BACK };

    class Block {
    protected:
        i32 base_texture_layer = 0dw;

    public:
        virtual ~Block();
        virtual auto get_texture_layer(Face face) const -> i32 = 0;
        virtual auto init_tags() -> std::unordered_set<u32>;
        virtual auto init_metadatas() -> std::unordered_map<u32, std::string>;

        friend auto atlas_texture::build_texture_array() -> void;
    };

    struct Block1F : Block { auto get_texture_layer(Face face) const -> i32 override final; };
    struct Block3F : Block { auto get_texture_layer(Face face) const -> i32 override final; };
    struct Block6F : Block { auto get_texture_layer(Face face) const -> i32 override final; };
    struct ComplexBlock : Block { auto get_texture_layer(Face face) const -> i32 override final; };

    struct BlockEntry final {
        Ref<Texture2D> texture = nullptr;
        Ref<Mesh> mesh;
        std::unique_ptr<Block> block = nullptr;
        std::string name;

        BlockEntry(std::unique_ptr<Block> b, std::string_view n, Ref<Texture2D> const& t, Ref<Mesh> const& m);
    };

    namespace block_registry {
        inline auto registry = std::vector<BlockEntry>{};
        inline auto name2id = std::unordered_map<std::string, u32, string_hash, std::equal_to<>>();

        template <typename T>
        requires std::derived_from<T, Block>
        auto register_block(std::string_view name, std::string_view path) -> void {
            auto texture = Ref<Texture2D>();
            auto mesh = Ref<Mesh>();

            if constexpr (std::derived_from<T, Block1F>) {
                texture = asset_loader::load_block_texture(path, FaceCount::ONE);
            }
            else if constexpr (std::derived_from<T, Block3F>) {
                texture = asset_loader::load_block_texture(path, FaceCount::THREE);
            }
            else if constexpr (std::derived_from<T, Block6F>) {
                texture = asset_loader::load_block_texture(path, FaceCount::SIX);
            }
            else if constexpr (std::derived_from<T, ComplexBlock>) {
                auto glb_model = asset_loader::load_block_model(path);

                if (glb_model.is_valid()) {
                    auto root = glb_model->instantiate();
                    auto mesh_children = root->find_children("*", "MeshInstance3D", true, false);
                    auto mesh_inst = mesh_children.is_empty() ? nullptr : Object::cast_to<MeshInstance3D>(mesh_children[0]);

                    mesh = mesh_inst->get_mesh();
                }
            }

            registry.emplace_back(std::make_unique<T>(), name, texture, mesh);
            name2id[std::string(name)] = u32(registry.size() - 1);
        }

        auto get_block(u32 block_id) -> Block&;
        auto get_name(u32 block_id) -> std::string&;
        auto get_id(std::string_view block_name) -> u32;
        auto has_block(std::string_view block_name) -> bool;
    }
}