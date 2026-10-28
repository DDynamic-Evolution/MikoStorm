// A queued viewer action must not outlive its request or server session.
#ifndef LL_MCP_TASK_H
#define LL_MCP_TASK_H
#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <exception>
#include <utility>

template<class Result> class LLMCPCancellableTask
{
public:
    explicit LLMCPCancellableTask(std::shared_ptr<std::atomic<bool>> session,
        std::shared_ptr<std::atomic<bool>> request = {})
        : mSession(std::move(session)), mRequest(std::move(request)), mFuture(mPromise.get_future()) {}
    template<class Function> void execute(Function&& function)
    {
        if (!alive() || mCancelled.load()) return;
        try { mPromise.set_value(function()); }
        catch (...) { mPromise.set_exception(std::current_exception()); }
    }
    bool wait(std::chrono::milliseconds timeout)
    {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (alive())
        {
            if (mFuture.wait_for(std::chrono::milliseconds(25)) == std::future_status::ready) return true;
            if (std::chrono::steady_clock::now() >= deadline) break;
        }
        mCancelled.store(true);
        return false;
    }
    Result get() { return mFuture.get(); }
private:
    bool alive() const { return mSession->load() && (!mRequest || mRequest->load()); }
    std::shared_ptr<std::atomic<bool>> mSession, mRequest;
    std::atomic<bool> mCancelled{false};
    std::promise<Result> mPromise;
    std::future<Result> mFuture;
};
#endif
