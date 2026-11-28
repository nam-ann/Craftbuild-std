module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/box_shape3d.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/static_body3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/stream_peer_tcp.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/variant/node_path.hpp>
ENABLE_WARNING

module game.main;

import game.player;
import game.command;

using namespace std::literals;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    auto Main::_ready() -> void {
        start_log_thread();

        meta_registry::register_metadata("transparent");
        
        block_registry::register_block<Air>          ("Air"           , "");
        block_registry::register_block<Grass>        ("Grass Block"   , "grass_block.png");
        block_registry::register_block<Dirt>         ("Dirt"          , "dirt.png");
        block_registry::register_block<Stone>        ("Stone"         , "stone.png");
        block_registry::register_block<Pebble>       ("Pebble"        , "pebble.png");
        block_registry::register_block<OakLog>       ("Oak Log"       , "oak_log.png");
        block_registry::register_block<OakPlanks>    ("Oak Planks"    , "oak_planks.png");
        block_registry::register_block<OakLeaves>    ("Oak Leaves"    , "oak_leaves.png");
        block_registry::register_block<DiamondBlock> ("Diamond Block" , "diamond_block.png");
        block_registry::register_block<DiamondOre>   ("Diamond Ore"   , "diamond_ore.png");
        block_registry::register_block<Bedrock>      ("Bedrock"       , "bedrock.png");
        block_registry::register_block<RedstoneBlock>("Redstone Block", "redstone_block.png");
        block_registry::register_block<RedstoneDust> ("Redstone Dust" , "redstone_dust.glb");

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

        {
            auto const lock = std::unique_lock(player_mutex);
            player_ptr = get_node<Player>("Player");
        }
        if (not load_userdata()) log<LogType::WARNING>("Userdata file not found.");

        atlas_texture::build_texture_array();
        setup_voxel_material();

        log<LogType::INFO>("Main initialized");
    }

    auto Main::_process(f64 delta) -> void {
        auto player = static_cast<Player*>(player_ptr);
        if (not player) return;

		auto const player_pos = player->get_global_position();
        player_x.store(player_pos.x, std::memory_order::relaxed);
        player_y.store(player_pos.y, std::memory_order::relaxed);
        player_z.store(player_pos.z, std::memory_order::relaxed);

        static auto last_sent_pos = Pos3D<fsize>();

        if (not server_ptr) {
            if (player_pos - last_sent_pos > Pos3D(0.01fz, 0.01fz, 0.01fz)) {
                last_sent_pos = player_pos;
                send_queue.store({ "Update player pos", { player_name, std::to_string(player_pos.x), std::to_string(player_pos.y), std::to_string(player_pos.z)} });
            }

            static auto last_sent_get_player_request = std::chrono::steady_clock::now();
            auto const elapsed = std::chrono::steady_clock::now() - last_sent_get_player_request;
            
            if (elapsed >= 1s) {
                last_sent_get_player_request = std::chrono::steady_clock::now();
                send_queue.store({ "Get players data", { player_name } });
            }
        }
        else {
            if (player_pos - last_sent_pos > Pos3D(0.01fz, 0.01fz, 0.01fz)) {
                server_ptr->update(player_name, player_pos);
            }

            static auto last_sent_get_player_request = std::chrono::steady_clock::now();
            auto const elapsed = std::chrono::steady_clock::now() - last_sent_get_player_request;
            
            if (elapsed >= 1s) {
                auto const lock = std::unique_lock(player_mutex);
                auto& player_data = server_ptr->players[player_name];

                player->hp = player_data.hp;
                std::memcpy(&player->hotbar, &player_data.hotbar, sizeof player_data.hotbar);
            }
        }

        auto chunks_to_upload = std::vector<Pos2D<i32>>{};
        {
            auto const lock = std::lock_guard(ready_chunks_queue_mutex);
            ready_chunks_queue.swap(chunks_to_upload);
        }

        auto deferred_chunks = std::vector<Pos2D<i32>>{};
        {
            auto vertices = PackedVector3Array();
            auto normals = PackedVector3Array();
            auto indices = PackedInt32Array();
            auto uvs = PackedVector2Array();
            auto uvs_layer = PackedVector2Array();

            auto total_collision_faces = PackedVector3Array();

            static constexpr auto max_updates = 8;
            auto updates_this_frame = 0;

            for (auto& chunk_pos : chunks_to_upload) {
                if (updates_this_frame >= max_updates) {
                    deferred_chunks.push_back(chunk_pos);
                    continue;
                }

                auto& chunk_render = ref_mesh(chunk_pos.x, chunk_pos.y);

                auto chunk_data_ptr = std::unique_ptr<MeshesData>();
                {
                    auto const lock = std::lock_guard(chunk_render.mesh_mutex);
                    if (chunk_render.pending_meshes_data) chunk_data_ptr.swap(chunk_render.pending_meshes_data);
                }

                if (not chunk_data_ptr) continue;
                auto& chunk_data = *chunk_data_ptr;

                for (auto sub_idx : std::views::iota(0, 4)) {
                    auto& data = chunk_data[sub_idx];

                    if (not data.vertices.empty()) {
                        vertices.resize(data.vertices.size());
                        normals.resize(data.normals.size());
                        indices.resize(data.indices.size());
                        uvs.resize(data.uvs.size());
                        uvs_layer.resize(data.uvs_layer.size());

                        std::memcpy(vertices.ptrw(), data.vertices.data(), data.vertices.size() * sizeof(Pos3D<f32>));
                        std::memcpy(normals.ptrw(), data.normals.data(), data.normals.size() * sizeof(Pos3D<f32>));
                        std::memcpy(indices.ptrw(), data.indices.data(), data.indices.size() * sizeof(i32));
                        std::memcpy(uvs.ptrw(), data.uvs.data(), data.uvs.size() * sizeof(Pos2D<f32>));
                        std::memcpy(uvs_layer.ptrw(), data.uvs_layer.data(), data.uvs_layer.size() * sizeof(Pos2D<f32>));

                        auto arrays = Array();
                        arrays.resize(Mesh::ARRAY_MAX);

                        arrays[Mesh::ARRAY_VERTEX] = vertices;
                        arrays[Mesh::ARRAY_NORMAL] = normals;
                        arrays[Mesh::ARRAY_INDEX] = indices;
                        arrays[Mesh::ARRAY_TEX_UV] = uvs;
                        arrays[Mesh::ARRAY_TEX_UV2] = uvs_layer;

                        auto mesh = Ref<ArrayMesh>();
                        mesh.instantiate();

                        mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
                        update_chunk_mesh(chunk_render, chunk_pos, mesh, sub_idx);

                        auto const current_size = i32(total_collision_faces.size());
                        total_collision_faces.resize(current_size + data.collision_faces.size());

                        std::memcpy(total_collision_faces.ptrw() + current_size, data.collision_faces.data(), data.collision_faces.size() * sizeof(Pos3D<f32>));
                    }

                    if (data.complex_instance.empty()) {
                        ++updates_this_frame;
                        continue;
                    }

                    if (not chunk_render.mesh_instances[sub_idx]) update_chunk_mesh(chunk_render, chunk_pos, nullptr, sub_idx);
                    if (not chunk_render.dynamic_body) {
                        chunk_render.dynamic_body = memnew(StaticBody3D);
                        chunk_render.mesh_instances[sub_idx]->add_child(chunk_render.dynamic_body);
                    }
                    else while (chunk_render.dynamic_body->get_child_count() > 0) {
                        auto child = chunk_render.dynamic_body->get_child(0);
                        chunk_render.dynamic_body->remove_child(child);
                        child->queue_free();
                    }

                    auto grouped_instances = std::unordered_map<u32, std::vector<ComplexBlockInstance>>();
                    for (auto&& inst : data.complex_instance) {
                        auto box = Ref<BoxShape3D>();
                        box.instantiate();
                        box->set_size(Vector3(1.0f, 0.1f, 1.0f));

                        auto col_shape = memnew(CollisionShape3D);
                        col_shape->set_shape(box);
                        col_shape->set_position(Vector3(inst.local_pos.x + 0.5f, inst.local_pos.y + 0.5f, inst.local_pos.z + 0.5f));

                        chunk_render.dynamic_body->add_child(col_shape);
                        grouped_instances[inst.block_id].emplace_back(std::move(inst));
                    }

                    for (auto const& [id, instances] : grouped_instances) {
                        if (id >= block_registry::registry.size()) continue;

                        auto multimesh = Ref<MultiMesh>();
                        multimesh.instantiate();
                        multimesh->set_transform_format(MultiMesh::TRANSFORM_3D);
                        multimesh->set_mesh(block_registry::registry[id].mesh);
                        multimesh->set_instance_count(i32(instances.size()));

                        for (auto i : std::views::iota(0uz, instances.size())) {
                            auto const& inst = instances[i];
                            auto transform = Transform3D();
                            transform.origin = Vector3(inst.local_pos.x, inst.local_pos.y + 0.01f, inst.local_pos.z);
                            multimesh->set_instance_transform(i32(i), transform);
                        }

                        if (not chunk_render.multi_mesh_instance) {
                            chunk_render.multi_mesh_instance = memnew(MultiMeshInstance3D);
                            chunk_render.mesh_instances[sub_idx]->add_child(chunk_render.multi_mesh_instance);
                        }

                        chunk_render.multi_mesh_instance->set_multimesh(multimesh);
                    }
                }

                if (not total_collision_faces.is_empty()) {
                    create_chunk_collision(chunk_render, total_collision_faces);
                }

                ++updates_this_frame;
            }
        }

        {
            auto const lock = std::lock_guard(ready_chunks_queue_mutex);
            for (auto& chunk_pos : deferred_chunks) ready_chunks_queue.push_back(chunk_pos);
        }
    }

    auto Main::_exit_tree() -> void {
		running.store(false, std::memory_order::relaxed);
        loop_cv.notify_all();
        save_userdata();
    }

    auto Main::init_singleplayer() -> void {
        log<LogType::INFO>("Starting local server...");
        server_ptr = std::make_unique<World>();
        server_ptr->connect(player_name);

        if (auto player = static_cast<Player*>(player_ptr)) {
            auto const lock = std::unique_lock(player_mutex);
            player->set_global_position(server_ptr->players[player_name].pos);
        }

        start_scheduler_thread();
    }

    auto Main::init_multiplayer() -> void { start_network_thread(); }

    auto Main::setup_voxel_material() -> void {
        auto mat = Ref<ShaderMaterial>();
        mat.instantiate();

        static constexpr auto shader_path = "res://assets/shaders/visual.gdshader"fs;
        static auto const g_shader_path = to_gstring(shader_path);

        auto shader = Ref<Shader>();
        shader.instantiate();

        if (FileAccess::file_exists(g_shader_path)) {
            auto const file = FileAccess::open(g_shader_path, FileAccess::READ);

            if (file.is_valid()) shader->set_code(file->get_as_text());
			else log<LogType::ERROR>("Failed to open shader file at: {}"f(shader_path));
        }
        else log<LogType::ERROR>("Shader file not found at: {}"f(shader_path));

        mat->set_shader(shader);
        mat->set_shader_parameter("u_texture_array", atlas_texture::atlas_texture);

        world_material = mat;
    }

    auto Main::start_log_thread() -> void {
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

    auto Main::start_network_thread() -> void {
        if (network_thread.joinable()) return;

        auto worker = [this] {
            thread_registry::register_thread("Network");
            log<LogType::INFO>("Network thread started");

            auto client_peer = Ref<StreamPeerTCP>();
            client_peer.instantiate();

            log<LogType::INFO>("Connecting to server...");

        RECONNECT:
            client_peer->disconnect_from_host();
            
            if (auto err = client_peer->connect_to_host(std::get<0>(server_socket), std::get<1>(server_socket)); err != OK) {
                log<LogType::ERROR>("Connect failed");
                log<LogType::INFO>("Reconnecting to server...");
                goto RECONNECT;
            }

            while (running.load(std::memory_order::relaxed)) {
                client_peer->poll();
                auto status = client_peer->get_status();

                switch (status) {
                case StreamPeerTCP::STATUS_CONNECTED: goto BREAK;

                case StreamPeerTCP::STATUS_ERROR:
                case StreamPeerTCP::STATUS_NONE:
                    log<LogType::WARNING>("Lost connect to server");
                    log<LogType::INFO>("Reconnecting to server...");
                    goto RECONNECT;
                }

                std::this_thread::sleep_for(10ms);
                continue;

            BREAK:
                break;
            }

            send_queue.store({ "Connect", { player_name }});
            auto buffer = ""s;

            while (running.load(std::memory_order::relaxed)) {
                send_queue.send(**client_peer);
                std::ranges::fill(buffer, 0);

                auto const recv_state = receive_queue.receive(**client_peer, buffer);

                switch (recv_state) {
                case ReceiveState::WAITING:
                    std::this_thread::sleep_for(100ms);
                    continue;

                case ReceiveState::ERROR:
                    log<LogType::WARNING>("Lost connect to server");
                    log<LogType::INFO>("Reconnecting to server...");
                    goto RECONNECT;
                }

                auto const message = ReceiveQueue::parse(buffer);
                if (message.content.empty()) continue;

                if (message.content == "Connected") {
                    log<LogType::INFO>("Connected to server");

                    auto player_pos = Pos3D{
                        fsize(std::stod(message.arguments[0])),
                        fsize(std::stod(message.arguments[1])),
                        fsize(std::stod(message.arguments[2]))
                    };

                    if (auto const& server_version = message.arguments[3]; server_version != std::string_view(VERSION)) {
                        log<LogType::WARNING>("Server version({}) mismatch with current version({})"f(server_version, VERSION));
                    }

                    if (auto player = static_cast<Player*>(player_ptr)) {
                        auto const lock = std::unique_lock(player_mutex);
                        player->call_deferred("set_global_position", Vector3(player_pos));
                    }
                    break;
                }
                
                std::this_thread::sleep_for(10ms);
            }

            requested_chunks.clear();
            start_scheduler_thread();

            while (running.load(std::memory_order::relaxed)) {
                send_queue.send(**client_peer);
                std::ranges::fill(buffer, 0);

                auto const recv_state = receive_queue.receive(**client_peer, buffer);
                switch (recv_state) {
                case ReceiveState::WAITING:
                    std::this_thread::sleep_for(100ms);
                    continue;

                case ReceiveState::ERROR:
                    log<LogType::WARNING>("Lost connect to server");
                    log<LogType::INFO>("Reconnecting to server...");
                    goto RECONNECT;
                }

                auto message = ReceiveQueue::parse(buffer);
                if (message.content.empty()) continue;

                try {
                    if (message.content == "Chunk version") {
                        auto const chunk_version = u8(std::stoi(message.arguments[0]));
                        auto const [cx_str, cy_str] = std::tuple(&message.arguments[1], &message.arguments[2]);
                        auto const [cx, cy] = std::tuple(std::stoi(*cx_str), std::stoi(*cy_str));

                        if (auto chunk = get_chunk(cx, cy); not chunk or chunk->chunk_version != chunk_version) {
                            send_queue.store({ "Get chunk data",{ *cx_str, *cy_str } });
                        }
                    }
                    else if (message.content == "Chunk data") {
                        // Unzip
                        if (buffer.size() < sizeof(u32)) {
                            log<LogType::ERROR>("Received chunk data is too small({}). Packet might be corrupted"f(buffer.size()));
                            continue;
                        }

                        auto const& base64_payload = message.arguments[0];
                        auto const payload = Marshalls::get_singleton()->base64_to_raw(to_gstring(base64_payload));

                        if (payload.size() < sizeof(u32)) {
                            log<LogType::ERROR>("Received chunk data is too small({})) to contain uncompressed size. Packet might be corrupted"f(payload.size()));
                            continue;
                        }

                        auto uncompressed_size = 0u;
                        memcpy(&uncompressed_size, payload.ptr(), sizeof(u32));

                        auto const compressed_size = u64(payload.size() - sizeof(u32));
                        auto compressed_pba = PackedByteArray();
                        compressed_pba.resize(compressed_size);
                        memcpy(compressed_pba.ptrw(), payload.ptr() + sizeof(u32), compressed_size);

                        auto const decompressed_pba = compressed_pba.decompress(uncompressed_size, FileAccess::COMPRESSION_ZSTD);

                        if (decompressed_pba.is_empty() and uncompressed_size > 0) {
                            log<LogType::ERROR>("Failed to decompress chunk data. Packet might be corrupted");
                            continue;
                        }

                        // Deserialize
                        auto const world_data = std::string(reinterpret_cast<char const*>(decompressed_pba.ptr()), decompressed_pba.size());
                        auto is = std::istringstream(world_data, std::ios::binary);

                        auto const [cx, cy] = std::tuple(std::stoi(message.arguments[1]), std::stoi(message.arguments[2]));
                        auto& chunk = *get_or_create_chunk(cx, cy);

                        {
                            auto const data_lock = std::unique_lock(chunk.data_mutex);
                            chunk.clear();

                            is.read(reinterpret_cast<char*>(&chunk.blocks), sizeof chunk.blocks);

                            is.read(reinterpret_cast<char*>(&chunk.block_ids_size), sizeof chunk.block_ids_size);
                            is.read(reinterpret_cast<char*>(&chunk.block_ids), sizeof chunk.block_ids);

                            auto id2block_size = 0ub;
                            is.read(reinterpret_cast<char*>(&id2block_size), sizeof id2block_size);
                            chunk.id2block.reserve(id2block_size);
                            for (auto j : std::views::iota(0ub, id2block_size)) {
                                auto global_id = 0u;
                                is.read(reinterpret_cast<char*>(&global_id), sizeof global_id);
                                is.read(reinterpret_cast<char*>(&chunk.id2block[global_id]), sizeof chunk.id2block[global_id]);
                            }

                            auto meta_size = 0uqw;
                            is.read(reinterpret_cast<char*>(&meta_size), sizeof meta_size);
                            for (auto j : std::views::iota(0uqw, meta_size)) {
                                auto pos = Pos3D<u8>();

                                is.read(reinterpret_cast<char*>(&pos.x), sizeof pos.x);
                                is.read(reinterpret_cast<char*>(&pos.y), sizeof pos.y);
                                is.read(reinterpret_cast<char*>(&pos.z), sizeof pos.z);

                                auto& meta_storages = chunk.meta_ids[pos];

                                auto meta_storages_size = 0uqw;
                                is.read(reinterpret_cast<char*>(&meta_storages_size), sizeof meta_storages_size);
                                for (auto k : std::views::iota(0uqw, meta_storages_size)) {
                                    auto key = 0u;
                                    is.read(reinterpret_cast<char*>(&key), sizeof key);

                                    auto& value = meta_storages[key];

                                    auto data_size = 0uqw;
                                    is.read(reinterpret_cast<char*>(&data_size), sizeof data_size);
                                    value.resize(data_size);
                                    is.read(reinterpret_cast<char*>(value.data()), data_size);
                                }
                            }

                            auto tag_size = 0uqw;
                            is.read(reinterpret_cast<char*>(&tag_size), sizeof tag_size);
                            for (auto j : std::views::iota(0uqw, tag_size)) {
                                auto pos = Pos3D<u8>();

                                is.read(reinterpret_cast<char*>(&pos.x), sizeof pos.x);
                                is.read(reinterpret_cast<char*>(&pos.y), sizeof pos.y);
                                is.read(reinterpret_cast<char*>(&pos.z), sizeof pos.z);

                                auto& tag_storages = chunk.tag_ids[pos];

                                auto tag_storages_size = 0uqw;
                                is.read(reinterpret_cast<char*>(&tag_storages_size), sizeof tag_storages_size);
                                for (auto k : std::views::iota(0uqw, tag_storages_size)) {
                                    u32 key = 0;
                                    is.read(reinterpret_cast<char*>(&key), sizeof key);
                                    tag_storages.insert(key);
                                }
                            }

                            auto extended_block_size = 0uqw;
                            is.read(reinterpret_cast<char*>(&extended_block_size), sizeof extended_block_size);

                            for (auto j : std::views::iota(0uqw, extended_block_size)) {
                                auto pos = Pos3D<u8>();

                                is.read(reinterpret_cast<char*>(&pos.x), sizeof pos.x);
                                is.read(reinterpret_cast<char*>(&pos.y), sizeof pos.y);
                                is.read(reinterpret_cast<char*>(&pos.z), sizeof pos.z);

                                is.read(reinterpret_cast<char*>(&chunk.extended_block_id[pos]), sizeof chunk.extended_block_id[pos]);
                            }

                            is.read(reinterpret_cast<char*>(&chunk.chunk_version), sizeof chunk.chunk_version);

                            chunk.generated.store(true, std::memory_order::release);
                            chunk.dirty.store(true, std::memory_order::release);

                            static constexpr auto offsets = std::array{Pos2D(1, 0), Pos2D(-1, 0), Pos2D(0, 1), Pos2D(0, -1)};
                            for (auto& o : offsets) {
                                auto n = get_chunk(chunk.chunk_pos.x + o.x, chunk.chunk_pos.y + o.y);
                                if (n and n->generated.load(std::memory_order::acquire)) n->dirty.store(true);
                            }
                        }

                        auto const lock = std::lock_guard(requested_chunks_mutex);
                        requested_chunks.erase({ cx, cy });
                    }
                    else if (message.content == "Chunk not ready") {
                        auto const lock = std::lock_guard(requested_chunks_mutex);
                        requested_chunks.erase({ std::stoi(message.arguments[0]), std::stoi(message.arguments[1]) });
                    }
                    else if (message.content == "Players data") {
                        auto is = std::istringstream(message.arguments[0], std::ios::binary);

                        auto player_count = 0uqw;
                        is.read(reinterpret_cast<char*>(&player_count), sizeof player_count);

                        for (auto i : std::views::iota(0uqw, player_count)) {
                            auto name_len = 0uqw;
                            auto name = ""s;
                            auto player_data = PlayerData();

                            is.read(reinterpret_cast<char*>(&name_len), sizeof name_len);
                            name.resize(name_len);

                            is.read(reinterpret_cast<char*>(name.data()), name_len);
                            is.read(reinterpret_cast<char*>(&player_data.hp), sizeof player_data.hp);
                            is.read(reinterpret_cast<char*>(&player_data.pos), sizeof player_data.pos);
                            is.read(reinterpret_cast<char*>(&player_data.hotbar), sizeof player_data.hotbar);

                            if (name == player_name) {
                                auto player = static_cast<Player*>(player_ptr);
                                if (not player) continue;

                                auto const lock = std::unique_lock(player_mutex);
                                player->hp = player_data.hp;
                                memcpy(&player->hotbar, &player_data.hotbar, sizeof player_data.hotbar);
                            }
                        }
                    }
                    else if (message.content == "Chat response") call_deferred("emit_signal", "chat_output", to_gstring(message.arguments[0]));
                }
                catch (std::exception const& e) {
                    log<LogType::ERROR>("Error processing message: {} - {}"f(message.content, e.what()));
                }

                std::this_thread::sleep_for(10ms);
            }

            client_peer->disconnect_from_host();
        };

        network_thread = std::jthread(worker);
    }

    auto Main::start_scheduler_thread() -> void {
        auto worker = [this] {
            thread_registry::register_thread("Mesh");
            log<LogType::INFO>("Mesh thread started");

            auto last_unload_time = std::chrono::steady_clock::now();
            while (running.load(std::memory_order::relaxed)) {
                submit_jobs();

                if (auto now = std::chrono::steady_clock::now(); std::chrono::duration_cast<std::chrono::seconds>(now - last_unload_time).count() >= 5) {
                    unload_distant_chunks();
                    last_unload_time = now;
                }

                auto lock = std::unique_lock(loop_mutex);
                loop_cv.wait_for(lock, std::chrono::milliseconds(cpu_sleep_time));
            }
        };

        scheduler_thread = std::jthread(worker);
    }

    auto Main::submit_jobs() -> void {
        auto const px = i32(std::floor(player_x.load() / Chunk::WIDTH));
        auto const pz = i32(std::floor(player_z.load() / Chunk::WIDTH));

        auto get_chunk_server_or_not = +[](Main& self, Pos2D<i32> const& chunk_pos) {
            auto chunk = self.server_ptr->get_chunk(chunk_pos.x, chunk_pos.y);
            self.set_chunk(chunk, chunk_pos.x, chunk_pos.y);
            return chunk;
        };

        if (not server_ptr) {
            get_chunk_server_or_not = +[](Main& self, Pos2D<i32> const& chunk_pos) {
                auto const lock = std::lock_guard(self.requested_chunks_mutex);
                if (not self.requested_chunks.contains({ chunk_pos.x, chunk_pos.y })) {
                    self.send_queue.store({ "Get chunk version", { std::to_string(chunk_pos.x), std::to_string(chunk_pos.y) } });
                    self.requested_chunks.insert({ chunk_pos.x, chunk_pos.y });
                }
                return self.get_chunk(chunk_pos.x, chunk_pos.y);
            };
        }

        auto process_cell = [&](i32 x, i32 z) {
            auto const chunk_pos = Pos2D(px + x, pz + z);
            auto chunk_ptr = get_chunk_server_or_not(*this, chunk_pos);

            if (not chunk_ptr) return;
            auto& chunk = *chunk_ptr;

            set_chunk(chunk_ptr, chunk_pos.x, chunk_pos.y);

            if (not chunk.dirty.load(std::memory_order::acquire)) return;

            {
                auto const lock = std::lock_guard(pending_jobs_mutex);
                if (pending_mesh_jobs.contains(chunk_pos)) return;
                pending_mesh_jobs.insert(chunk_pos);
            }

            mesh_pool.enqueue([this, chunk_ptr]() {
                auto& chunk = *chunk_ptr;

                if (running.load(std::memory_order::relaxed)) {
                    auto neighbors = std::array{
                        get_chunk(chunk.chunk_pos.x + 1, chunk.chunk_pos.y),
                        get_chunk(chunk.chunk_pos.x - 1, chunk.chunk_pos.y),
                        get_chunk(chunk.chunk_pos.x, chunk.chunk_pos.y + 1),
                        get_chunk(chunk.chunk_pos.x, chunk.chunk_pos.y - 1)
                    };

                    chunk.generate_mesh(ref_mesh(chunk.chunk_pos.x, chunk.chunk_pos.y), neighbors);

                    auto const lock = std::lock_guard(ready_chunks_queue_mutex);
                    ready_chunks_queue.push_back(chunk.chunk_pos);
                }

                auto const lock = std::lock_guard(pending_jobs_mutex);
                pending_mesh_jobs.erase(chunk.chunk_pos);

                std::this_thread::sleep_for(10ms);
            });
        };

        process_cell(0, 0);

        for (auto r : std::views::iota(1, render_distance + 1)) {
            for (auto x : std::views::iota(-r, r + 1)) {
                process_cell(x, -r);
                process_cell(x, r);
            }
            for (auto z : std::views::iota(-r + 1, r)) {
                process_cell(-r, z);
                process_cell(r, z);
            }
        }
    }

    auto Main::create_chunk_collision(ChunkRender& chunk_render, PackedVector3Array const& collision_faces) -> void {
        if (not chunk_render.mesh_instances[0]) return;

        auto mesh = chunk_render.mesh_instances[0]->get_mesh();
        if (mesh.is_null() or mesh->get_surface_count() == 0) return;
        if (collision_faces.size() == 0) return;

        if (collision_faces.size() % 3 != 0) {
            log<LogType::ERROR>("Invalid faces size for collision: {}"f(collision_faces.size()));
            return;
        }

        if (not chunk_render.collision_body) {
            chunk_render.collision_body = memnew(StaticBody3D);
            chunk_render.mesh_instances[0]->add_child(chunk_render.collision_body);
        }

        if (not chunk_render.collision_shape) {
            chunk_render.collision_shape = memnew(CollisionShape3D);
            chunk_render.collision_body->add_child(chunk_render.collision_shape);
        }

        auto concave = Ref<ConcavePolygonShape3D>();
        concave.instantiate();
        concave->set_faces(collision_faces);

        chunk_render.collision_shape->set_shape(concave);
    }

    auto Main::update_chunk_mesh(ChunkRender& chunk_render, Pos2D<i32>& pos, Ref<ArrayMesh> const& mesh, i32 submesh_idx) -> void {
        if (not chunk_render.mesh_instances[submesh_idx]) {
            auto mi = memnew(MeshInstance3D);
            mi->set_position(Vector3(fsize(pos.x * Chunk::WIDTH), 0, fsize(pos.y * Chunk::WIDTH)));
            mi->set_material_override(world_material);
            add_child(mi);
            chunk_render.mesh_instances[submesh_idx] = mi;
        }

        chunk_render.mesh_instances[submesh_idx]->set_mesh(mesh);
    }

    auto Main::unload_distant_chunks() -> void {
        auto const p_cx = i32(player_x.load() / Chunk::WIDTH);
        auto const p_cy = i32(player_z.load() / Chunk::WIDTH);
        auto const unload_dist = render_distance + 4;

        auto removed = 0;
        {
            auto const lock = std::unique_lock(chunks_mutex);
            auto it = chunks.begin();

            while (it != chunks.end()) {
                auto& [chunk_pos, chunk_pair] = *it;

                auto const dx = std::abs(chunk_pos.x - p_cx);
                auto const dz = std::abs(chunk_pos.y - p_cy);

                if (dx > unload_dist or dz > unload_dist) {
                    std::get<1>(chunk_pair).clear();
                    it = chunks.erase(it);
                    ++removed;
                }
                else ++it;
            }
        }

        if (removed) log<LogType::VERBOSE>("Queued unload for {} chunks"f(removed));
    }

    auto Main::get_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk> {
        auto const cpos = Pos2D(cx, cy);

        auto const lock = std::shared_lock(chunks_mutex);
        auto it = chunks.find(cpos);

        if (it == chunks.end()) return nullptr;
        return std::get<0>(it->second);
    }

    auto Main::get_or_create_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk> {
        auto const chunk_pos = Pos2D(cx, cy);
        {
            auto const lock = std::shared_lock(chunks_mutex);
            auto it = chunks.find(chunk_pos);
            if (it != chunks.end()) return std::get<0>(it->second);
        }

        auto const lock = std::unique_lock(chunks_mutex);

        auto chunk_ptr = std::make_shared<Chunk>();
        chunk_ptr->chunk_pos = chunk_pos;

        auto& chunk = std::get<0>(chunks[chunk_pos]);
        chunk.swap(chunk);

        return chunk;
    }

    auto Main::ref_mesh(i32 cx, i32 cy) -> ChunkRender& {
        auto const cpos = Pos2D(cx, cy);

        std::unique_lock lock(chunks_mutex);
        return std::get<1>(chunks[cpos]);
    }
    
    auto Main::get_global_block_id(i32 wx, i32 wy, i32 wz) -> u32 {
        if (wy < 0 or wy >= Chunk::HEIGHT) return block_registry::get_id("Air");

        auto const cx = i32(std::floor(f32(wx) / Chunk::WIDTH));
        auto const cy = i32(std::floor(f32(wz) / Chunk::WIDTH));
        auto const cpos = Pos2D(cx, cy);

        auto chunk = get_chunk(cx, cy);
        if (not chunk) return block_registry::get_id("Air");

        auto const lx = (wx % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;
        auto const lz = (wz % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;

        return chunk->get_block({ u8(lx), u8(wy), u8(lz) });
    }

    auto Main::set_chunk(std::shared_ptr<Chunk>& chunk, i32 cx, i32 cy) -> void {
        auto const cpos = Pos2D(cx, cy);

        auto const lock = std::unique_lock(chunks_mutex);
		std::get<0>(chunks[cpos]) = chunk;
    }

    auto Main::set_global_block_id(u32 block_id, i32 wx, i32 wy, i32 wz) -> void {
        if (wy < 0 or wy >= Chunk::HEIGHT) return;
        if (not server_ptr) send_queue.store({ "std::unordered_set block", { std::to_string(block_id), std::to_string(wx), std::to_string(wy), std::to_string(wz) } });
        
        auto const cx = i32(std::floor(f32(wx) / Chunk::WIDTH));
        auto const cy = i32(std::floor(f32(wz) / Chunk::WIDTH));
        auto const cpos = Pos2D(cx, cy);

        auto chunk = get_chunk(cx, cy);
        if (not chunk) return;

        auto const lx = (wx % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;
        auto const lz = (wz % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;

        chunk->set_block({ u8(lx), u8(wy), u8(lz) }, block_id);
    }

    auto Main::save_userdata(std::string_view path) -> void {
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring(path));
        auto const std_path = std::string(real_path.utf8());

        // Tạo thư mục
        std::filesystem::create_directories(std::filesystem::path(std_path).parent_path());

        auto ofs = std::ofstream(std_path, std::ios::binary);
        if (not ofs.is_open()) {
            log<LogType::ERROR>("Cannot open save file: {}"f(std_path));
            return;
        }

        auto const lock = std::shared_lock(player_mutex);
        auto player = static_cast<Player*>(player_ptr);
        if (not player) return;

        ofs.write(reinterpret_cast<char const*>(&full_screen), sizeof full_screen);
        ofs.write(reinterpret_cast<char const*>(&player->sensitivity), sizeof player->sensitivity);
        ofs.write(reinterpret_cast<char const*>(&player->mouse_pitch), sizeof player->mouse_pitch);
        ofs.write(reinterpret_cast<char const*>(&render_distance), sizeof render_distance);
    }

    auto Main::load_userdata(std::string_view path) -> bool {
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring(path));
        auto const std_path = std::string(real_path.utf8());

        auto ifs = std::ifstream(std_path, std::ios::binary);
        if (not ifs.is_open()) return false;

        {
            auto const lock = std::shared_lock(player_mutex);
            auto player = static_cast<Player*>(player_ptr);
            if (not player) return false;

            ifs.read(reinterpret_cast<char*>(&full_screen), sizeof full_screen);
            ifs.read(reinterpret_cast<char*>(&player->sensitivity), sizeof player->sensitivity);
            ifs.read(reinterpret_cast<char*>(&player->mouse_pitch), sizeof player->mouse_pitch);
            ifs.read(reinterpret_cast<char*>(&render_distance), sizeof render_distance);
        }

		log<LogType::INFO>("Userdata loaded");
        return true;
    }

    auto Main::pause() -> void {
        auto input = Input::get_singleton();
        input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
        pausing.store(true, std::memory_order::relaxed);
    }
    auto Main::resume() -> void {
        auto input = Input::get_singleton();
        input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
        pausing.store(false, std::memory_order::relaxed);
    }

    auto Main::start_chat() -> void {
        auto input = Input::get_singleton();
        input->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
        chatting.store(true, std::memory_order::relaxed);
    }

    auto Main::chat(String const msg) -> void {
        if (not server_ptr) send_queue.store({ "Chat", { std::string(msg.utf8()) } });
        else {
            auto const output = server_ptr->chat(std::string(msg.utf8()));
			emit_signal("chat_output", to_gstring(output));
        }

        auto input = Input::get_singleton();
        input->set_mouse_mode(Input::MOUSE_MODE_CAPTURED);
        chatting.store(false, std::memory_order::relaxed);
    }

	auto Main::get_player_position() -> Vector3 {
        return { player_x.load(std::memory_order::relaxed), player_y.load(std::memory_order::relaxed), player_z.load(std::memory_order::relaxed) };
	}

    void Main::set_seed_and_world_name(i32 seed, const String name) {
        if (not server_ptr) send_queue.store({ "std::unordered_set seed and world name", { std::to_string(seed), std::string(name.utf8()) } });
        else server_ptr->set_seed_and_world_name(seed, std::string(name.utf8()));
    }

    void Main::set_render_distance(i32 rd) {
        if (not server_ptr) send_queue.store({ "std::unordered_set render distance", { std::to_string(rd) } });
        else server_ptr->set_render_distance(rd);

        if (auto sky = get_node<CraftSky>("CraftSky")) sky->reset_fog_distance();
    }

    void Main::set_cpu_sleep_time(i32 stc) {
        if (not server_ptr) send_queue.store({ "std::unordered_set sleep time CPU", { std::to_string(stc) } });
        else server_ptr->set_cpu_sleep_time(stc);
    }

    void Main::set_server_socket(String ip, i32 port) {
        server_socket = std::tuple(ip, port);
    }
    
    void Main::_bind_methods() {
        ADD_SIGNAL(MethodInfo("chat_output", PropertyInfo(Variant::STRING, "line")));
        ClassDB::bind_method(D_METHOD("init"), &Main::_ready);
        ClassDB::bind_method(D_METHOD("init_singleplayer"), &Main::init_singleplayer);
        ClassDB::bind_method(D_METHOD("init_multiplayer"), &Main::init_multiplayer);
        ClassDB::bind_method(D_METHOD("process"), &Main::_process);
        ClassDB::bind_method(D_METHOD("pause_game"), &Main::pause);
        ClassDB::bind_method(D_METHOD("resume_game"), &Main::resume);
        ClassDB::bind_method(D_METHOD("start_chat"), &Main::start_chat);
        ClassDB::bind_method(D_METHOD("chat", "msg"), &Main::chat);
        ClassDB::bind_method(D_METHOD("get_player_position"), &Main::get_player_position);
        ClassDB::bind_method(D_METHOD("set_seed_and_world_name", "seed", "name"), &Main::set_seed_and_world_name);
        ClassDB::bind_method(D_METHOD("set_render_distance", "rd"), &Main::set_render_distance);
        ClassDB::bind_method(D_METHOD("set_cpu_sleep_time", "stc"), &Main::set_cpu_sleep_time);
        ClassDB::bind_method(D_METHOD("set_server_socket", "stc"), &Main::set_server_socket);
    }
}
