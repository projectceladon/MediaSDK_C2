#include <future>
#include "C2Buffer.h"

namespace android {

class C2Fence::Impl
{
public:
    std::shared_future<void> future_;
};

c2_status_t C2Fence::wait(nsecs_t timeoutNs)
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

class C2Event::Impl
{
public:
    std::promise<void> promise_;
};

C2Event::C2Event()
{
    mImpl = std::make_shared<Impl>();
    mImpl->promise_ = std::promise<void>();
}

C2Fence C2Event::fence() const
{
    C2Fence fence;
    fence.mImpl = std::make_shared<C2Fence::Impl>();
    fence.mImpl->future_ = mImpl->promise_.get_future();
    return fence;
}

c2_status_t C2Event::fire()
{
    c2_status_t res = C2_OK;
    try {
        mImpl->promise_.set_value();
    }
    catch(const std::future_error& ex) {
        res = C2_BAD_STATE;
    }
    return res;
}

} //namespace android;
