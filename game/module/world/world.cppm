module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
ENABLE_WARNING

export module game.world;

import std;

import misc.pos;
import misc.string;
import misc.number;
import game.core;
import game.block;
import game.logger;
import game.thread;
import game.command;
import game.world.chunk;
import game.player.player_data;

using namespace godot;

export namespace craftbuild {
    class World final {
        std::unordered_map<Pos2D<i32>, std::shared_ptr<Chunk>> chunks;
        mutable std::shared_mutex chunks_mutex;

        Ref<FastNoiseLite> noise;
        std::atomic<i32> world_seed = 0dw;
        std::string world_name = "My World";

        std::unordered_map<std::string, PlayerData, string_hash, std::equal_to<>> players;
        std::unordered_map<std::string, u8, string_hash, std::equal_to<>> online_players;
        decltype(online_players.begin()) current_player;
        CommandInterpreter interpreter;
        mutable std::shared_mutex player_mutex;
        mutable std::mutex current_player_mutex;

        std::atomic<bool> running = true;
        std::jthread redstone_thread;
        std::jthread scheduler_thread;
        ThreadPool terrain_pool = 4;
        std::unordered_set<Pos2D<i32>> pending_terrain_jobs;
        mutable std::mutex pending_jobs_mutex;

        std::atomic<bool> pausing = true;
        std::atomic<bool> chatting = false;
        std::condition_variable loop_cv;
        mutable std::mutex loop_mutex;

    public:
        inline static auto RANGE = render_distance * 16dw;

        World();
        ~World();
        auto connect(std::string_view player_name) -> void;
        auto disconnect(std::string_view player_name) -> void;
        auto update(std::string_view player_name, Pos3D<fsize> const& new_pos) -> void;

        auto start_redstone_thread() -> void;
        auto start_scheduler_thread() -> void;
        auto submit_jobs(Pos3D<fsize> const& player) -> void;

        auto serialize_players() -> std::string;
        auto serialize_chunk(i32 cx, i32 cy) -> std::string;
        auto get_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk>;
        auto get_or_load_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk>;
        auto get_or_create_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk>;
        auto get_global_block_id(i32 wx, i32 wy, i32 wz) -> u32;
        auto set_global_block_id(u32 block_id, i32 wx, i32 wy, i32 wz) -> void;
        auto unload_distant_chunks() -> void;

        auto set_seed_and_world_name(i32 seed, std::string_view name) -> void;
        auto set_render_distance(i32 rd) -> void;
        auto set_cpu_sleep_time(i32 stc) -> void;

        auto chat(std::string_view message) -> std::string;

        auto save_world(std::string_view path) -> void;
        auto load_world(std::string_view path) -> bool;
        auto save_region(std::string_view path, i32 rx, i32 ry) -> void;
        auto load_region(std::string_view path, i32 rx, i32 ry) -> bool;

        friend class Main;
        friend class Server;
        friend class CommandInterpreter;
    };
}