module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/classes/project_settings.hpp>
ENABLE_WARNING

#define LOC_PARAM std::source_location __loc__

module game.logger;

using namespace std::literals;

namespace craftbuild {
    inline static constexpr auto get_file_name(std::string_view path) {
        auto const last_slash = path.find_last_of(R"(\/)");
        auto const start_pos = last_slash == std::string_view::npos ? 0 : last_slash + 1;

        auto const filename_with_ext = path.substr(start_pos);
        auto const last_dot = filename_with_ext.find_last_of('.');

        if (last_dot != std::string_view::npos and last_dot != 0) return filename_with_ext.substr(0, last_dot);
        return filename_with_ext;
    }

    static inline auto get_tm_time() {
        auto now = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
        return std::chrono::floor<std::chrono::seconds>(now);
    }

    static inline auto get_time()  {
        return "[{:%H:%M:%S}] "f(get_tm_time());
    }

    static inline auto get_info(LOC_PARAM) {
        return "({} | {}):{} "f(thread_registry::get_name(std::this_thread::get_id()), get_file_name(__loc__.file_name()), __loc__.line());
    }

    auto LogQueue::store(std::string_view log, std::string_view file_log) -> void {
        if (craftbuild_debug) UtilityFunctions::print(to_gstring(log));

        auto const lock = std::lock_guard(log_mutex);
        (file_queue += file_log) += "\n";
    }

    auto LogQueue::flush() -> void {
        auto file_dump = ""s;

        {
            auto const lock = std::lock_guard(log_mutex);
            file_dump.swap(file_queue);
        }

        if (file_dump.empty()) return;

        static auto log_file = [] {
            auto const time = get_tm_time();
            auto const real_path = ProjectSettings::get_singleton()->globalize_path(to_gstring("user://game/logs/{:%Y.%m.%d %H-%M-%S}.txt"f(time)));
            auto const std_path = real_path.utf8().get_data();

            std::filesystem::create_directories(std::filesystem::path(std_path).parent_path());
            return std::ofstream(std_path);
        }();

        std::print(log_file, "{}", file_dump);
    }

    template <>
    void log<LogType::NORMAL>(std::string_view message, LOC_PARAM) {
        auto const time = get_time();
        auto const info = get_info(__loc__);

        auto const current_log = "{}None:    {}{}"f(time, info, message);
        auto log = ""s;

        if (colored_log) log = "\033[97m{}None:    {}\033[37m{}\033[0m"f(time, info, message);
        else log = current_log;

        LogQueue::store(log, current_log);
    }

    template <>
    void log<LogType::VERBOSE>(std::string_view message, LOC_PARAM) {
        if (not log_verbose) return;

        auto const time = get_time();
        auto const info = get_info(__loc__);

        auto const current_log = "{}Verbose: {}{}"f(time, info, message);
        auto log = ""s;

        if (colored_log) log = "\033[90m{}Verbose: {}{}\033[0m"f(time, info, message);
        else log = current_log;

        LogQueue::store(log, current_log);
    }

    template <>
    void log<LogType::INFO>(std::string_view message, LOC_PARAM) {
        auto const time = get_time();
        auto const info = get_info(__loc__);

        auto const current_log = "{}Info:    {}{}"f(time, info, message);
        auto log = ""s;

        if (colored_log) log = "\033[96m{}Info:    {}\033[36m{}\033[0m"f(time, info, message);
        else log = current_log;

        LogQueue::store(log, current_log);
    }

    template <>
    void log<LogType::WARNING>(std::string_view message, LOC_PARAM) {
        auto const time = get_time();
        auto const info = get_info(__loc__);

        auto const current_log = "{}Warning: {}{}"f(time, info, message);
        auto log = ""s;

        if (colored_log) log = "\033[93m{}Warning: {}\033[33m{}\033[0m"f(time, info, message);
        else log = current_log;

        LogQueue::store(log, current_log);
    }

    template <>
    void log<LogType::ERROR>(std::string_view message, LOC_PARAM) {
        auto const time = get_time();
        auto const info = get_info(__loc__);

        auto const current_log = "{}Error:   {}{}"f(time, info, message);
        auto log = ""s;

        if (colored_log) log = "\033[91m{}Error:   {}\033[31m{}\033[0m"f(time, info, message);
        else log = current_log;

        LogQueue::store(log, current_log);
    }
}