module;

#include <defs.hpp>

DISABLE_WARNING
#include <godot_cpp/classes/stream_peer_tcp.hpp>
ENABLE_WARNING

export module game.network;

import std;

import misc.string;
import misc.number;
import game.logger;

using namespace godot;
using namespace std::literals;

export namespace craftbuild {
    struct Message final {
        std::string content;
        std::vector<std::string> arguments;

        auto swap(Message& other) -> void;
    };

    struct SendDeque final {
        std::deque<Message> msg;
        mutable std::mutex msg_mutex;

        auto store(Message const& message) -> void;
        auto send(StreamPeerTCP& peer) -> void;
    };

    enum class ReceiveState {
        COMPLETE,
        WAITING,
        ERROR
    };

    struct ReceiveQueue final {
        std::string buffer;

        auto receive(StreamPeerTCP& peer, std::string& out_data) -> ReceiveState;
        static auto parse(std::string const& buffer) -> Message;
    };
}