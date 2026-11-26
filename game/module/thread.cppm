export module game.thread;

import std;

import misc.string;
import misc.number;

export namespace craftbuild {
	namespace thread_registry {
		inline std::unordered_map<std::jthread::id, std::string> threads;
		inline std::mutex threads_mutex;

        auto register_thread(std::string_view thread_name) -> void;
        auto get_name(std::jthread::id const& thread_id) -> std::string;
	};

    class ThreadPool final {
        std::vector<std::jthread> workers;
        std::queue<std::function<void()>> tasks;

        mutable std::mutex mutex;
        std::condition_variable cv;
        bool stop;

    public:
        ThreadPool(usize n);
        ~ThreadPool();

        auto enqueue(auto&& f) -> void {
            {
                auto lock = std::lock_guard(mutex);
                tasks.emplace(std::forward<decltype(f)>(f));
            }
            cv.notify_one();
        }
    };
}
