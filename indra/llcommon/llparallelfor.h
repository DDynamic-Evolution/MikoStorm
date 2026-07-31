/**
 * @file   llparallelfor.h
 * @brief  Parallel-for utility built on top of LL::ThreadPool.
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Copyright (c) 2026, MikoStorm
 * $/LicenseInfo$
 */

#ifndef LL_PARALLELFOR_H
#define LL_PARALLELFOR_H

#include "threadpool.h"
#include <atomic>
#include <exception>
#include <future>
#include <iterator>
#include <mutex>

namespace LL
{

    /**
     * Splits [begin, end) into chunks of up to chunk_size elements each and
     * posts each chunk as a task to the given ThreadPool. Blocks until all
     * chunks have completed, then rethrows the first exception (if any) that
     * a chunk raised.
     *
     * func is called with (element, slot) for every element. slot is a
     * unique index for the chunk the element belongs to; consumers can use it
     * to index into per-chunk thread-local scratch buffers without any
     * locking (no two chunks share a slot).
     *
     * Requires random access iterators (the LLCullResult iterators are raw
     * pointers).
     *
     * Runs serially when the range is small or the pool has no workers.
     */
    template <typename Iter, typename Func>
    void parallel_for(Iter begin, Iter end, Func&& func,
                      ThreadPool& pool, size_t chunk_size = 16)
    {
        const size_t count = static_cast<size_t>(std::distance(begin, end));
        if (count == 0)
        {
            return;
        }

        if (pool.getWidth() == 0 || count <= chunk_size)
        {
            size_t slot = 0;
            for (Iter it = begin; it != end; ++it, ++slot)
            {
                func(*it, slot);
            }
            return;
        }

        const size_t num_chunks = (count + chunk_size - 1) / chunk_size;

        std::atomic<size_t> pending{ num_chunks };
        std::promise<void> completion;
        auto future = completion.get_future();

        std::exception_ptr first_exception;
        std::mutex exception_mutex;

        auto& queue = pool.getQueue();

        for (size_t c = 0; c < num_chunks; ++c)
        {
            const size_t cstart = c * chunk_size;
            const size_t cend = std::min(cstart + chunk_size, count);
            const size_t slot = c;
            const Iter cbegin = begin + cstart;
            const Iter cend_it = begin + cend;

            bool posted = queue.post(
                [cbegin, cend_it, slot, &func, &pending, &completion,
                 &first_exception, &exception_mutex]()
                {
                    try
                    {
                        for (Iter it = cbegin; it != cend_it; ++it)
                        {
                            func(*it, slot);
                        }
                    }
                    catch (...)
                    {
                        std::lock_guard<std::mutex> lock(exception_mutex);
                        if (!first_exception)
                        {
                            first_exception = std::current_exception();
                        }
                    }
                    if (--pending == 0)
                    {
                        completion.set_value();
                    }
                });

            if (!posted)
            {
                // Pool queue closed; skip this chunk.
                if (--pending == 0)
                {
                    completion.set_value();
                }
            }
        }

        future.wait();

        if (first_exception)
        {
            std::rethrow_exception(first_exception);
        }
    }

} // namespace LL

#endif // LL_PARALLELFOR_H
