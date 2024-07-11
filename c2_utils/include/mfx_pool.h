// Copyright (c) 2017-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <utils/Log.h>
#include <mutex>
#include <future>
#include <memory>
#include <list>

// Generic pool resource management providing allocated resource through
// std::shared_ptr.
// Keeps unique_ptr inside pool and provides shared_ptr outside.
// Returned shared_ptr has custom deleter to return freed object back to the pool.
// Automatically tracks returned shared_ptrs and
// returns released resources back to the pool.
// Raw pointers of the same objects are retained.
template<typename T>
class MfxPool
{
public:
    MfxPool()
    {
        m_poolImpl = std::make_shared<MfxPoolImpl>();
    }

    ~MfxPool()
    {
        // Correct method to free pool instance.
        // This waiting for destroyed is needed as module with pool could be unloaded from
        // memory while some resource could try to add itself back to the pool.
        try
        {
            std::future<void> destroyed = m_poolImpl->Destroyed();
            m_poolImpl.reset();
            destroyed.wait();
        }
        catch(const std::exception& e)
        {
            // ALOGE("Destroyed function execution error");
        }
    }

    MFX_CLASS_NO_COPY(MfxPool<T>)

public:
    void Append(std::unique_ptr<T>&& free_item)
    {
        m_poolImpl->Append(std::move(free_item));
    }

    std::shared_ptr<T> Alloc()
    {
        return m_poolImpl->Alloc();
    }

private:
    // This impl auxilary class is separated as it should be instantiated as shared_ptr,
    // that ptr should be freed with MfxPoolImpl<T>::Destroy.
    class MfxPoolImpl : public std::enable_shared_from_this<MfxPoolImpl>
    {
    MFX_CLASS_NO_COPY(MfxPoolImpl)
    private:
        std::mutex m_mutex;
        // Signals new free resource added to the pool.
        std::condition_variable m_condition;
        // Collection of free resources.
        std::list<std::unique_ptr<T>> m_free;
        // Signal when instance destroyed
        std::promise<void> m_destroyed;
    public:
        MfxPoolImpl() = default;
        ~MfxPoolImpl()
        {
            try
            {
                m_destroyed.set_value();
            }
            catch(const std::exception& e)
            {
                // ALOGE("set_value function execution error");
            }
        }

        void Append(std::unique_ptr<T>&& free_item)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_free.push_back(std::move(free_item));
            m_condition.notify_one();
        }
        // Returns allocated resource through shared_ptr.
        // That shared_ptr has Deleter returning released resource back to the pool.
        // The methods blocks if no free resource is in the pool.
        std::shared_ptr<T> Alloc()
        {
            std::unique_lock<std::mutex> lock(m_mutex);

            if (m_free.empty()) {
                bool success = m_condition.wait_for(lock, std::chrono::seconds(1), [this] { return !m_free.empty(); });
                if (!success) return nullptr;
            }

            std::weak_ptr<MfxPoolImpl> weak_this { this->shared_from_this() };
            auto deleter = [weak_this] (T* item) {
                // weak_ptr and its lock needed to make possible allocated resources
                // live longer than this pool
                std::shared_ptr<MfxPoolImpl> shared_this = weak_this.lock();
                if (shared_this) {
                    shared_this->Append(std::unique_ptr<T>(item));
                } else {
                    delete item;
                }
            };

            std::unique_ptr<T> free_block = std::move(m_free.front());
            m_free.pop_front();
            return std::shared_ptr<T>(free_block.release(), std::move(deleter));
        }

        std::future<void> Destroyed()
        {
            return m_destroyed.get_future();
        }
    };

private:
    std::shared_ptr<MfxPoolImpl> m_poolImpl;
};
