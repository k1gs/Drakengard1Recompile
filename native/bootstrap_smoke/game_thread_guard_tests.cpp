#include "game_thread_guard.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>

namespace {
void check(bool value) {
    if (!value) throw std::runtime_error("game thread ownership regression failed");
}

void hostFailureStopsAndJoinsBeforePropagation() {
    std::mutex mutex;
    std::condition_variable changed;
    std::atomic<bool> stop{}, finished{}, observedStop{};
    std::thread worker([&] {
        std::unique_lock lock(mutex);
        // Bound a broken guard's failure path rather than hanging the suite.
        changed.wait_for(lock, std::chrono::seconds(2), [&] { return stop.load(); });
        observedStop = stop.load();
        finished = true;
    });
    struct UiFailure { const void* identity; };
    const int identity{};
    bool caught{};
    try {
        drakengard::GameThreadJoinGuard owner(worker, [&]() noexcept {
            {
                std::lock_guard lock(mutex);
                stop = true;
            }
            changed.notify_all();
        });
        throw UiFailure{&identity};
    } catch (const UiFailure& failure) {
        caught = failure.identity == &identity;
        check(!worker.joinable());
        check(finished && observedStop);
    }
    check(caught);
}

void completedJoinDoesNotStopAgain() {
    bool finished{};
    unsigned stops{};
    std::thread worker([&] { finished = true; });
    {
        drakengard::GameThreadJoinGuard owner(worker, [&]() noexcept { ++stops; });
        worker.join();
    }
    check(finished && !worker.joinable() && stops == 0);
}
}

int main() {
    hostFailureStopsAndJoinsBeforePropagation();
    completedJoinDoesNotStopAgain();
}
