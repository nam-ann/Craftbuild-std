module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/window.hpp>
#include <godot_cpp/classes/world3d.hpp>
#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/capsule_shape3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/input_event_key.hpp>
#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/input_event_mouse_motion.hpp>
#include <godot_cpp/classes/input_event_mouse_button.hpp>
#include <godot_cpp/classes/physics_direct_space_state3d.hpp>
#include <godot_cpp/classes/physics_ray_query_parameters3d.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/variant/vector3i.hpp>
#include <godot_cpp/variant/dictionary.hpp>
ENABLE_WARNING

module game.player;

import game.main;
import game.player.skin_manager;

namespace craftbuild {
    auto Player::_ready() -> void {
        // Camera
        camera = get_node<Camera3D>("Camera");
        camera->set_position(Vector3(0, 1.8f, 0));

        // Model
        auto model = get_node<MeshInstance3D>("Mesh");

        auto box = memnew(BoxMesh);
        box->set_size(Vector3(0.6f, 2.0f, 0.3f));

        model->set_mesh(box);
        model->set_position(Vector3(0, 1.0f, 0));

        // Collision
        auto collision = get_node<CollisionShape3D>("Shape");

        auto capsule = memnew(CapsuleShape3D);
        capsule->set_radius(0.3f);
        capsule->set_height(1.8f);

        collision->set_shape(capsule);
        collision->set_position(Vector3(0, 1.0f, 0));

        // Selection box
        selection_box = memnew(MeshInstance3D);
        selection_box->set_scale(Pos3D(1.001f, 1.001f, 1.001f));

        auto selection_mesh = memnew(BoxMesh);
        selection_mesh->set_size(Pos3D(1.001f, 1.001f, 1.001f));

        selection_box->set_mesh(selection_mesh);
        selection_box->set_material_override(create_selection_box_material());

        // Load skin
        skin_manager::load_skin(*this, "res://assets/textures/skin/creeper_boy.png");
        
		// World reference
        world_ptr = Object::cast_to<Main>(get_parent());

        log<LogType::INFO>("Player initialized");
    }

    auto Player::_process(f64 delta) -> void {
        auto world = static_cast<Main*>(world_ptr);
        if (not world) return;
        if (world->pausing.load(std::memory_order_relaxed)) return;

        hit = raycast_block();
        if (hit.is_empty()) {
            if (selection_box->get_parent() == world) world->remove_child(selection_box);
            return;
        }
        if (selection_box->get_parent() != world) world->add_child(selection_box);

        auto const hit_pos = Vector3(hit["position"]);
        auto const normal = Vector3(hit["normal"]);

        auto const pos_float = hit_pos - (normal * 0.001f);
        auto const block_pos = Pos3D<fsize>(pos_float.floor());

        selection_box->set_position(Vector3(block_pos.x, block_pos.y, block_pos.z) + Vector3(0.5, 0.5, 0.5));
    }

