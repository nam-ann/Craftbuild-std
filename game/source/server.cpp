module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
ENABLE_WARNING

module game.server;

using namespace std::literals;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    auto Server::_ready() -> void {
        start_log_thread();

        meta_registry::register_metadata("transparent");

        block_registry::register_block<Air>("Air", "");
        block_registry::register_block<Grass>("Grass Block", "");
        block_registry::register_block<Dirt>("Dirt", "");
        block_registry::register_block<Stone>("Stone", "");
        block_registry::register_block<Pebble>("Pebble", "");
        block_registry::register_block<OakLog>("Oak Log", "");
        block_registry::register_block<OakPlanks>("Oak Planks", "");
        block_registry::register_block<OakLeaves>("Oak Leaves", "");
        block_registry::register_block<DiamondBlock>("Diamond Block", "");
        block_registry::register_block<DiamondOre>("Diamond Ore", "");
        block_registry::register_block<Bedrock>("Bedrock", "");
        block_registry::register_block<RedstoneBlock>("Redstone Block", "");
        block_registry::register_block<RedstoneDust>("Redstone Dust", "");

        auto plains = Biome();
        plains.base_height = 5.0f;
        plains.base_noise = 0.1f;
        plains.detail_height = 4.0f;
        plains.detail_noise = 0.2f;
        plains.min_height = 40;

        auto normal = Biome();
        normal.base_height = 60.0f;
        normal.base_noise = 0.05f;
        normal.detail_height = 8.0f;
        normal.detail_noise = 0.5f;
        normal.min_height = 40;

        auto mountains = Biome();
        mountains.base_height = 140.0f;
        mountains.base_noise = 0.02f;
        mountains.detail_height = 35.0f;
        mountains.detail_noise = 0.15f;
        mountains.min_height = 40;

        auto jagged_peaks = Biome();
        jagged_peaks.base_height = 180.0f;
        jagged_peaks.base_noise = 0.01f;
        jagged_peaks.detail_height = 60.0f;
        jagged_peaks.detail_noise = 0.4f;
        jagged_peaks.min_height = 60;

        auto cherry_grove = Biome();
        cherry_grove.base_height = 50.0f;
        cherry_grove.base_noise = 0.08f;
        cherry_grove.detail_height = 15.0f;
        cherry_grove.detail_noise = 0.25f;
        cherry_grove.min_height = 45;

        biome_registry::register_biome("Plains", plains);
        biome_registry::register_biome("Normal", normal);
        biome_registry::register_biome("Mountains", mountains);
        biome_registry::register_biome("Jagged Peaks", jagged_peaks);
        biome_registry::register_biome("Cherry Grove", cherry_grove);

        cave_registry::register_cave("Large Cavern", { CaveType::CHEESE, 0.5f, 0.02f });
        cave_registry::register_cave("Standard Tunnel", { CaveType::SPAGHETTI, 0.45f, 0.05f });
        cave_registry::register_cave("Deep Noodle", { CaveType::NOODLE, 0.35f, 0.08f });

        tcp_server.instantiate();
        auto const err = tcp_server->listen(8888);

        if (err != OK) {
            log<LogType::ERROR>("Failed to start TCP Server on port 8888");
            return;
        }

        log<LogType::INFO>("TCP Server listening on port 8888...");
    }

    auto Server::_process(f64 delta) -> void {
        if (tcp_server.is_valid() and tcp_server->is_connection_available()) {
            auto client_peer = tcp_server->take_connection();
            if (client_peer.is_valid()) {
                clients[client_peer].ip_addr = "{}:{}"f(client_peer->get_connected_host().utf8(), client_peer->get_connected_port());
                log<LogType::INFO>("Accepted client: {}"f(clients[client_peer].ip_addr));
            }
        }

        auto disconnected_clients = std::vector<Ref<StreamPeerTCP>>{};
        for (auto& [client_peer, client] : clients) {
            auto buffer = ""s;
            const auto recv_state = client.receive_queue.receive(**client_peer, buffer);

            if (recv_state == ReceiveState::WAITING) continue;
            if (recv_state == ReceiveState::ERROR) {
                log<LogType::WARNING>("Lost connect to client: {}"f(client.ip_addr));

                server.disconnect(client.name);
                disconnected_clients.push_back(client_peer);
                continue;
            }

            auto const message = ReceiveQueue::parse(buffer);
            if (message.content.empty()) continue;

            try {
                if (message.content == "Connect") {
                    auto const& name = message.arguments[0];

                    server.connect(name);
                    client.name = name;

                    const auto player_pos = server.players[client.name].pos;
                    client.send_queue.store({ "Connected", { std::to_string(player_pos.x), std::to_string(player_pos.y), std::to_string(player_pos.z), std::string(VERSION) } });
                }
                else if (message.content == "Chat") {
                    auto const response = server.chat(message.arguments[0]);
                    client.send_queue.store({ "Chat response", { response } });
                }
                else if (message.content == "Get chunk version") {
                    auto const cx = std::stoi(message.arguments[0]);
                    auto const cy = std::stoi(message.arguments[1]);

                    if (auto chunk_ptr = server.get_chunk(cx, cy)) client.send_queue.store({ "Chunk version", { std::to_string(chunk_ptr->chunk_version), message.arguments[0], message.arguments[1]} });
                    else client.send_queue.store({ "Chunk not ready", { message.arguments[0], message.arguments[1] } });
                }
                else if (message.content == "Get chunk data") {
                    auto const cx = std::stoi(message.arguments[0]);
                    auto const cy = std::stoi(message.arguments[1]);

                    auto chunk_ptr = server.get_chunk(cx, cy);
                    if (chunk_ptr and chunk_ptr->generated.load(std::memory_order::acquire)) {
                        auto const world_data = server.serialize_chunk(cx, cy);
                        client.send_queue.store({ "Chunk data", { world_data, message.arguments[0], message.arguments[1] } });
                    }
                    else client.send_queue.store({ "Chunk not ready", { message.arguments[0], message.arguments[1] } });
                }
                else if (message.content == "std::unordered_set block") {
                    server.set_global_block_id(std::stoi(message.arguments[0]), std::stoi(message.arguments[1]), std::stoi(message.arguments[2]), std::stoi(message.arguments[3]));
                    client.send_queue.store({ "Block set" });
                }
                else if (message.content == "Get players data") {
                    auto const players_data = server.serialize_players();
                    client.send_queue.store({ "Players data", { players_data } });
                }
                else if (message.content == "std::unordered_set seed and world name") {
                    auto const seed = std::stoi(message.arguments[0]);
                    auto const& world_name = message.arguments[1];

                    server.set_seed_and_world_name(seed, world_name);
                    client.send_queue.store({ "std::unordered_set" });
                }
                else if (message.content == "std::unordered_set render distance") {
                    auto const rd = std::stoi(message.arguments[0]);

                    server.set_render_distance(rd);
                    client.send_queue.store({ "std::unordered_set" });
                }
                else if (message.content == "std::unordered_set sleep time CPU") {
                    auto const stc = std::stoi(message.arguments[0]);

                    server.set_cpu_sleep_time(stc);
                    client.send_queue.store({ "std::unordered_set" });
                }
                else if (message.content == "Update player pos") {
                    auto const pos = Pos3D{
                        fsize(std::stod(message.arguments[1])),
                        fsize(std::stod(message.arguments[2])),
                        fsize(std::stod(message.arguments[3]))
                    };

                    server.update(message.arguments[0], pos);
                    client.send_queue.store({ "Updated" });
                }
                else client.send_queue.store({ "Invalid message content", { message.content } });
            }
            catch (std::exception const& e) {
                log<LogType::ERROR>("Error processing message: {} - {}"f(message.content, e.what()));
                client.send_queue.store({ "Error", { e.what() } });
            }

            client.send_queue.send(**client_peer);
        }

        for (auto const& client : disconnected_clients) {
            client->disconnect_from_host();
            clients.erase(client);
        }

        LogQueue::flush();
    }

    void Server::_exit_tree() {
        if (tcp_server.is_valid() and tcp_server->is_listening()) tcp_server->stop();

        for (auto& [client_peer, client] : clients) {
            if (not client_peer.is_valid()) continue;
            client_peer->disconnect_from_host();
        }

        clients.clear();
        running.store(false, std::memory_order::release);
    }

    auto Server::start_log_thread() -> void {
        if (log_thread.joinable()) return;

        auto worker = [this] {
            thread_registry::register_thread("Log");
            log<LogType::INFO>("Log thread started");

            while (running.load(std::memory_order::relaxed)) {
                LogQueue::flush();
                std::this_thread::sleep_for(1s);
            }

            LogQueue::flush();
        };

        log_thread = std::jthread(worker);
    }
}