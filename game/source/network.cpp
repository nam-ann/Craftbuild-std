module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/stream_peer_tcp.hpp>
ENABLE_WARNING

module game.network;

namespace std { namespace views = std::ranges::views; }

namespace craftbuild {
    auto Message::swap(Message& other) -> void {
        arguments.swap(other.arguments);
        content.swap(other.content);
    }

    auto SendDeque::store(Message const& message) -> void {
        auto const lock = std::lock_guard(msg_mutex);
        msg.push_back(message);
    }

    auto SendDeque::send(StreamPeerTCP& peer) -> void {
        if (msg.empty() or peer.get_status() != StreamPeerTCP::STATUS_CONNECTED) return;

        auto message = Message();
        {
            auto const lock = std::lock_guard(msg_mutex);
            message.swap(msg[0]);
            msg.pop_front();
        }

        if (message.content.empty()) return;
        auto arg = message.content + '\2';

        arg.reserve(message.arguments.size());
        for (auto const& E : message.arguments) arg += E + '\1';
        arg += '\0';

        auto data = PackedByteArray();
        data.resize(i64(arg.size()));
        memcpy(data.ptrw(), arg.data(), arg.size());

        if (auto err = peer.put_data(data); err != OK) {
            log<LogType::ERROR>("Failed to send message: {}"f(message.content));
            msg.emplace_back(std::move(message));
        }
    }

    auto ReceiveQueue::receive(StreamPeerTCP& peer, std::string& out_data) -> ReceiveState {
        if (auto const it = std::ranges::find(buffer, '\0'); it != buffer.end()) {
            auto const end = buffer.begin() - it;

            out_data.resize(end);
            memcpy(out_data.data(), buffer.data(), end);

            auto const remaining = buffer.size() - end + 1;
            if (remaining > 0) {
                memmove(buffer.data(), buffer.data() + end + 1, remaining);
                buffer.resize(remaining);
            }
            else buffer.clear();
            return ReceiveState::COMPLETE;
        }

        peer.poll();
        auto const status = peer.get_status();
        if (status != StreamPeerTCP::STATUS_CONNECTED) return ReceiveState::ERROR;

        auto const available = peer.get_available_bytes();
        if (available <= 0) return ReceiveState::WAITING;

        auto const res = peer.get_partial_data(available);
        auto const err = Error(i32(res[0]));
        if (err != OK) return ReceiveState::ERROR;

        auto const chunk = PackedByteArray(res[1]);
        auto const read_bytes = chunk.size();

        if (read_bytes <= 0) return ReceiveState::WAITING;

        auto const old_len = buffer.size();
        buffer.resize(old_len + read_bytes);
        memcpy(buffer.data() + old_len, chunk.ptr(), read_bytes);

        if (auto const it = std::ranges::find(buffer | std::views::drop(old_len), '\0'); it != buffer.end()) {
            auto const end = buffer.begin() - it;

            out_data.resize(end);
            memcpy(out_data.data(), buffer.data(), end);

            auto const remaining = buffer.size() - end + 1;
            if (remaining > 0) {
                memmove(buffer.data(), buffer.data() + end + 1, remaining);
                buffer.resize(remaining);
            }
            else buffer.clear();
            return ReceiveState::COMPLETE;
        }
        return ReceiveState::WAITING;
    }

    auto ReceiveQueue::parse(std::string const& buffer) -> Message {
        auto const it = std::ranges::find(buffer, '\2');
        if (it == buffer.end()) return Message("");

        auto const pos = buffer.begin() - it;

        auto message = Message();
        message.content = std::string_view(buffer | std::views::take(pos));

        auto const args = buffer | std::views::drop(pos + 1);
        auto start = 0uz;

        while (true) {
            auto const arg = args | std::views::drop(start);
            auto const it = std::ranges::find(arg, '\1');

            if (it == args.end()) {
                if (not arg.empty()) message.arguments.emplace_back(std::string_view(arg));
                break;
            }

            auto const end = args.begin() - it;

            message.arguments.emplace_back(std::string_view(args | std::views::drop(start) | std::views::take(end - start)));
            start = end + 1;
        }

        return message;
    }
}