    auto Player::_physics_process(f64 delta) -> void {
        if (gamemode == Gamemode::SPECTATOR) return;
        if (not camera or not world_ptr) return;

		auto world = static_cast<Main*>(world_ptr);
        if (world->pausing.load(std::memory_order::relaxed)) return;

        auto velocity = get_velocity();
        auto input = Input::get_singleton();

        auto const chatting = world->chatting.load(std::memory_order::relaxed);

        auto const key_space = not chatting and input->is_key_pressed(KEY_SPACE);
        auto const key_shift = not chatting and input->is_key_pressed(KEY_SHIFT);
        auto const key_ctrl = not chatting and input->is_key_pressed(KEY_CTRL);
        auto const key_w = not chatting and input->is_key_pressed(KEY_W);
        auto const key_d = not chatting and input->is_key_pressed(KEY_D);
        auto const key_s = not chatting and input->is_key_pressed(KEY_S);
        auto const key_a = not chatting and input->is_key_pressed(KEY_A);

        is_grounded = is_on_floor();

        running = key_w and (key_ctrl or running);
        auto const current_speed = running ? speed * 2.0f : speed;

        auto forward = -camera->get_global_transform().basis.get_column(2);
        auto right = camera->get_global_transform().basis.get_column(0);

        auto forward_input = (key_w ? 1.0f : 0.0f) - (key_s ? 1.0f : 0.0f);
        auto strafe_input = (key_d ? 1.0f : 0.0f) - (key_a ? 1.0f : 0.0f);

        auto wish_dir = (forward * forward_input) + (right * strafe_input);
        wish_dir.y = 0;
        if (wish_dir.length() > 0) wish_dir = wish_dir.normalized();

        auto const accel = is_grounded ? 18.0f : 6.0f;
        auto const decel = is_grounded ? 22.0f : 2.0f;

        // Gravity & Jump
        if (not can_fly) {
            auto const has_input = wish_dir.length_squared() > 0.0f;
            auto const blend = has_input ? accel : decel;

            auto const target_xz = wish_dir * current_speed;
            velocity.x = velocity.x + (target_xz.x - velocity.x) * fsize(std::min(blend * delta, 1.0));
            velocity.z = velocity.z + (target_xz.z - velocity.z) * fsize(std::min(blend * delta, 1.0));

            if (is_grounded) {
                if (velocity.y < 0.0f) velocity.y = -0.1f;
                if (key_space) velocity.y = jump_velocity;
                if (gamemode == Gamemode::CREATIVE and key_space and not jump_was_pressed) {
                    can_fly = true;
                    velocity.y = 0.0f;
                }
            }
            else if (gamemode == Gamemode::CREATIVE and key_space and not jump_was_pressed) {
                can_fly = true;
                velocity.y = 0.0f;
            }
            else velocity.y -= fsize(gravity * delta);
        }
        else {
            auto const has_h_input = wish_dir.length_squared() > 0.0f;
            auto const h_blend = has_h_input ? accel : decel;
            auto const target_xz = wish_dir * current_speed;
            velocity.x = velocity.x + (target_xz.x - velocity.x) * fsize(std::min(h_blend * delta, 1.0));
            velocity.z = velocity.z + (target_xz.z - velocity.z) * fsize(std::min(h_blend * delta, 1.0));

            auto wish_y = 0.0f;
            if (key_space) wish_y = speed;
            if (key_shift) wish_y = -speed;

            auto const has_v_input = (wish_y != 0.0f);
            auto const v_blend = has_v_input ? accel : decel;
            velocity.y = velocity.y + (wish_y - velocity.y) * fsize(std::min(v_blend * delta, 1.0));

            if (is_grounded and not key_space) can_fly = false;
        }

        jump_was_pressed = key_space;

        set_velocity(velocity);
        move_and_slide();
    }

