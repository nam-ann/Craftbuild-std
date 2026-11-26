module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
ENABLE_WARNING

export module game.world.chunk;

import std;

import misc.pos;
import misc.raii;
import misc.string;
import misc.number;
import misc.multi_array;
import game.core;
import game.block;
import game.logger;
import game.world.cave;
import game.world.biome;
import game.world.terrain;
import game.block.block_data;

using namespace godot;

export namespace craftbuild {
    struct ComplexBlockInstance final {
        u32 block_id;
        Pos3D<u8> local_pos;
    };

    struct MeshData final {
        std::vector<Pos3D<fsize>> vertices;
        std::vector<Pos3D<fsize>> normals;
        std::vector<i32> indices;
        std::vector<Pos2D<fsize>> uvs;
        std::vector<Pos2D<fsize>> uvs_layer;
        std::vector<Pos3D<fsize>> collision_faces;
        std::vector<ComplexBlockInstance> complex_instance;
    };

    class MeshesData final {
        std::array<MeshData, 4> sub;

    public:
		auto operator[](u8 idx) -> MeshData& { return sub[idx]; }
    };

    class FaceMask final {
        u32 value = 0udw;

    public:
        static constexpr auto BACK_FACE_BIT = 0x80000000udw;

        constexpr FaceMask();
        constexpr FaceMask(i32 layer, bool back_face);

        auto layer() const -> i32;
        auto back_face() const -> bool;

        auto operator==(FaceMask const& other) const -> bool;
    };

    struct ChunkRender final {
        std::array<MeshInstance3D*, 4> mesh_instances = {};
        StaticBody3D* collision_body = nullptr;
        CollisionShape3D* collision_shape = nullptr;

        MultiMeshInstance3D* multi_mesh_instance = nullptr;
        StaticBody3D* dynamic_body = nullptr;

        std::unique_ptr<MeshesData> pending_meshes_data = nullptr;
        mutable std::shared_mutex mesh_mutex;

        ~ChunkRender() noexcept;
        void clear();
    };

    class Chunk final {
    public:
        inline static constexpr auto WIDTH  = 16ub;
        inline static constexpr auto HEIGHT = 255ub;

        std::array<u32, 256> block_ids;
        u8 block_ids_size = 0ub;

        std::unordered_map<u32, u8> id2block;

        std::unordered_map<Pos3D<u8>, std::unordered_set<u32>> tag_ids;
        std::unordered_map<Pos3D<u8>, std::unordered_map<u32, std::string>> meta_ids;
        std::unordered_map<Pos3D<u8>, u32> extended_block_id;
        multi_array<u8, WIDTH, HEIGHT, WIDTH> blocks = {};

        Pos2D<i32> chunk_pos;
        TrapezoidHeight height_provider{ VerticalAnchor::absolute(18), VerticalAnchor::absolute(38), 8 };
        std::atomic<bool> generated = false;
        std::atomic<bool> dirty = true;
        mutable std::shared_mutex data_mutex;

        u8 chunk_version = 0ub;

        auto clear() -> void;

        static auto column_seed(i32 seed, i32 x, i32 z) -> u32;
        static auto smoothstep(f32 value) -> f32;
        static auto lerp_biome(Biome const& a, Biome const& b, f32 t) -> Biome;
        static auto select_biome_at(i32 wx, i32 wz, Ref<FastNoiseLite> noise, usize biome_count) -> Biome;
        static auto get_blended_biome(i32 wx, i32 wz, Ref<FastNoiseLite> noise, usize biome_count) -> Biome;

        auto set_block(Pos3D<u8> const& pos, std::string_view block) -> void;
        auto set_block(Pos3D<u8> const& pos, u32 block_id) -> void;

        auto tag_block(Pos3D<u8> const& pos, std::string_view tag) -> void;
        auto tag_block(Pos3D<u8> const& pos, u32 tag_id) -> void;

        auto set_block_metadata(Pos3D<u8> const& pos, std::string_view meta, std::string_view meta_data = "") -> void;
        auto set_block_metadata(Pos3D<u8> const& pos, u32 meta_id, std::string_view meta_data = "") -> void;

        auto has_tag(Pos3D<u8> const& pos, std::string_view tag) const -> bool;
        auto has_tag(Pos3D<u8> const& pos, u32 tag_id) const -> bool;

        auto has_metadata(Pos3D<u8> const& pos, std::string_view meta, std::string_view meta_data = "") const -> bool;
        auto has_metadata(Pos3D<u8> const& pos, u32 meta_id, std::string_view meta_data = "") const -> bool;

        auto get_block(Pos3D<u8> const& pos) const -> u32;
        auto get_tag(Pos3D<u8> const& pos) const -> std::unordered_set<u32> const*;
        auto get_metadata(Pos3D<u8> const& pos) const -> std::unordered_map<u32, std::string> const*;

        auto generate_terrain(i32 seed, Ref<FastNoiseLite> noise) -> void;
        auto generate_mesh(ChunkRender& mesh, std::array<std::shared_ptr<Chunk>, 4> neighbors) -> void;
    };
}
