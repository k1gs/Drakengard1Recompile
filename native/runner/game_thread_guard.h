#pragma once

#include <thread>
#include <type_traits>
#include <utility>

namespace drakengard {

// Keep a live game thread from masking a host/UI exception with std::terminate.
// Normal shutdown already joins it; only an outstanding thread needs stopping.
template<class Stop>
class GameThreadJoinGuard {
    static_assert(std::is_nothrow_copy_constructible_v<Stop>);
    static_assert(std::is_nothrow_move_constructible_v<Stop>);
    static_assert(std::is_nothrow_invocable_v<Stop&>);
    std::thread& thread;
    Stop stop;

public:
    GameThreadJoinGuard(std::thread& thread, Stop stop) noexcept
        : thread(thread), stop(std::move(stop)) {}
    GameThreadJoinGuard(const GameThreadJoinGuard&) = delete;
    GameThreadJoinGuard& operator=(const GameThreadJoinGuard&) = delete;

    ~GameThreadJoinGuard() noexcept {
        if (!thread.joinable()) return;
        stop();
        thread.join();
    }
};

} // namespace drakengard