    auto Player::_input(Ref<InputEvent> const& event) -> void {
        if (not camera or not world_ptr) return;

        auto world = static_cast<Main*>(world_ptr);
        if (world->pausing.load(std::memory_order::relaxed) or world->chatting.load(std::memory_order::relaxed)) return;

        static auto gamemode_toggled = false;

        // Mouse look
        if (auto mm = Object::cast_to<InputEventMouseMotion>(event.ptr())) {
            auto rel = mm->get_relative();
            rotate_y(-rel.x * sensitivity);
            mouse_pitch = std::clamp(mouse_pitch - rel.y * sensitivity, -MATH_PI / MAXIMUM_CAMERA_ANGLE, MATH_PI / MAXIMUM_CAMERA_ANGLE);
            camera->set_rotation(Vector3(mouse_pitch, 0, 0));
        }

        // Mouse click/roll
        if (auto mb = Object::cast_to<InputEventMouseButton>(event.ptr())) {
            if (mb->is_pressed()) {
                // Mouse roll
                if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_UP)   cycle_hotbar(-1);
                if (mb->get_button_index() == MOUSE_BUTTON_WHEEL_DOWN) cycle_hotbar(1);
                
                // Mouse click
                auto mid = mb->get_button_index() == MOUSE_BUTTON_MIDDLE;
                auto left = mb->get_button_index() == MOUSE_BUTTON_LEFT;
                auto right = mb->get_button_index() == MOUSE_BUTTON_RIGHT;

                if (not hit.is_empty()) {
                    auto const hit_pos = Vector3(hit["position"]);
                    auto const normal = Vector3(hit["normal"]);

                    auto const pos_float = hit_pos - (normal * 0.001f);
                    auto block_pos = Vector3i(pos_float.floor());

                    auto target_block_id = world->get_global_block_id(block_pos.x, block_pos.y, block_pos.z);
                    log<LogType::VERBOSE>("Looking at block id: {} at {}"f(target_block_id, block_pos));

                    if (mid and gamemode == Gamemode::CREATIVE) hotbar[selected_slot] = target_block_id;

                    if (left or right) {
                        auto AIR = block_registry::get_id("Air");
                        auto block = get_selected_block_id();

                        if (left) world->set_global_block_id(AIR, block_pos.x, block_pos.y, block_pos.z);
                        if (right and block != AIR) {
                            auto face = get_face(normal);
                            switch (face) {
                            case Face::TOP:    block_pos.y += 1;  break;
                            case Face::BOTTOM: block_pos.y += -1; break;
                            case Face::LEFT:   block_pos.x += 1;  break;
                            case Face::RIGHT:  block_pos.x += -1; break;
                            case Face::FRONT:  block_pos.z += 1;  break;
                            case Face::BACK:   block_pos.z += -1; break;
                            }

                            if (not would_collide_with_player(block_pos)) world->set_global_block_id(block, block_pos.x, block_pos.y, block_pos.z);
                        }

                        auto const cx = i32(std::floor(f32(block_pos.x) / Chunk::WIDTH));
                        auto const cy = i32(std::floor(f32(block_pos.z) / Chunk::WIDTH));
                        if (auto chunk = world->get_chunk(cx, cy)) {
                            chunk->dirty.store(true, std::memory_order_release);

                            auto mark_neighbor = [&](Pos2D<i32> offset) {
                                if (auto neighbor = world->get_chunk(cx + offset.x, cy + offset.y)) {
                                    neighbor->dirty.store(true, std::memory_order_release);
                                }
                            };

                            switch (block_pos.x) {
                            case 0:                mark_neighbor({ -1, 0 }); break;
                            case Chunk::WIDTH - 1: mark_neighbor({ 1, 0 }); break;
                            }

                            switch (block_pos.z) {
                            case 0:                mark_neighbor({ 0, -1 }); break;
                            case Chunk::WIDTH - 1: mark_neighbor({ 0, 1 }); break;
                            }
                        }
                    }
                }
            }
        }

        if (auto input = Object::cast_to<InputEventKey>(event.ptr())) {
            auto is_f3_held = Input::get_singleton()->is_key_pressed(KEY_F3);

            if (input->is_pressed() and input->get_keycode() == KEY_F4 and is_f3_held) {
                if (not gamemode_toggled) {
                    gamemode = Gamemode((u8(gamemode) + 1) % 4);
                    log<LogType::INFO>("Changed gamemode to {}"f(i32(gamemode)));
                    can_fly = false;
                    gamemode_toggled = true;
                }
            }
            else if (not input->is_pressed() and input->get_keycode() == KEY_F4) gamemode_toggled = false;
            if (gamemode == Gamemode::SPECTATOR) can_fly = true;
        }

