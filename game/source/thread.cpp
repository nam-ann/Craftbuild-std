module game.thread;

namespace std { namespace views = ranges::views; }

namespace craftbuild {
    namespace thread_registry {
        auto register_thread(std::string_view thread_name) -> void {
            auto const lock = std::lock_guard(threads_mutex);
            threads[std::this_thread::get_id()] = thread_name;
        }

        auto get_name(std::jthread::id const& thread_id) -> std::string {
            auto const lock = std::lock_guard(threads_mutex);
            auto it = threads.find(thread_id);
            if (it == threads.end()) return "Main";
            return it->second;
        }
    }

    ThreadPool::ThreadPool(usize n) : stop(false) {
        for (usize i : std::views::iota(0uz, n)) {
            workers.emplace_back([this]() {
                while (true) {
                    std::function<void()> task;
                    {
                        auto lock = std::unique_lock(mutex);
                        cv.wait(lock, [this] { return stop or not tasks.empty(); });

                        if (stop and tasks.empty()) return;

                        task.swap(tasks.front());
                        tasks.pop();
                    }
                    task();
                }
            });
        }
    }

    ThreadPool::~ThreadPool() {
        {
            auto const lock = std::lock_guard(mutex);
            stop = true;
        }
        cv.notify_all();
    }
}