#include "C2Buffer.h"
#include "C2BlockAllocator.h"

#include <future>

namespace android {

class C2Fence::Impl
{
public:
    std::shared_future<void> future_;
};

C2Error C2Fence::wait(nsecs_t timeoutNs)
{
    C2Error res = C2_OK;
    try {
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
    } catch(const std::exception&) {
        res = C2_CORRUPTED;
    }
    return res;
}

bool C2Fence::valid() const
{
    return mImpl->future_.valid();
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

C2Error C2Event::fire()
{
    C2Error res = C2_OK;
    try {
        mImpl->promise_.set_value();
    }
    catch(const std::future_error& ex) {
        res = C2_BAD_STATE;
    }
    return res;
}

typedef std::shared_ptr<std::vector<uint8_t>> DataBuffer;

class C2Block1D::Impl
{
public:
    DataBuffer data_;
};

class C2ReadView::Impl
{
public:
    DataBuffer data_;
};

const uint8_t *C2ReadView::data()
{
    return &(mImpl->data_->front());
}

class C2WriteView::Impl
{
public:
    DataBuffer data_;
};

uint8_t *C2WriteView::data()
{
    return &(mImpl->data_->front());
}

C2Acquirable<C2ReadView> C2ConstLinearBlock::map() const
{
    C2Error error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    C2ReadView read_view(this);
    read_view.mImpl = std::make_shared<C2ReadView::Impl>();
    read_view.mImpl->data_ = mImpl->data_;

    return C2Acquirable<C2ReadView>(error, event.fence(), read_view);
}

C2Acquirable<C2WriteView> C2LinearBlock::map()
{
    C2Error error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    C2WriteView write_view(this);
    write_view.mImpl = std::make_shared<C2WriteView::Impl>();
    write_view.mImpl->data_ = mImpl->data_;

    return C2Acquirable<C2WriteView>(error, event.fence(), write_view);
}

C2ConstLinearBlock C2LinearBlock::share(size_t offset, size_t size, C2Fence fence)
{
    C2ConstLinearBlock const_linears_block(this, fence);
    const_linears_block.mImpl = std::make_shared<C2Block1D::Impl>();
    (void)offset;
    (void)size;
    const_linears_block.mImpl->data_ = mImpl->data_;

    return const_linears_block;
}

class C2Block2D::Impl
{
public:
    DataBuffer data_;
};

class C2GraphicView::Impl
{
public:
    DataBuffer data_;
};

uint8_t *C2GraphicView::data()
{
    return &(mImpl->data_->front());
}

C2Acquirable<const C2GraphicView> C2ConstGraphicBlock::map() const
{
    C2Error error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    C2GraphicView graphic_view(this);
    graphic_view.mImpl = std::make_shared<C2GraphicView::Impl>();
    graphic_view.mImpl->data_ = mImpl->data_;

    return C2Acquirable<const C2GraphicView>(error, event.fence(), graphic_view);
}

C2Acquirable<C2GraphicView> C2GraphicBlock::map()
{
    C2Error error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    C2GraphicView graphic_view(this);
    graphic_view.mImpl = std::make_shared<C2GraphicView::Impl>();
    graphic_view.mImpl->data_ = mImpl->data_;

    return C2Acquirable<C2GraphicView>(error, event.fence(), graphic_view);
}

C2ConstGraphicBlock C2GraphicBlock::share(const C2Rect &crop, C2Fence fence)
{
    C2ConstGraphicBlock const_graphics_block(this, fence);
    const_graphics_block.mImpl = std::make_shared<C2Block2D::Impl>();
    (void)crop;//const_graphics_block.setCrop(crop); crop not supported yet
    const_graphics_block.mImpl->data_ = mImpl->data_;

    return const_graphics_block;
}

class C2BufferData::Impl
{
public:
    Type type_;
    std::list<C2ConstLinearBlock> linear_blocks;
    std::list<C2ConstGraphicBlock> graphic_blocks;
};

C2BufferData::C2BufferData(C2ConstLinearBlock linear_block)
{
    mImpl = std::make_shared<Impl>();
    mImpl->type_ = LINEAR;
    mImpl->linear_blocks.push_back(linear_block);
}

C2BufferData::C2BufferData(C2ConstGraphicBlock graphic_block)
{
    mImpl = std::make_shared<Impl>();
    mImpl->type_ = GRAPHIC;
    mImpl->graphic_blocks.push_back(graphic_block);
}

C2BufferData::Type C2BufferData::type() const
{
    return mImpl->type_;
}

const std::list<C2ConstLinearBlock> C2BufferData::linearBlocks() const
{
    return mImpl->linear_blocks;
}

const std::list<C2ConstGraphicBlock> C2BufferData::graphicBlocks() const
{
    return mImpl->graphic_blocks;
}

C2Buffer::C2Buffer(const C2BufferData& buffer_data) : buffer_data_(buffer_data)
{

}

const C2BufferData C2Buffer::data() const
{
    return buffer_data_;
}

class C2BlockAllocatorImpl : public C2BlockAllocator
{
    C2Error allocateLinearBlock(
            uint32_t capacity, C2MemoryUsage usage __unused,
            std::shared_ptr<C2LinearBlock> *block /* nonnull */) override {

        C2Error res = C2_OK;
        try {
            DataBuffer data_buffer = std::make_shared<std::vector<uint8_t>>();
            data_buffer->resize(capacity);
            *block = std::make_shared<C2LinearBlock>(capacity);
            (*block)->mImpl = std::make_shared<C2Block1D::Impl>();
            (*block)->mImpl->data_ = data_buffer;
        }
        catch(const std::bad_alloc&) {
            res = C2_NO_MEMORY;
        }
        return res;
    }

    C2Error allocateGraphicBlock(
            uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
            C2MemoryUsage usage __unused,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) override {

        C2Error res = C2_OK;
        if(format == 0/*nv12*/) {
            try {
                DataBuffer data_buffer = std::make_shared<std::vector<uint8_t>>();
                data_buffer->resize(width * height * 3 / 2);
                *block = std::make_shared<C2GraphicBlock>(width, height);
                (*block)->mImpl = std::make_shared<C2Block2D::Impl>();
                (*block)->mImpl->data_ = data_buffer;
            }
            catch(const std::bad_alloc&) {
                res = C2_NO_MEMORY;
            }
        } else {
            res = C2_UNSUPPORTED;
        }
        return res;
    }
};

} //namespace android;

using namespace android;
status_t GetC2BlockAllocator(std::shared_ptr<C2BlockAllocator>* allocator)
{
    *allocator = std::make_shared<C2BlockAllocatorImpl>();
    return C2_OK;
}
