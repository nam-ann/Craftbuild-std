module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
ENABLE_WARNING

export module game.main;

import std;

import misc.pos;
import misc.string;
import misc.number;
import game.core;
import game.block;
import game.world;
import game.logger;
import game.thread;
import game.network;
import game.environment;
import game.world.cave;
import game.world.chunk;
import game.world.biome;
import game.block.redstone;
import game.block.block_data;
import game.player.player_data;
import game.block.normal_blocks;
import game.texture.atlas_texture;

using namespace godot;

export namespace craftbuild {
    class Main final : public Node3D {
        GDCLASS(Main, Node3D)

    private:
        std::vector<Pos2D<i32>> ready_chunks_queue;
        mutable std::mutex ready_chunks_queue_mutex;

        std::unordered_map<Pos2D<i32>, std::tuple<std::shared_ptr<Chunk>, ChunkRender>> chunks;
        mutable std::shared_mutex chunks_mutex;

        std::unordered_set<Pos2D<i32>> requested_chunks;
        mutable std::mutex requested_chunks_mutex;

        Ref<ShaderMaterial> world_material;
        std::atomic<i32> world_seed = 0;
        std::string world_name = "My World";
        std::string player_name = "Player";
        std::tuple<String, i32> server_socket = std::tuple(String("127.0.0.1"), 8888);

        void* player_ptr = nullptr;
        mutable std::shared_mutex player_mutex;

        std::atomic<bool> running = true;
        std::atomic<fsize> player_x = 0.0fz;
        std::atomic<fsize> player_y = 0.0fz;
        std::atomic<fsize> player_z = 0.0fz;
        std::jthread log_thread;
        std::jthread network_thread;
        std::jthread scheduler_thread;
        ThreadPool mesh_pool = 4;
        std::unordered_set<Pos2D<i32>> pending_mesh_jobs;
        mutable std::mutex pending_jobs_mutex;

        std::atomic<bool> pausing = true;
        std::atomic<bool> chatting = false;
        std::condition_variable loop_cv;
        mutable std::mutex loop_mutex;

        bool full_screen = false;

        SendDeque send_queue;
        ReceiveQueue receive_queue;
		std::unique_ptr<World> server_ptr;

    public:
        auto _ready() -> void override;
        auto _process(f64 delta) -> void override;
        auto _exit_tree() -> void override;

        auto init_singleplayer() -> void;
        auto init_multiplayer() -> void;
        auto setup_voxel_material() -> void;

        auto start_log_thread() -> void;
        auto start_network_thread() -> void;
        auto start_scheduler_thread() -> void;
        auto submit_jobs() -> void;
        auto create_chunk_collision(ChunkRender& chunk_render, PackedVector3Array const& collision_faces) -> void;
        auto update_chunk_mesh(ChunkRender& chunk_render, Pos2D<i32>& pos, Ref<ArrayMesh> const& mesh, i32 submesh_idx) -> void;
        auto unload_distant_chunks() -> void;

        auto get_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk>;
        auto get_or_create_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk>;
        auto ref_mesh(i32 cx, i32 cy) -> ChunkRender&;
        auto get_global_block_id(i32 wx, i32 wy, i32 wz) -> u32;
        auto set_chunk(std::shared_ptr<Chunk>& chunk, i32 cx, i32 cy) -> void;
        auto set_global_block_id(u32 block_id, i32 wx, i32 wy, i32 wz) -> void;

        auto save_userdata(std::string_view path = "user://game/userdata.cbdata") -> void;
        auto load_userdata(std::string_view path = "user://game/userdata.cbdata") -> bool;

        auto pause() -> void;
        auto resume() -> void;
        auto start_chat() -> void;

        auto chat(const String msg) -> void;

        auto get_player_position() -> Vector3;
        auto set_seed_and_world_name(i32 seed, const String name) -> void;
        auto set_render_distance(i32 rd) -> void;
        auto set_cpu_sleep_time(i32 stc) -> void;
        auto set_server_socket(String ip, int32_t port) -> void;

        static auto _bind_methods() -> void;

        friend class Player;
    };
}
