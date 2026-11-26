export module game.player.player_data;

import misc.pos;
import misc.string;
import misc.number;
import game.core;

export namespace craftbuild {
    struct PlayerData final {
        std::string name;
        Pos3D<fsize> pos;

        inline static constexpr auto HOTBAR_SIZE = 9ub;
        std::array<u32, HOTBAR_SIZE> hotbar = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        u8 selected_slot = 0;
        std::unordered_map<std::string, i32> inventory;
        i8 hp = 20;
    };
}