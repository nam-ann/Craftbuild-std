export module game.command;

import std;

import misc.pos;
import misc.string;
import misc.number;
import game.block;
import game.logger;
import game.player;

export namespace craftbuild {
    class CommandInterpreter final {
        void* world_ptr = nullptr;

        auto is_valid_coordinate(i64 x, i64 y, i64 z) -> bool;

    public:
        CommandInterpreter(void* world) : world_ptr(world) {}

        auto execute_command(std::string_view command_line) -> std::string;
        auto execute_set_block(std::vector<std::string> const& args) -> std::string;
        auto execute_fill(std::vector<std::string> const& args) -> std::string;
        auto execute_give(std::vector<std::string> const& args) -> std::string;
    };
}