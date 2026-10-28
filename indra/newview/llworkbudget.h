#ifndef LL_WORK_BUDGET_H
#define LL_WORK_BUDGET_H
#include <chrono>
#include <cstddef>
// Cooperative budget: never interrupt a viewer operation in the middle of a mutation.
class LLWorkBudget
{
public:
    using Clock = std::chrono::steady_clock;
    LLWorkBudget(std::size_t limit, Clock::duration duration)
        : mLimit(limit), mStart(Clock::now()), mDeadline(mStart + duration) {}
    bool take()
    {
        if (mUsed >= mLimit || Clock::now() >= mDeadline) return false;
        ++mUsed;
        return true;
    }
    double elapsedMilliseconds() const
    {
        return std::chrono::duration<double, std::milli>(Clock::now() - mStart).count();
    }
private:
    std::size_t mLimit, mUsed{0};
    Clock::time_point mStart, mDeadline;
};
#endif
