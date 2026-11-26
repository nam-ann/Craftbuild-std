module game.command;

import game.world;
import game.world.chunk;

using namespace std::literals;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    static inline auto trim(std::string_view str) {
        auto const first = str.find_first_not_of(" \t\n\r");
        if (first == std::string::npos) return ""sv;

        auto const last = str.find_last_not_of(" \t\n\r");
        return str.substr(first, last - first + 1);
    }

    static inline auto tokenize_with_quotes(std::string_view input) -> std::vector<std::string> {
        auto tokens = std::vector<std::string>{};
        auto current_token = ""s;
        bool in_quotes = false;

        for (auto i : std::views::iota(0uz, input.size())) {
            auto const c = input[i];

            switch (c) {
            case '"':
                in_quotes = not in_quotes;

                if (not current_token.empty()) {
                    tokens.push_back(current_token);
                    current_token.clear();
                }
                break;

            case ' ':
                if (not in_quotes and not current_token.empty()) {
                    tokens.push_back(current_token);
                    current_token.clear();
                }
                break;

            default: current_token += c;
            }
        }

        if (not current_token.empty()) tokens.push_back(current_token);

        return tokens;
    }

	auto CommandInterpreter::is_valid_coordinate(i64 x, i64 y, i64 z) -> bool {
        return x >= -World::RANGE and x <= World::RANGE and y >= 0 and y <= Chunk::HEIGHT and z >= -World::RANGE and z <= World::RANGE;
    }

    auto CommandInterpreter::execute_command(std::string_view command_line) -> std::string {
        auto const cmd = trim(command_line);
        if (cmd.empty()) return "";

        auto const parts = tokenize_with_quotes(cmd);
        if (parts.empty()) return "";

        if (parts[0] == "set_block") return execute_set_block(parts);
        else if (parts[0] == "fill") return execute_fill(parts);
        else if (parts[0] == "give") return execute_give(parts);
        else {
            auto const output ="Invalid command: {}"f(parts[0]);
            log<LogType::ERROR>(output);
            return output;
        }
    }

    auto CommandInterpreter::execute_set_block(std::vector<std::string> const& args) -> std::string {
        auto world = static_cast<World*>(world_ptr);
        if (not world) return "";
        auto output = ""s;

        if (args.size() < 5) {
            output = "Must fill atleast (x y z block_type)";
            log<LogType::ERROR>(output);
            return output;
        }

        try {
            auto const x = std::stoi(args[1]);
            auto const y = std::stoi(args[2]);
            auto const z = std::stoi(args[3]);
            auto const& block_type = args[4];

            if (not is_valid_coordinate(x, y, z)) {
                output = "{} outs of bounds"f(Pos3D(x, y, z));
                log<LogType::ERROR>(output);
                return output;
            }

            if (not block_registry::has_block(block_type)) {
                output = "Invalid block: '{}'"f(block_type);
                log<LogType::ERROR>(output);
                return output;
            }

            world->set_global_block_id(block_registry::get_id(block_type), x, y, z);
            output = "std::unordered_set block {} at {}"f(block_type, Pos3D(x, y, z));
            log<LogType::INFO>(output);
        }
        catch (std::exception const&) {
            output = "Invalid command arguments";
            log<LogType::ERROR>(output);
        }

        return output;
    }

    auto CommandInterpreter::execute_fill(std::vector<std::string> const& args) -> std::string {
        auto world = static_cast<World*>(world_ptr);
        if (not world) return "";
        auto output = ""s;

        if (args.size() < 8) {
            output = "Must fill atleast (x1 y1 z1 x2 y2 z2 block_type)";
            log<LogType::ERROR>(output);
            return output;
        }

        try {
            auto const x1 = std::stoi(args[1]);
            auto const y1 = std::stoi(args[2]);
            auto const z1 = std::stoi(args[3]);
            auto const x2 = std::stoi(args[4]);
            auto const y2 = std::stoi(args[5]);
            auto const z2 = std::stoi(args[6]);
            auto const& block_type = args[7];

            if (not block_registry::has_block(block_type)) {
                output = "Invalid block: '{}'"f(block_type);
                log<LogType::ERROR>(output);
                return output;
            }

            auto const min_x = std::min(x1, x2);
            auto const max_x = std::max(x1, x2);
            auto const min_y = std::min(y1, y2);
            auto const max_y = std::max(y1, y2);
            auto const min_z = std::min(z1, z2);
            auto const max_z = std::max(z1, z2);

            if (not is_valid_coordinate(min_x, min_y, min_z) or not is_valid_coordinate(max_x, max_y, max_z)) {
                output = "{} or {}outs of bounds"f(Pos3D(min_x, min_y, min_z), Pos3D(max_x, max_y, max_z));
                log<LogType::ERROR>(output);
                return output;
            }

            auto block_count = 0;
            for (auto x : std::views::iota(min_x, max_x)) {
                for (auto y : std::views::iota(min_y, max_y)) {
                    for (auto z : std::views::iota(min_z, max_z)) {
                        world->set_global_block_id(block_registry::get_id(block_type), x, y, z);
                        block_count++;
                    }
                }
            }

            output = "Filled {} blocks {} from {} to {}"f(block_count, block_type, Pos3D(min_x, min_y, min_z), Pos3D(max_x, max_y, max_z));
            log<LogType::INFO>(output);
        }
        catch (std::exception const&) {
            output = "Invalid command arguments";
            log<LogType::ERROR>(output);
        }

        return output;
    }

    auto CommandInterpreter::execute_give(std::vector<std::string> const& args) -> std::string {
        auto world = static_cast<World*>(world_ptr);
        if (not world) return "";
        auto output = ""s;

        if (args.size() < 3) {
            output = "Must fill atleast (player_name item_name)";
            log<LogType::ERROR>(output);
            return output;
        }

        auto amount = 1ll;
        if (args.size() >= 4) {
            try {
                amount = std::stoi(args[3]);
                if (amount <= 0) {
                    output = "Amount cannot be negative";
                    log<LogType::ERROR>(output);
                    return output;
                }
                if (amount > 64) {
                    output = "Maximum for a stack is 64";
                    log<LogType::WARNING>(output);
                }
            }
            catch (std::exception const&) {
                output = "Invalid command arguments";
                log<LogType::ERROR>(output);
                return output;
            }
        }

        if (not block_registry::has_block(args[2])) {
            output = "Invalid block: '{}'"f(args[2]);
            log<LogType::ERROR>(output);
            return output;
        }

        auto& player = world->players[args[1]];
        player.hotbar[player.selected_slot] = block_registry::get_id(args[2]);

        output = "Gave {} {} {} (s)"f(args[1], amount, args[2]);
        log<LogType::INFO>(output);
        return output;
    }
}