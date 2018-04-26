#include <future>
#include "C2Buffer.h"

class C2Fence::Impl
{
public:
    std::shared_future<void> future_;
};

c2_status_t C2Fence::wait(c2_nsecs_t timeoutNs)
{
    c2_status_t res = C2_OK;
    try {
        if (mImpl) {
            std::future_status sts = mImpl->future_.wait_for(std::chrono::nanoseconds(timeoutNs));
            switch(sts) {
                case std::future_status::deferred: {
                    res = C2_BAD_STATE;
                    break;
                }
                case std::future_status::ready: {
                    res = C2_OK;
                    break;
                }
                case std::future_status::timeout: {
                    res = C2_TIMED_OUT;
                    break;
                }
                default: {
                    res = C2_CORRUPTED;
                }
            }
        } // no fence means wait returns immediately with OK
    } catch(const std::exception&) {
        res = C2_CORRUPTED;
    }
    return res;
}

bool C2Fence::valid() const
{
    return mImpl && mImpl->future_.valid();
}

struct _C2FenceFactory
{
public:
    // Creates fence already ready.
    static C2Fence CreateFence()
    {
        std::promise<void> promise;

        C2Fence fence;
        fence.mImpl = std::make_shared<C2Fence::Impl>();
        fence.mImpl->future_ = promise.get_future();
        promise.set_value();
        return fence;
    }
};

C2Fence C2Event::fence() const
{
    return _C2FenceFactory::CreateFence();
}

c2_status_t C2Event::fire()
{
    return C2_OK;
}
