/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

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
        pool_impl_ = std::make_shared<MfxPoolImpl>();
    }

    ~MfxPool()
    {
        // Correct method to free pool instance.
        // This waiting for destroyed is needed as module with pool could be unloaded from
        // memory while some resource could try to add itself back to the pool.
        std::future<void> destroyed = pool_impl_->Destroyed();
        pool_impl_.reset();
        destroyed.wait();
    }

    MFX_CLASS_NO_COPY(MfxPool<T>)

public:
    void Append(std::unique_ptr<T>&& free_item)
    {
        pool_impl_->Append(std::move(free_item));
    }

    std::shared_ptr<T> Alloc()
    {
        return pool_impl_->Alloc();
    }

private:
    // This impl auxilary class is separated as it should be instantiated as shared_ptr,
    // that ptr should be freed with MfxPoolImpl<T>::Destroy.
    class MfxPoolImpl : public std::enable_shared_from_this<MfxPoolImpl>
    {
    private:
        std::mutex mutex_;
        // Signals new free resource added to the pool.
        std::condition_variable condition_;
        // Collection of free resources.
        std::list<std::unique_ptr<T>> free_;
        // Signal when instance destroyed
        std::promise<void> destroyed_;
    public:
        ~MfxPoolImpl()
        {
            destroyed_.set_value();
        }

        void Append(std::unique_ptr<T>&& free_item)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            free_.push_back(std::move(free_item));
            condition_.notify_one();
        }
        // Returns allocated resource through shared_ptr.
        // That shared_ptr has Deleter returning released resource back to the pool.
        // The methods blocks if no free resource is in the pool.
        std::shared_ptr<T> Alloc()
        {
            std::unique_lock<std::mutex> lock(mutex_);

            if (free_.empty()) {
                bool success = condition_.wait_for(lock, std::chrono::seconds(1), [this] { return !free_.empty(); });
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

            std::unique_ptr<T> free_block = std::move(free_.front());
            free_.pop_front();
            return std::shared_ptr<T>(free_block.release(), deleter);
        }

        std::future<void> Destroyed()
        {
            return destroyed_.get_future();
        }
    };

private:
    std::shared_ptr<MfxPoolImpl> pool_impl_;
};
