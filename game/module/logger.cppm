export module game.logger;

import std;

import misc.string;
import misc.number;
import game.core;
import game.thread;

using namespace godot;

namespace std { namespace views = ranges::views; }

export namespace craftbuild {
    struct LogQueue final {
        inline static std::string file_queue;
        inline static std::mutex log_mutex;

        static auto store(std::string_view log, std::string_view file_log) -> void;
        static auto flush() -> void;
    };

    enum class LogType : u8 {
        NORMAL,
        VERBOSE,
        INFO,
        WARNING,
        ERROR
    };

    template <LogType LOG_TYPE>
    auto log(std::string_view message, std::source_location __loc__ = std::source_location::current()) -> void;
}