        for (auto i : std::views::iota(0, 9)) {
            auto key = (Key)(KEY_1 + i);
            if (Input::get_singleton()->is_key_pressed(key)) {
                if (event->is_pressed() and not event->is_echo()) select_slot(i);
            }
        }
    }

    auto Player::would_collide_with_player(Pos3D<i32> const& block_pos) const -> bool {
        auto player_pos = get_position();

        auto min_x = player_pos.x - 0.3f;
        auto max_x = player_pos.x + 0.3f;
        auto min_y = player_pos.y;
        auto max_y = player_pos.y + 1.8f;
        auto min_z = player_pos.z - 0.3f;
        auto max_z = player_pos.z + 0.3f;

        auto block_min_x = fsize(block_pos.x);
        auto block_max_x = fsize(block_pos.x + 1.0);
        auto block_min_y = fsize(block_pos.y);
        auto block_max_y = fsize(block_pos.y + 0.8);
        auto block_min_z = fsize(block_pos.z);
        auto block_max_z = fsize(block_pos.z + 1.0);

        return max_x > block_min_x and min_x < block_max_x and max_y > block_min_y and min_y < block_max_y and max_z > block_min_z and min_z < block_max_z;
    }

    auto Player::create_selection_box_material() -> Ref<ShaderMaterial> {
        auto mat = Ref<ShaderMaterial>();
        mat.instantiate();

        static constexpr auto shader_path = "res://assets/shaders/selection_box.gdshader"fs;
        static auto g_shader_path = to_gstring(shader_path);

        Ref<Shader> shader;
        shader.instantiate();

        if (FileAccess::file_exists(g_shader_path)) {
            auto file = FileAccess::open(g_shader_path, FileAccess::READ);
            if (file.is_valid()) {
                auto shader_code = file->get_as_text();
                shader->set_code(shader_code);
            }
            else log<LogType::ERROR>("Failed to open shader file at: {}"f(shader_path));
        }
        else log<LogType::ERROR>("Shader file not found at: {}"f(shader_path));

        mat->set_shader(shader);
        return mat;
    }

    auto Player::raycast_block(fsize max_distance) -> Dictionary {
        if (not camera) return Dictionary();

        auto screen_center = camera->get_viewport()->get_visible_rect().get_center();
        auto origin = camera->project_ray_origin(screen_center);
        auto direction = camera->project_ray_normal(screen_center);
        auto end = origin + direction * max_distance;

        auto space_state = camera->get_world_3d()->get_direct_space_state();
        auto query = PhysicsRayQueryParameters3D::create(origin, end);

        query->set_collision_mask(1); 

        return space_state->intersect_ray(query);
    }

    auto Player::get_face(Pos3D<fsize> const& n) -> Face {
        if (n == Pos3D<fsize>(0, 1, 0))  return Face::TOP;
        if (n == Pos3D<fsize>(0, -1, 0)) return Face::BOTTOM;
        if (n == Pos3D<fsize>(1, 0, 0))  return Face::LEFT;
        if (n == Pos3D<fsize>(-1, 0, 0)) return Face::RIGHT;
        if (n == Pos3D<fsize>(0, 0, 1))  return Face::FRONT;
        if (n == Pos3D<fsize>(0, 0, -1)) return Face::BACK;
        return Face::TOP;
    }

    auto Player::cycle_hotbar(i16 dir) -> void {
        selected_slot = (selected_slot + dir + HOTBAR_SIZE) % HOTBAR_SIZE;
        log<LogType::INFO>("Selected slot: {}"f(selected_slot + 1));
    }

    void Player::select_slot(u8 slot) {
        selected_slot = slot;
    }

    u32 Player::get_selected_block_id() const {
        return hotbar[selected_slot];
    }

    void Player::save_data(std::ostream& os) {
        os.write(reinterpret_cast<char const*>(&speed), sizeof(f32));
        os.write(reinterpret_cast<char const*>(&gravity), sizeof(f32));
        os.write(reinterpret_cast<char const*>(&jump_velocity), sizeof(f32));
        os.write(reinterpret_cast<char const*>(&is_grounded), sizeof(bool));
        os.write(reinterpret_cast<char const*>(&can_fly), sizeof(bool));
        os.write(reinterpret_cast<char const*>(&running), sizeof(bool));
        os.write(reinterpret_cast<char const*>(&gamemode), sizeof(Gamemode));
        os.write(reinterpret_cast<char const*>(&hotbar), sizeof(u32) * HOTBAR_SIZE);
        os.write(reinterpret_cast<char const*>(&selected_slot), sizeof(u8));
    }

    void Player::load_data(std::istream& is) {
        is.read(reinterpret_cast<char*>(&speed), sizeof(f32));
        is.read(reinterpret_cast<char*>(&gravity), sizeof(f32));
        is.read(reinterpret_cast<char*>(&jump_velocity), sizeof(f32));
        is.read(reinterpret_cast<char*>(&is_grounded), sizeof(bool));
        is.read(reinterpret_cast<char*>(&can_fly), sizeof(bool));
        is.read(reinterpret_cast<char*>(&running), sizeof(bool));
        is.read(reinterpret_cast<char*>(&gamemode), sizeof(Gamemode));
        is.read(reinterpret_cast<char*>(&hotbar), sizeof(u32) * HOTBAR_SIZE);
        is.read(reinterpret_cast<char*>(&selected_slot), sizeof(u8));
    }

    void Player::_bind_methods() {}
}
