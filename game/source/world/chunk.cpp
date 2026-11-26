module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/fast_noise_lite.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
ENABLE_WARNING

module game.world.chunk;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    constexpr FaceMask::FaceMask() {}
    constexpr FaceMask::FaceMask(i32 layer, bool back_face) : value((layer + 1) | (back_face ? BACK_FACE_BIT : 0)) {}

    auto FaceMask::layer() const -> i32 {
        return (value & ~BACK_FACE_BIT) - 1;
    }

    auto FaceMask::back_face() const -> bool {
        return (u32(value) & BACK_FACE_BIT) != 0;
    }

    auto FaceMask::operator==(FaceMask const& other) const -> bool {
        return value == other.value;
    }

    ChunkRender::~ChunkRender() noexcept { clear(); }

    auto ChunkRender::clear() -> void {
        for (auto& mesh_instance : mesh_instances) {
            if (mesh_instance) mesh_instance->queue_free();
        }
        if (collision_body) collision_body->queue_free();
        if (collision_shape) collision_shape->queue_free();
        if (multi_mesh_instance) multi_mesh_instance->queue_free();
        if (dynamic_body) dynamic_body->queue_free();
    }

    void Chunk::clear() {
        memset(block_ids.data(), 0, sizeof block_ids);
        memset(blocks.data(), 0, sizeof blocks);
        block_ids_size = 0;

        tag_ids.clear();
        meta_ids.clear();
        id2block.clear();
        extended_block_id.clear();
    }

    auto Chunk::column_seed(i32 seed, i32 x, i32 z) -> u32 {
        auto h = u32(seed);
        h ^= u32(x) + 0x9e3779b9u + (h << 6) + (h >> 2);
        h ^= u32(z) + 0x85ebca6bu + (h << 6) + (h >> 2);
        h ^= h >> 16;
        h *= 0x7feb352du;
        h ^= h >> 15;
        h *= 0x846ca68bu;
        h ^= h >> 16;
        return h;
    }

    auto Chunk::smoothstep(f32 value) -> f32 {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }

    auto Chunk::lerp_biome(Biome const& a, Biome const& b, f32 t) -> Biome {
        return {
            a.base_noise + (b.base_noise - a.base_noise) * t,
            a.base_height + (b.base_height - a.base_height) * t,
            a.detail_noise + (b.detail_noise - a.detail_noise) * t,
            a.detail_height + (b.detail_height - a.detail_height) * t,
            a.temperature + (b.temperature - a.temperature) * t,
            i32(std::round(f32(a.min_height) + f32(b.min_height - a.min_height) * t))
        };
    }

    auto Chunk::select_biome_at(i32 wx, i32 wz, Ref<FastNoiseLite> noise, usize biome_count) -> Biome {
        if (biome_count == 0) return { 0.01f, 40.0f, 0.4f, 4.0f, 60.0f, 0 };

        auto const biome_noise_val = noise->get_noise_2d(
            fsize(wx + 10000) * 0.005f,
            fsize(wz + 10000) * 0.005f
        );

        auto const normalized = (biome_noise_val + 1.0f) * 0.5f;
        auto const biome_idx = std::clamp(usize(normalized * biome_count), usize(0), biome_count - 1);
        
        return biome_registry::get_biome(biome_idx);
    }

    auto Chunk::get_blended_biome(i32 wx, i32 wz, Ref<FastNoiseLite> noise, usize biome_count) -> Biome {
        if (biome_count <= 1) return select_biome_at(wx, wz, noise, biome_count);

        static constexpr auto BLEND_CELL_SIZE = 96;
        auto const cell_xf = f32(wx) / f32(BLEND_CELL_SIZE);
        auto const cell_zf = f32(wz) / f32(BLEND_CELL_SIZE);
        auto const cell_x = i32(std::floor(cell_xf));
        auto const cell_z = i32(std::floor(cell_zf));
        auto const tx = smoothstep(cell_xf - f32(cell_x));
        auto const tz = smoothstep(cell_zf - f32(cell_z));

        auto const x0 = cell_x * BLEND_CELL_SIZE;
        auto const z0 = cell_z * BLEND_CELL_SIZE;
        auto const x1 = x0 + BLEND_CELL_SIZE;
        auto const z1 = z0 + BLEND_CELL_SIZE;

        auto const b00 = select_biome_at(x0, z0, noise, biome_count);
        auto const b10 = select_biome_at(x1, z0, noise, biome_count);
        auto const b01 = select_biome_at(x0, z1, noise, biome_count);
        auto const b11 = select_biome_at(x1, z1, noise, biome_count);

        auto const bx0 = lerp_biome(b00, b10, tx);
        auto const bx1 = lerp_biome(b01, b11, tx);

        return lerp_biome(bx0, bx1, tz);
    }

    auto Chunk::set_block(Pos3D<u8> const& pos, std::string_view block) -> void {
        set_block(pos, block_registry::get_id(block));
    }
    auto Chunk::set_block(Pos3D<u8> const& pos, u32 block_id) -> void {
        auto const lock = std::unique_lock(data_mutex);

        if (block_ids_size >= 255) {
            extended_block_id[pos] = block_id;
            return;
        }

        if (id2block.contains(block_id)) {
            blocks[pos.x][pos.y][pos.z] = id2block[block_id];
            return;
        }

        blocks[pos.x][pos.y][pos.z] = block_ids_size;
        id2block[block_id] = block_ids_size;
        block_ids[block_ids_size++] = block_id;

        auto const& default_metadatas = block_registry::get_block(block_id).init_metadatas();
        for (auto const& [meta, data] : default_metadatas) set_block_metadata(pos, meta, data);

        auto const& default_tags = block_registry::get_block(block_id).init_tags();
        for (auto tag : default_tags) tag_block(pos, tag);
    }

    auto Chunk::tag_block(Pos3D<u8> const& pos, std::string_view tag) -> void {
		return tag_block(pos, tag_registry::get_id(tag));
    }
    auto Chunk::tag_block(Pos3D<u8> const& pos, u32 tag_id) -> void {
        auto const lock = std::unique_lock(data_mutex);
        tag_ids[pos].emplace(tag_id);
    }

    auto Chunk::set_block_metadata(Pos3D<u8> const& pos, std::string_view meta, std::string_view meta_data) -> void {
        return set_block_metadata(pos, meta_registry::get_id(meta), meta_data);
    }
    auto Chunk::set_block_metadata(Pos3D<u8> const& pos, u32 meta_id, std::string_view meta_data) -> void {
        auto const lock = std::unique_lock(data_mutex);
        meta_ids[pos][meta_id] = meta_data;
    }

    auto Chunk::has_tag(Pos3D<u8> const& pos, std::string_view tag) const -> bool {
		return has_tag(pos, tag_registry::get_id(tag));
    }
    auto Chunk::has_tag(Pos3D<u8> const& pos, u32 tag_id) const -> bool {
        auto const lock = std::shared_lock(data_mutex);
        auto const valid = pos.x >= WIDTH or pos.y >= HEIGHT or pos.z >= WIDTH;

        return valid and tag_ids.contains(pos) and tag_ids.at(pos).contains(tag_id);
    }

    bool Chunk::has_metadata(Pos3D<u8> const& pos, std::string_view meta, std::string_view meta_data) const {
        return has_metadata(pos, meta_registry::get_id(meta), meta_data);
    }
    bool Chunk::has_metadata(Pos3D<u8> const& pos, u32 tag_id, std::string_view tag_data) const {
        auto const lock = std::shared_lock(data_mutex);
        if (pos.x >= WIDTH or pos.y >= HEIGHT or pos.z >= WIDTH) return false;

        return meta_ids.contains(pos) and meta_ids.at(pos).contains(tag_id);
    }

    auto Chunk::get_block(Pos3D<u8> const& pos) const -> u32 {
        if (pos.x >= WIDTH or pos.y >= HEIGHT or pos.z >= WIDTH) return 0;
        if (extended_block_id.contains(pos)) return extended_block_id.at(pos);

        return block_ids[blocks[pos.x][pos.y][pos.z]];
    }

    auto Chunk::get_tag(Pos3D<u8> const& pos) const -> std::unordered_set<u32> const* {
        if (pos.x >= WIDTH or pos.y >= HEIGHT or pos.z >= WIDTH) return nullptr;
        return tag_ids.contains(pos) ? &tag_ids.at(pos) : nullptr;
    }

    auto Chunk::get_metadata(Pos3D<u8> const& pos) const -> std::unordered_map<u32, std::string> const* {
        if (pos.x >= WIDTH or pos.y >= HEIGHT or pos.z >= WIDTH) return nullptr;
        return meta_ids.contains(pos) ? &meta_ids.at(pos) : nullptr;
    }

    auto Chunk::generate_terrain(i32 seed, Ref<FastNoiseLite> noise) -> void {
        auto const AIR     = block_registry::get_id("Air");
        auto const DIRT    = block_registry::get_id("Dirt");
        auto const WATER   = block_registry::get_id("Air");
        auto const GRASS   = block_registry::get_id("Grass Block");
        auto const STONE   = block_registry::get_id("Stone");
        auto const BEDROCK = block_registry::get_id("Bedrock");
        auto const CHEESE_CAVE = cave_registry::get_cave(cave_registry::get_id("Large Cavern"));

        auto new_block_ids = std::array<u8, 256>{};
        auto new_block_ids_size = 0ub;

        auto new_id2block = std::unordered_map<u32, u8>();
        auto new_extended_block_id = std::unordered_map<Pos3D<u8>, u32>();
        auto new_blocks = multi_array<u8, WIDTH, HEIGHT, WIDTH>{};

        auto new_meta_ids = std::unordered_map<Pos3D<u8>, std::unordered_map<u32, std::string>>();
        auto new_tag_ids = std::unordered_map<Pos3D<u8>, std::unordered_set<u32>>();

        auto iadd_block_metadata = [&new_meta_ids](Pos3D<u8> const& pos, u32 meta_id, std::string_view meta_data = "") {
            new_meta_ids[pos][meta_id] = meta_data;
        };

        auto itag_block = [&new_tag_ids](Pos3D<u8> const& pos, u32 tag_id) {
            new_tag_ids[pos].emplace(tag_id);
        };

        auto iadd_block = [&](Pos3D<u8> const& pos, u32 block_id) {
            if (new_block_ids_size >= 255) {
                new_extended_block_id[pos] = block_id;
                return;
            }

            if (new_id2block.contains(block_id)) {
                new_blocks[pos.x][pos.y][pos.z] = new_id2block[block_id];
                return;
            }

            new_blocks[pos.x][pos.y][pos.z] = new_block_ids_size;
            new_id2block[block_id] = new_block_ids_size;
            new_block_ids[new_block_ids_size++] = block_id;

            auto const& default_metadatas = block_registry::get_block(block_id).init_metadatas();
            for (auto const& [meta, data] : default_metadatas) iadd_block_metadata(pos, meta, data);

            auto const& default_tags = block_registry::get_block(block_id).init_tags();
            for (auto tag : default_tags) itag_block(pos, tag);
        };

        auto const biome_count = biome_registry::registry.size();
        for (auto x : std::views::iota(0ub, WIDTH)) {
            for (auto z : std::views::iota(0ub, WIDTH)) {
                auto const global_x = chunk_pos.x * WIDTH + x;
                auto const global_z = chunk_pos.y * WIDTH + z;

                auto const current_biome = get_blended_biome(global_x, global_z, noise, biome_count);

                auto const base_noise = noise->get_noise_2d(fsize(global_x) * current_biome.base_noise, fsize(global_z) * current_biome.base_noise);
                auto const base_elevation = ((base_noise + 1.0f) * 0.5f) * current_biome.base_height;
                
                auto detail_elevation = 0.0f;
                if (current_biome.detail_noise > 0.0f and current_biome.detail_height > 0.0f) {
                    auto const detail_noise = noise->get_noise_2d(fsize(global_x) * current_biome.detail_noise, fsize(global_z) * current_biome.detail_noise);
                    detail_elevation = detail_noise * current_biome.detail_height;
                }

                auto const terrain_base_y = current_biome.min_height + base_elevation + detail_elevation;
                auto solid_depth = -1;

                for (auto y : std::views::iota(0ub, HEIGHT) | std::views::reverse) {
                    if (y == 0) {
                        iadd_block({ x, y, z }, BEDROCK);
                        continue;
                    }

                    auto const cave_noise = noise->get_noise_3d(
                        global_x * CHEESE_CAVE.frequency,
                        y * CHEESE_CAVE.frequency,
                        global_z * CHEESE_CAVE.frequency
                    );

                    auto const noise_3d = noise->get_noise_3d(
                        fsize(global_x) * 0.2f,
                        fsize(y) * 0.3f,
                        fsize(global_z) * 0.2f
                    );

                    auto const density = terrain_base_y - f32(y) + (noise_3d * 25.0f);
                    auto block_id = AIR;

                    if (cave_noise <= CHEESE_CAVE.threshold) {
                        if (density > current_biome.base_height * 0.005f) {
                            if (solid_depth == -1) {
                                block_id = GRASS;
                                solid_depth = 1;
                            }
                            else if (solid_depth < 4) {
                                block_id = DIRT;
                                solid_depth++;
                            }
                            else block_id = STONE;
                        }
                        else {
                            if (y <= 62) block_id = WATER;
                            solid_depth = -1;
                        }
                    }

                    iadd_block({ x, y, z }, block_id);
                }
            }
        }

        {
            auto lock = std::unique_lock(data_mutex);
            std::memcpy(blocks.data(), new_blocks.data(), sizeof new_blocks);
            std::memcpy(block_ids.data(), new_block_ids.data(), sizeof new_block_ids);

            block_ids_size = new_block_ids_size;
            id2block.swap(new_id2block);
            extended_block_id.swap(new_extended_block_id);

            meta_ids.swap(new_meta_ids);
            tag_ids.swap(new_tag_ids);
        }

        dirty.store(true, std::memory_order::release);
        generated.store(true, std::memory_order::release);
        ++chunk_version;
    }

    static auto get_submesh_index(u8 y) -> u8 { return std::clamp(y / (Chunk::HEIGHT / 4), 0, 3); }

    auto Chunk::generate_mesh(ChunkRender& mesh, std::array<std::shared_ptr<Chunk>, 4> neighbors) -> void {
        auto chunk_data_ptr = std::make_unique<MeshesData>();
        auto& chunk_data = *chunk_data_ptr;

        for (auto i : std::views::iota(0, 4)) {
            auto& data = chunk_data[i];

            data.vertices.reserve(1024);
            data.normals.reserve(1024);
            data.indices.reserve(1536);
            data.uvs.reserve(1024);
            data.uvs_layer.reserve(1024);
            data.collision_faces.reserve(1536);
        }

        auto const AIR = block_registry::get_id("Air");
        auto const TRANSPARENT = tag_registry::get_id("transparent");

        auto mutexes_to_lock = std::vector<std::shared_mutex*>{};
        mutexes_to_lock.reserve(5);
        mutexes_to_lock.push_back(&data_mutex);
        for (auto i : std::views::iota(0, 4)) if (neighbors[i]) mutexes_to_lock.push_back(&neighbors[i]->data_mutex);

        std::ranges::sort(mutexes_to_lock);

        auto const result = std::ranges::unique(mutexes_to_lock);
        mutexes_to_lock.resize(result.begin() - mutexes_to_lock.begin());

        auto locks = std::vector<std::shared_lock<std::shared_mutex>>{};
        for (auto* m : mutexes_to_lock) locks.emplace_back(std::shared_lock(*m));

        auto is_complex_block = [this, AIR](u32 id) -> bool {
            return block_registry::get_block(id).get_texture_layer(Face::TOP) == -1;
        };

        auto transparent = [this, &neighbors, AIR, TRANSPARENT, &is_complex_block](i32 bx, u8 by, i32 bz) -> bool {
            if (bx < Chunk::WIDTH and bx >= 0 and bz < Chunk::WIDTH and bz >= 0) {
                auto id = get_block({ u8(bx), by, u8(bz) });
                if (id == AIR or is_complex_block(id)) return true;

                auto tag_ptr = get_tag({ u8(bx), by, u8(bz) });
                return tag_ptr and tag_ptr->contains(TRANSPARENT);
            }

            auto const nid = bx >= Chunk::WIDTH ? 0 :
                             bx < 0             ? 1 :
                             bz >= Chunk::WIDTH ? 2 :
                             bz < 0             ? 3 : 0;

            auto const& neighbor_ptr = neighbors[nid];
			if (not neighbor_ptr) return true;

			auto& neighbor = *neighbor_ptr;
            if (not neighbor.generated.load(std::memory_order::acquire)) return true;

            auto lx = u8((bx % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH);
            auto lz = u8((bz % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH);

            auto const id = neighbor.get_block({ lx, by, lz });
            if (id == AIR or is_complex_block(id)) return true;

			auto const tag_ptr = neighbor.get_tag({ lx, by, lz });
            return tag_ptr and tag_ptr->contains(TRANSPARENT);
        };

        auto get_block_layer = [this, AIR, &is_complex_block](u8 bx, u8 by, u8 bz, Face face) -> i32 {
            auto const id = get_block({ bx, by, bz });
            if (id == AIR or is_complex_block(id)) return -1;

            return block_registry::get_block(id).get_texture_layer(face);
        };

        static constexpr auto dims = std::array{Chunk::WIDTH, Chunk::HEIGHT, Chunk::WIDTH};
        static constexpr auto front_faces = std::array{Face::RIGHT, Face::TOP,    Face::FRONT};
        static constexpr auto back_faces =  std::array{Face::LEFT,  Face::BOTTOM, Face::BACK };

        auto mask = std::vector<FaceMask>{};
        mask.resize(Chunk::HEIGHT * Chunk::WIDTH);

        auto vertex_offsets = std::array<u8, 4>{};
        for (auto d : std::views::iota(0, 3)) {
            auto const u = (d + 1) % 3;
            auto const v = (d + 2) % 3;

            auto x = std::array<i32, 3>{};
            auto q = std::array<i32, 3>{};
            q[d] = 1;
            x[d] = -1;

            while (x[d] < dims[d]) {
                auto const _ = RAII([&x, d] { ++x[d]; });

                std::ranges::fill(mask, FaceMask(-1, false));

                x[v] = 0;
                while (x[v] < dims[v]) {
                    auto const _ = RAII([&x, v] { ++x[v]; });

                    x[u] = 0;
                    while (x[u] < dims[u]) {
                        auto const _ = RAII([&x, u] { ++x[u]; });

                        auto const a_trans = transparent(x[0], x[1], x[2]);
                        auto const b_trans = transparent(x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                        if (x[d] >= 0 and not a_trans and b_trans) {
                            auto const layer = get_block_layer(x[0], x[1], x[2], front_faces[d]);
                            if (layer >= 0) mask[x[u] + x[v] * dims[u]] = FaceMask(layer, false);
                        }
                        else if (x[d] + 1 < dims[d] and a_trans and not b_trans) {
                            auto const layer = get_block_layer(x[0] + q[0], x[1] + q[1], x[2] + q[2], back_faces[d]);
                            if (layer >= 0) mask[x[u] + x[v] * dims[u]] = FaceMask(layer, true);
                        }
                    }
                }

                for (auto j : std::views::iota(0ub, dims[v])) {
                    auto i = 0uqw;

                    while (i < dims[u]) {
                        auto const current_face = mask[i + j * dims[u]];
                        if (current_face.layer() < 0) {
                            ++i;
                            continue;
                        }

                        auto width = 1;
                        while (i + width < dims[u] and mask[i + width + j * dims[u]] == current_face) ++width;

                        auto height = 1;
                        while (j + height < dims[v]) {
                            for (auto k : std::views::iota(0, width)) {
                                if (mask[(i + k) + (j + height) * dims[u]] != current_face) goto CANT_GROW_MORE;
                            }
                            ++height;
                        }

                    CANT_GROW_MORE:

                        auto du = std::array<f32, 3>{}; du[u] = f32(width);
                        auto dv = std::array<f32, 3>{}; dv[v] = f32(height);

                        auto start = std::array<f32, 3>{};
                        start[d] = f32(x[d] + 1);
                        start[u] = f32(i);
                        start[v] = f32(j);

                        auto const avg_y = u8(start[1]);
                        auto const s_idx = get_submesh_index(avg_y);

                        auto& data = chunk_data[s_idx];
                        auto& vertex_offset = vertex_offsets[s_idx];

                        auto const p0 = Pos3D(start[0], start[1], start[2]);
                        auto const p1 = Pos3D(start[0] + du[0], start[1] + du[1], start[2] + du[2]);
                        auto const p2 = Pos3D(start[0] + du[0] + dv[0], start[1] + du[1] + dv[1], start[2] + du[2] + dv[2]);
                        auto const p3 = Pos3D(start[0] + dv[0], start[1] + dv[1], start[2] + dv[2]);

                        auto get_uv = [&p0, current_face, width, height, d](Pos3D<f32> const& p) -> Pos2D<fsize> {
                            auto const dx = p.x - p0.x;
                            auto const dy = p.y - p0.y;
                            auto const dz = p.z - p0.z;

                            auto const result =
                                d == 0 ? Pos2D(current_face.back_face() ? height - dz : dz, width - dy) :
                                d == 1 ? Pos2D(dx, current_face.back_face() ? height - dz : dz) :
                                         Pos2D(current_face.back_face() ? width - dx : dx, height - dy);

                            return result;
                        };

                        auto& vertices        = data.vertices;
                        auto& normals         = data.normals;
                        auto& indices         = data.indices;
                        auto& uvs             = data.uvs;
                        auto& uvs_layer       = data.uvs_layer;
                        auto& collision_faces = data.collision_faces;

                        if (not current_face.back_face()) {
                            vertices.push_back(p0); vertices.push_back(p1);
                            vertices.push_back(p2); vertices.push_back(p3);

                            uvs.push_back(get_uv(p0));
                            uvs.push_back(get_uv(p1));
                            uvs.push_back(get_uv(p2));
                            uvs.push_back(get_uv(p3));
                        }
                        else {
                            vertices.push_back(p0); vertices.push_back(p3);
                            vertices.push_back(p2); vertices.push_back(p1);

                            uvs.push_back(get_uv(p0));
                            uvs.push_back(get_uv(p3));
                            uvs.push_back(get_uv(p2));
                            uvs.push_back(get_uv(p1));
                        }

                        auto normal = Pos3D(0.0fz, 0.0fz, 0.0fz);

                        (d == 0 ? normal.x : d == 1 ? normal.y : normal.z) = current_face.back_face() ? -1.0fz : 1.0fz;

                        for (auto n : std::views::iota(0, 4)) normals.push_back(normal);

                        auto layer_uv = Pos2D(f32(current_face.layer()), 0.0f);
                        for (auto n : std::views::iota(0, 4)) uvs_layer.push_back(layer_uv);

                        indices.push_back(i32(vertex_offset)); indices.push_back(i32(vertex_offset + 2)); indices.push_back(i32(vertex_offset + 1));
                        indices.push_back(i32(vertex_offset)); indices.push_back(i32(vertex_offset + 3)); indices.push_back(i32(vertex_offset + 2));

                        collision_faces.push_back(vertices[vertex_offset    ]);
                        collision_faces.push_back(vertices[vertex_offset + 2]);
                        collision_faces.push_back(vertices[vertex_offset + 1]);
                        collision_faces.push_back(vertices[vertex_offset + 0]);
                        collision_faces.push_back(vertices[vertex_offset + 3]);
                        collision_faces.push_back(vertices[vertex_offset + 2]);

                        vertex_offset += 4;

                        for (auto v_idx : std::views::iota(0, height))
                            for (auto u_idx : std::views::iota(0, width))
                                mask[i + u_idx + (j + v_idx) * dims[u]] = FaceMask(-1, false);

                        i += width;
                    }
                }
            }
        }

        for (auto x : std::views::iota(0ub, Chunk::WIDTH)) {
            for (auto y : std::views::iota(0ub, Chunk::HEIGHT)) {
                for (auto z : std::views::iota(0ub, Chunk::WIDTH)) {
                    auto const id = get_block({ x, y, z });

                    if (not is_complex_block(id)) continue;
                    chunk_data[get_submesh_index(y)].complex_instance.push_back(ComplexBlockInstance(id, {x, y, z}));
                }
            }
        }

        {
            auto lock = std::unique_lock(mesh.mesh_mutex);
            mesh.pending_meshes_data.swap(chunk_data_ptr);
        }

        dirty.store(false, std::memory_order::release);
    }
}