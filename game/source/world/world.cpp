module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/marshalls.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
ENABLE_WARNING

module game.world;

using namespace std::literals;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    World::World() : interpreter(this) {
        noise.instantiate();
        noise->set_noise_type(FastNoiseLite::TYPE_SIMPLEX);
        noise->set_frequency(0.01f);

        if (not load_world("user://game/saves/{}"f(world_name))) {
            log<LogType::WARNING>("Save file not found, starting new world.");
            if (world_seed.load(std::memory_order::acquire) == 0) {
                auto generator = std::mt19937();
                auto distribution = std::uniform_int_distribution<i32>();
                world_seed.store(distribution(generator), std::memory_order::release);
            }
        }
        noise->set_seed(world_seed.load(std::memory_order::acquire));

        log<LogType::VERBOSE>("Assets loaded");

        start_redstone_thread();
        start_scheduler_thread();

        log<LogType::INFO>("Server initialized");
    }

    World::~World() {
        running.store(false, std::memory_order::relaxed);
        loop_cv.notify_all();
        save_world("user://game/saves/{}"f(world_name));
    }

    auto World::disconnect(std::string_view player_name) -> void {
        if (not players.contains(player_name)) return;

        auto const lock = std::unique_lock(player_mutex);
        online_players.erase(player_name);

        {
            auto const lock = std::lock_guard(current_player_mutex);
            current_player = online_players.begin();
        }

        log<LogType::INFO>("Player disconnected: {}"f(player_name));
    }

    auto World::connect(std::string_view player_name) -> void {
        auto const lock = std::unique_lock(player_mutex);
        auto const player_name_str = std::string(player_name);

        online_players[player_name_str];

        {
            auto const lock = std::lock_guard(current_player_mutex);
            current_player = online_players.begin();
        }

        if (players.contains(player_name)) return;
        players[player_name_str] = PlayerData{ .name = player_name_str, .pos = { 0, 0, 0 } };

        log<LogType::INFO>("Player connected: {}"f(player_name));
    }

    auto World::update(std::string_view player_name, Pos3D<fsize> const& new_pos) -> void {
        auto const lock = std::unique_lock(player_mutex);
        players[std::string(player_name)].pos = new_pos;
    }

    auto World::start_redstone_thread() -> void {
        if (redstone_thread.joinable()) return;

        auto worker = [this] {
            thread_registry::register_thread("Redstone");
            log<LogType::INFO>("Redstone thread started");

            while (running.load(std::memory_order::relaxed)) {
                std::this_thread::sleep_for(10ms);
            }
        };

        redstone_thread = std::jthread(worker);
    }

    auto World::start_scheduler_thread() -> void {
        auto worker = [this] {
            thread_registry::register_thread("Terrain");
            log<LogType::INFO>("Terrain thread started");

            auto last_unload_time = std::chrono::steady_clock::now();
            current_player = online_players.begin();

            while (running.load(std::memory_order::relaxed)) {
                if (auto lock = std::shared_lock(player_mutex); online_players.empty()) {
                    lock.unlock();
                    std::this_thread::sleep_for(500ms);
                    continue;
                }

                if (auto now = std::chrono::steady_clock::now(); std::chrono::duration_cast<std::chrono::seconds>(now - last_unload_time).count() >= 5) {
                    unload_distant_chunks();
                    last_unload_time = now;
                }

                {
                    auto const lock1 = std::lock_guard(current_player_mutex);
                    auto pos = Pos3D<fsize>();
                    {
                        auto const lock = std::shared_lock(player_mutex);
                        pos = players[current_player->first].pos;
                    }

                    submit_jobs(pos);

                    auto const lock2 = std::shared_lock(player_mutex);
                    if (++current_player == online_players.end()) current_player = online_players.begin();
                }

                auto lock = std::unique_lock(loop_mutex);
                loop_cv.wait_for(lock, std::chrono::milliseconds(cpu_sleep_time));
            }
        };

        scheduler_thread = std::jthread(worker);
    }

    auto World::submit_jobs(Pos3D<fsize> const& player) -> void {
        auto const px = i32(std::floor(player.x / Chunk::WIDTH));
        auto const pz = i32(std::floor(player.z / Chunk::WIDTH));

        auto get_or_load_or_create_chunk = [this](i32 cx, i32 cy) -> std::shared_ptr<Chunk> {
            if (auto chunk_ptr = get_or_load_chunk(cx, cy)) return chunk_ptr;
            return get_or_create_chunk(cx, cy);
        };

        auto process_cell = [&](i32 cx, i32 cy) {
            auto chunk_pos = Pos2D<i32>(px + cx, pz + cy);
            auto chunk_ptr = get_or_load_or_create_chunk(chunk_pos.x, chunk_pos.y);

            if (chunk_ptr->generated.load(std::memory_order::acquire)) return;

            {
                auto const lock = std::lock_guard(pending_jobs_mutex);
                if (pending_terrain_jobs.contains(chunk_pos)) return;
                pending_terrain_jobs.insert(chunk_pos);
            }

            terrain_pool.enqueue([this, chunk_ptr]() {
                auto& chunk = *chunk_ptr;

                if (running.load(std::memory_order::relaxed)) {
                    chunk.generate_terrain(world_seed.load(), noise);

                    static constexpr auto offsets = std::array{Pos2D(1, 0), Pos2D(-1, 0), Pos2D(0, 1), Pos2D(0, -1)};
                    for (auto& o : offsets) {
                        auto n_ptr = get_or_load_chunk(chunk.chunk_pos.x + o.x, chunk.chunk_pos.y + o.y);
                        if (not n_ptr) continue;

                        auto& n = *n_ptr;
                        if (n.generated.load(std::memory_order::acquire)) {
                            n.dirty.store(true);
                            ++n.chunk_version;
                        }
                    }
                }

                auto const lock = std::lock_guard(pending_jobs_mutex);
                pending_terrain_jobs.erase(chunk.chunk_pos);

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

    auto World::serialize_players() -> std::string {
        auto os = std::ostringstream();

        {
            auto const lock = std::shared_lock(player_mutex);
            auto const player_count = u64(online_players.size());
            os.write(reinterpret_cast<char const*>(&player_count), sizeof(u64));

            for (auto const& [player_name, _] : online_players) {
                auto const& player_data = players[player_name];

                u64 name_len = player_name.size();
                os.write(reinterpret_cast<char const*>(&name_len), sizeof name_len);
                os.write(reinterpret_cast<char const*>(player_name.data()), name_len);
                os.write(reinterpret_cast<char const*>(&player_data.hp), sizeof player_data.hp);
                os.write(reinterpret_cast<char const*>(&player_data.pos), sizeof player_data.pos);
                os.write(reinterpret_cast<char const*>(&player_data.hotbar), sizeof player_data.hotbar);
            }
        }

        return os.str();
    }

    auto World::serialize_chunk(i32 cx, i32 cy) -> std::string {
        auto os = std::ostringstream(std::ios::binary | std::ios::out);

        // Get & serialize chunk data
        auto chunk_ptr = get_chunk(cx, cy);
        if (not chunk_ptr) return "";
        auto& chunk = *chunk_ptr;

        {
            auto const data_lock = std::shared_lock(chunk.data_mutex);

            os.write(reinterpret_cast<char const*>(&chunk.blocks), sizeof chunk.blocks);

            os.write(reinterpret_cast<char const*>(&chunk.block_ids_size), sizeof chunk.block_ids_size);
            os.write(reinterpret_cast<char const*>(&chunk.block_ids), sizeof chunk.block_ids);

            auto id2block_size = u8(chunk.id2block.size());
            os.write(reinterpret_cast<char const*>(&id2block_size), sizeof id2block_size);
            for (auto const& [global_id, local_id] : chunk.id2block) {
                os.write(reinterpret_cast<char const*>(&global_id), sizeof global_id);
                os.write(reinterpret_cast<char const*>(&local_id), sizeof local_id);
            }

            auto meta_size = u64(chunk.meta_ids.size());
            os.write(reinterpret_cast<char const*>(&meta_size), sizeof meta_size);
            for (auto const& [pos, meta_storages] : chunk.meta_ids) {
                os.write(reinterpret_cast<char const*>(&pos.x), sizeof pos.x);
                os.write(reinterpret_cast<char const*>(&pos.y), sizeof pos.y);
                os.write(reinterpret_cast<char const*>(&pos.z), sizeof pos.z);

                auto meta_storages_size = u64(meta_storages.size());
                os.write(reinterpret_cast<char const*>(&meta_storages_size), sizeof meta_storages_size);
                for (auto const& [key, value] : meta_storages) {
                    os.write(reinterpret_cast<char const*>(&key), sizeof key);

                    u64 data_size = value.size();
                    os.write(reinterpret_cast<char const*>(&data_size), sizeof data_size);
                    os.write(reinterpret_cast<char const*>(value.data()), data_size);
                }
            }

            auto tag_size = u64(chunk.tag_ids.size());
            os.write(reinterpret_cast<char const*>(&tag_size), sizeof tag_size);
            for (auto const& [pos, tag_storages] : chunk.tag_ids) {
                os.write(reinterpret_cast<char const*>(&pos.x), sizeof pos.x);
                os.write(reinterpret_cast<char const*>(&pos.y), sizeof pos.y);
                os.write(reinterpret_cast<char const*>(&pos.z), sizeof pos.z);

                auto tag_storages_size = u64(tag_storages.size());
                os.write(reinterpret_cast<char const*>(&tag_storages_size), sizeof tag_storages_size);
                for (auto key : tag_storages) {
                    os.write(reinterpret_cast<char const*>(&key), sizeof key);
                }
            }

            auto extended_block_size = u64(chunk.extended_block_id.size());
            os.write(reinterpret_cast<char const*>(&extended_block_size), sizeof extended_block_size);

            for (auto const& [pos, block_id] : chunk.extended_block_id) {
                os.write(reinterpret_cast<char const*>(&pos.x), sizeof pos.x);
                os.write(reinterpret_cast<char const*>(&pos.y), sizeof pos.y);
                os.write(reinterpret_cast<char const*>(&pos.z), sizeof pos.z);

                os.write(reinterpret_cast<char const*>(&block_id), sizeof block_id);
            }

            os.write(reinterpret_cast<char const*>(&chunk.chunk_version), sizeof chunk.chunk_version);
        }

        // Zip
        auto raw_data = os.str();
        auto uncompressed_size = u32(raw_data.size());

        auto pba = PackedByteArray();
        pba.resize(uncompressed_size);
        std::memcpy(pba.ptrw(), raw_data.data(), uncompressed_size);

        auto compressed_pba = pba.compress(FileAccess::COMPRESSION_ZSTD);

        auto final_payload = PackedByteArray();
        final_payload.resize(sizeof uncompressed_size + compressed_pba.size());

        std::memcpy(final_payload.ptrw(), &uncompressed_size, sizeof uncompressed_size);
        std::memcpy(final_payload.ptrw() + sizeof uncompressed_size, compressed_pba.ptr(), compressed_pba.size());

        return std::string(Marshalls::get_singleton()->raw_to_base64(final_payload).utf8());
    }

    auto World::get_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk> {
        auto const cpos = Pos2D<i32>(cx, cy);

        auto lock = std::shared_lock(chunks_mutex);
        auto it = chunks.find(cpos);

        if (it == chunks.end()) return nullptr;
        return it->second;
    }

    auto World::get_or_load_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk> {
        if (auto chunk_ptr = get_chunk(cx, cy)) return chunk_ptr;

        auto const rx = (cx >= 0) ? (cx / 16) : ((cx - 15) / 16);
        auto const ry = (cy >= 0) ? (cy / 16) : ((cy - 15) / 16);

        auto const path = "user://game/saves/{}"f(world_name);
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring("{}/regions/{}_{}.cbregion"f(path, rx, ry)));
        auto const std_path = std::string(real_path.utf8());

        auto const chunk_pos = Pos2D<i32>(cx, cy);

        if (std::filesystem::exists(std_path)) {
            load_region(path, rx, ry);

            auto lock = std::shared_lock(chunks_mutex);
            auto it_loaded = chunks.find(chunk_pos);
            if (it_loaded != chunks.end()) return it_loaded->second;
        }

        return nullptr;
    };

    auto World::get_or_create_chunk(i32 cx, i32 cy) -> std::shared_ptr<Chunk> {
        auto const chunk_pos = Pos2D<i32>(cx, cy);
        {
            auto const lock = std::shared_lock(chunks_mutex);
            auto it = chunks.find(chunk_pos);
            if (it != chunks.end()) return it->second;
        }

        auto const lock = std::unique_lock(chunks_mutex);

        auto& chunk = chunks[chunk_pos];
        chunk = std::make_shared<Chunk>();
        chunk->chunk_pos = chunk_pos;

        return chunk;
    }

    auto World::get_global_block_id(i32 wx, i32 wy, i32 wz) -> u32 {
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

    auto World::set_global_block_id(u32 block_id, i32 wx, i32 wy, i32 wz) -> void {
        if (wy < 0 or wy >= Chunk::HEIGHT) return;

        auto const cx = i32(std::floor(f32(wx) / Chunk::WIDTH));
        auto const cy = i32(std::floor(f32(wz) / Chunk::WIDTH));
        auto cpos = Pos2D(cx, cy);

        auto chunk = get_chunk(cx, cy);
        if (not chunk) return;

        auto lx = (wx % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;
        auto lz = (wz % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;

        chunk->set_block({ u8(lx), u8(wy), u8(lz) }, block_id);
    }

    auto World::unload_distant_chunks() -> void {
        auto const unload_dist = render_distance + 4;
        auto still_viewing_chunks = std::unordered_set<Pos2D<i32>>{};

        {
            auto const lock = std::shared_lock(chunks_mutex);
            for (auto const& [chunk_pos, _] : chunks) {
                auto const lock = std::shared_lock(player_mutex);
                for (auto const& [player_name, _] : online_players) {
                    auto& player_pos = players[player_name].pos;
                    auto const dx = std::abs(chunk_pos.x - i32(std::floor(player_pos.x / Chunk::WIDTH)));
                    auto const dz = std::abs(chunk_pos.y - i32(std::floor(player_pos.z / Chunk::WIDTH)));
                    if (dx <= unload_dist and dz <= unload_dist) still_viewing_chunks.insert(chunk_pos);
                }
            }
        }

        auto regions_to_save = std::unordered_set<Pos2D<i32>>();

        {
            auto const lock = std::unique_lock(chunks_mutex);
            auto it = chunks.begin();

            while (it != chunks.end()) {
                auto const& chunk_pos = it->first;

                if (still_viewing_chunks.find(chunk_pos) == still_viewing_chunks.end()) {
                    auto const rx = (chunk_pos.x >= 0 ? chunk_pos.x : chunk_pos.x - 15) / 16;
                    auto const ry = (chunk_pos.y >= 0 ? chunk_pos.y : chunk_pos.y - 15) / 16;

                    regions_to_save.insert({ rx, ry });
                    it = chunks.erase(it);
                }
                else ++it;
            }
        }

        for (auto const& [rx, ry] : regions_to_save) {
            save_region("user://game/saves/{}"f(world_name), rx, ry);
        }
    }

    auto World::set_seed_and_world_name(i32 seed, std::string_view name) -> void {
        world_seed.store(seed, std::memory_order::release);
        noise->set_seed(seed);
        world_name = name;
    }

    auto World::set_render_distance(i32 rd) -> void { render_distance = rd; }
    auto World::set_cpu_sleep_time(i32 stc) -> void { cpu_sleep_time = stc; }

    auto World::chat(std::string_view msg) -> std::string {
        if (msg.empty()) return "";
        log<LogType::NORMAL>("[Player] {}"f(msg));

        if (not msg.starts_with("/")) return std::string(msg);
        return interpreter.execute_command(std::string(msg).erase(0, 1));
    }

    auto World::save_world(std::string_view path) -> void {
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring("{}/{}.cbworld"f(path, world_name)));
        auto const std_path = std::string(real_path.utf8());

        // Tạo thư mục
        std::filesystem::create_directories(std::filesystem::path(std_path).parent_path());

        {
            auto ofs = std::ofstream(std_path, std::ios::binary);
            if (not ofs.is_open()) {
                log<LogType::ERROR>("Cannot open save file: {}"f(std_path));
                return;
            }

            log<LogType::INFO>("Saving world...");

            auto const version_len = u64(VERSION.size());
            ofs.write(reinterpret_cast<char const*>(&version_len), sizeof version_len);
            ofs.write(VERSION.data(), version_len);

            auto const seed = u32(world_seed.load(std::memory_order::release));
            ofs.write(reinterpret_cast<char const*>(&seed), sizeof seed);

            {
                auto const lock = std::shared_lock(player_mutex);
                auto const player_count = players.size();
                ofs.write(reinterpret_cast<char const*>(&player_count), sizeof player_count);

                for (auto const& [player_name, player_data] : players) {
                    auto const player_name_len = player_name.size();
                    ofs.write(reinterpret_cast<char const*>(&player_name_len), sizeof player_name_len);
                    ofs.write(reinterpret_cast<char const*>(player_name.data()), player_name_len);
                    ofs.write(reinterpret_cast<char const*>(&player_data.hp), sizeof player_data.hp);
                    ofs.write(reinterpret_cast<char const*>(&player_data.pos), sizeof player_data.pos);
                    ofs.write(reinterpret_cast<char const*>(&player_data.hotbar), sizeof player_data.hotbar);
                }
            }
        }

        auto regions_to_save = std::unordered_set<Pos2D<i32>>{};

        {
            auto const lock = std::shared_lock(chunks_mutex);
            for (auto const& [chunk_pos, _] : chunks) {
                auto const rx = (chunk_pos.x >= 0 ? chunk_pos.x : chunk_pos.x - 15) / 16;
                auto const ry = (chunk_pos.y >= 0 ? chunk_pos.y : chunk_pos.y - 15) / 16;
                regions_to_save.insert({ rx, ry });
            }
        }

        for (auto const& [rx, ry] : regions_to_save) save_region(path, rx, ry);

        log<LogType::INFO>("World saved!");
    }

    auto World::load_world(std::string_view path) -> bool {
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring("{}/{}.cbworld"f(path, world_name)));
        
        auto ifs = std::ifstream(real_path.utf8(), std::ios::binary);
        if (not ifs.is_open()) return false;

        log<LogType::INFO>("Loading world...");

        auto version_len = 0uqw;
        ifs.read(reinterpret_cast<char*>(&version_len), sizeof version_len);

        auto current_version = std::string(version_len, '\0');
        ifs.read(current_version.data(), version_len);
        if (current_version != std::string_view(VERSION)) {
            log<LogType::WARNING>("Save version({}) mismatch with current version({})"f(current_version, VERSION));
            log<LogType::WARNING>("This game's world loader doesn't support Data Migration. World data might get damaged and cause crashes");
        }

        auto seed = 0dw;
        ifs.read(reinterpret_cast<char*>(&seed), sizeof seed);

        noise->set_seed(seed);
        world_seed.store(seed, std::memory_order::release);

        auto player_count = 0uqw;
        ifs.read(reinterpret_cast<char*>(&player_count), sizeof player_count);

        {
            auto const lock = std::unique_lock(player_mutex);
            for (auto i : std::views::iota(0uqw, player_count)) {
                auto player_name = ""s;
                auto player_name_len = 0uqw;

                ifs.read(reinterpret_cast<char*>(&player_name_len), sizeof player_name_len);
                player_name.resize(player_name_len);
                ifs.read(reinterpret_cast<char*>(player_name.data()), player_name_len);

                auto& player = players[player_name];

                ifs.read(reinterpret_cast<char*>(&player.hp), sizeof player.hp);
                ifs.read(reinterpret_cast<char*>(&player.pos), sizeof player.pos);
                ifs.read(reinterpret_cast<char*>(&player.hotbar), sizeof player.hotbar);
            }
        }

        log<LogType::INFO>("World loaded successfully!");
        return true;
    }

    auto World::save_region(std::string_view path, i32 rx, i32 ry) -> void{
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring("{}/regions/{}_{}/.cbregion"f(path, rx, ry)));
        auto const std_path = std::string(real_path.utf8());

        std::filesystem::create_directories(std::filesystem::path(std_path).parent_path());

        auto ofs = std::ofstream(std_path, std::ios::binary);
        if (not ofs.is_open()) {
            log<LogType::ERROR>("Cannot open save file: {}"f(std_path));
            return;
        }

        auto const cx = rx * 16;
        auto const cy = ry * 16;

        for (auto x : std::views::iota(cx, cx + 16)) {
            for (auto z : std::views::iota(cy, cy + 16)) {
                auto chunk_ptr = get_chunk(x, z);

                auto const chunk_exists = chunk_ptr and chunk_ptr->generated.load(std::memory_order::acquire);
                ofs.write(reinterpret_cast<char const*>(&chunk_exists), sizeof chunk_exists);

                if (not chunk_exists) continue;

                auto& chunk = *chunk_ptr;
                auto const data_lock = std::shared_lock(chunk.data_mutex);

                ofs.write(reinterpret_cast<char const*>(&chunk.blocks), sizeof chunk.blocks);

                ofs.write(reinterpret_cast<char const*>(&chunk.block_ids_size), sizeof chunk.block_ids_size);
                ofs.write(reinterpret_cast<char const*>(&chunk.block_ids), sizeof chunk.block_ids);

                auto const id2block_size = u8(chunk.id2block.size());
                ofs.write(reinterpret_cast<char const*>(&id2block_size), sizeof(u8));
                for (auto const& [global_id, local_id] : chunk.id2block) {
                    ofs.write(reinterpret_cast<char const*>(&global_id), sizeof(u32));
                    ofs.write(reinterpret_cast<char const*>(&local_id), sizeof(u8));
                }

                auto const meta_size = u64(chunk.meta_ids.size());
                ofs.write(reinterpret_cast<char const*>(&meta_size), sizeof meta_size);
                for (auto const& [pos, meta_storages] : chunk.meta_ids) {
                    ofs.write(reinterpret_cast<char const*>(&pos.x), sizeof pos.x);
                    ofs.write(reinterpret_cast<char const*>(&pos.y), sizeof pos.y);
                    ofs.write(reinterpret_cast<char const*>(&pos.z), sizeof pos.z);

                    auto const meta_storages_size = u64(meta_storages.size());
                    ofs.write(reinterpret_cast<char const*>(&meta_storages_size), sizeof meta_storages_size);
                    for (auto const& [key, value] : meta_storages) {
                        ofs.write(reinterpret_cast<char const*>(&key), sizeof(u32));

                        auto const data_size = u64(value.size());
                        ofs.write(reinterpret_cast<char const*>(&data_size), sizeof data_size);
                        ofs.write(reinterpret_cast<char const*>(value.data()), data_size);
                    }
                }

                auto const tag_size = u64(chunk.tag_ids.size());
                ofs.write(reinterpret_cast<char const*>(&tag_size), sizeof tag_size);
                for (auto const& [pos, tag_storages] : chunk.tag_ids) {
                    ofs.write(reinterpret_cast<char const*>(&pos.x), sizeof pos.x);
                    ofs.write(reinterpret_cast<char const*>(&pos.y), sizeof pos.y);
                    ofs.write(reinterpret_cast<char const*>(&pos.z), sizeof pos.z);

                    auto const tag_storages_size = u64(tag_storages.size());
                    ofs.write(reinterpret_cast<char const*>(&tag_storages_size), sizeof tag_storages_size);
                    for (auto key : tag_storages) {
                        ofs.write(reinterpret_cast<char const*>(&key), sizeof key);
                    }
                }

                auto extended_block_size = u64(chunk.extended_block_id.size());
                ofs.write(reinterpret_cast<char const*>(&extended_block_size), sizeof extended_block_size);

                for (auto const& [pos, block_id] : chunk.extended_block_id) {
                    ofs.write(reinterpret_cast<char const*>(&pos.x), sizeof pos.x);
                    ofs.write(reinterpret_cast<char const*>(&pos.y), sizeof pos.y);
                    ofs.write(reinterpret_cast<char const*>(&pos.z), sizeof pos.z);

                    ofs.write(reinterpret_cast<char const*>(&block_id), sizeof block_id);
                }
            }
        }
    }

    auto World::load_region(std::string_view path, i32 rx, i32 ry) -> bool {
        auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring("{}/regions/{}_{}.cbregion"f(path, rx, ry)));
        
        auto ifs = std::ifstream(real_path.utf8(), std::ios::binary);
        if (not ifs.is_open()) return false;

        auto const w_rx = rx * 16;
        auto const w_rz = ry * 16;

        for (auto cx : std::views::iota(w_rx, w_rx + 16)) {
            for (auto cy : std::views::iota(w_rz, w_rz + 16)) {
                bool chunk_exists = true;
                ifs.read(reinterpret_cast<char*>(&chunk_exists), sizeof chunk_exists);

                if (not chunk_exists) continue;

                auto& chunk = *get_or_create_chunk(cx, cy);
                auto const data_lock = std::unique_lock(chunk.data_mutex);

                chunk.clear();

                ifs.read(reinterpret_cast<char*>(&chunk.blocks), sizeof chunk.blocks);

                ifs.read(reinterpret_cast<char*>(&chunk.block_ids_size), sizeof chunk.block_ids_size);
                ifs.read(reinterpret_cast<char*>(&chunk.block_ids), sizeof chunk.block_ids);

                auto id2block_size = 0ub;
                ifs.read(reinterpret_cast<char*>(&id2block_size), sizeof id2block_size);
                for (auto j : std::views::iota(0ub, id2block_size)) {
                    auto global_id = 0u;
                    ifs.read(reinterpret_cast<char*>(&global_id), sizeof global_id);
                    ifs.read(reinterpret_cast<char*>(&chunk.id2block[global_id]), sizeof chunk.id2block[global_id]);
                }

                auto meta_size = 0uqw;
                ifs.read(reinterpret_cast<char*>(&meta_size), sizeof meta_size);
                for (auto j : std::views::iota(0uqw, meta_size)) {
                    auto pos = Pos3D<u8>();

                    ifs.read(reinterpret_cast<char*>(&pos.x), sizeof pos.x);
                    ifs.read(reinterpret_cast<char*>(&pos.y), sizeof pos.y);
                    ifs.read(reinterpret_cast<char*>(&pos.z), sizeof pos.z);

                    auto& meta_storages = chunk.meta_ids[pos];

                    auto meta_storages_size = 0uqw;
                    ifs.read(reinterpret_cast<char*>(&meta_storages_size), sizeof meta_storages_size);
                    for (auto k : std::views::iota(0uqw, meta_storages_size)) {
                        auto key = 0u;
                        ifs.read(reinterpret_cast<char*>(&key), sizeof key);

                        auto& value = meta_storages[key];

                        auto data_size = u64(value.size());
                        ifs.read(reinterpret_cast<char*>(&data_size), sizeof data_size);
                        ifs.read(reinterpret_cast<char*>(value.data()), data_size);
                    }
                }

                auto tag_size = 0uqw;
                ifs.read(reinterpret_cast<char*>(&tag_size), sizeof tag_size);
                for (auto j : std::views::iota(0uqw, tag_size)) {
                    auto pos = Pos3D<u8>();

                    ifs.read(reinterpret_cast<char*>(&pos.x), sizeof pos.x);
                    ifs.read(reinterpret_cast<char*>(&pos.y), sizeof pos.y);
                    ifs.read(reinterpret_cast<char*>(&pos.z), sizeof pos.z);

                    auto& tag_storages = chunk.tag_ids[pos];

                    auto tag_storages_size = 0uqw;
                    ifs.read(reinterpret_cast<char*>(&tag_storages_size), sizeof tag_storages_size);
                    for (auto k : std::views::iota(0uqw, tag_storages_size)) {
                        auto key = 0u;

                        ifs.read(reinterpret_cast<char*>(&key), sizeof key);
                        tag_storages.insert(key);
                    }
                }

                auto extended_block_size = 0uqw;
                ifs.read(reinterpret_cast<char*>(&extended_block_size), sizeof extended_block_size);

                for (auto j : std::views::iota(0uqw, extended_block_size)) {
                    auto pos = Pos3D<u8>();

                    ifs.read(reinterpret_cast<char*>(&pos.x), sizeof pos.x);
                    ifs.read(reinterpret_cast<char*>(&pos.y), sizeof pos.y);
                    ifs.read(reinterpret_cast<char*>(&pos.z), sizeof pos.z);

                    ifs.read(reinterpret_cast<char*>(&chunk.extended_block_id[pos]), sizeof chunk.extended_block_id[pos]);
                }

                chunk.chunk_version = 1;
                chunk.generated.store(true, std::memory_order::release);
                chunk.dirty.store(true, std::memory_order::release);
            }
        }

        return true;
    }
}