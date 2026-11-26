module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/tcp_server.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
ENABLE_WARNING

export module game.server;

import std;

import misc.pos;
import misc.string;
import misc.number;
import game.core;
import game.block;
import game.world;
import game.logger;
import game.network;
import game.world.cave;
import game.world.chunk;
import game.world.biome;
import game.block.redstone;
import game.block.block_data;
import game.block.normal_blocks;

using namespace godot;

export namespace craftbuild {
    struct Client final {
        ReceiveQueue receive_queue;
        SendDeque send_queue;
        std::string name;
        std::string ip_addr;
    };

    class Server final : public Node {
        GDCLASS(Server, Node)

        Ref<TCPServer> tcp_server;
        World server;
        std::unordered_map<Ref<StreamPeerTCP>, Client> clients;

		std::atomic<bool> running = true;
        std::jthread log_thread;

    public:
        auto _ready() -> void override;
        auto _process(f64 delta) -> void override;
        auto _exit_tree() -> void override;

        auto start_log_thread() -> void;

		static auto _bind_methods() -> void {}
    };
}