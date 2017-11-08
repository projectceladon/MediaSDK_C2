#include "C2Buffer.h"
#include "C2BlockAllocator.h"
#include "mfx_defs.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_utils.h"
#include "mfx_gralloc_allocator.h"

#include <ui/Rect.h>
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

// TODO: as C2BlockxD are not shared their vectors to Read/WriteViews anymore
// probably vector could be stored directly here, not shared_ptr to it
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
    C2ConstLinearBlock const_linears_block(this, offset, size, fence);
    const_linears_block.mImpl = std::make_shared<C2Block1D::Impl>();
    const_linears_block.mImpl->data_ = mImpl->data_;

    return const_linears_block;
}

class GraphicSwBuffer
{
private:
    std::vector<uint8_t> data_;
    uint32_t width_ {};
    uint32_t height_ {};

public:
    uint8_t* data() { return &data_.front(); }

    status_t Alloc(uint32_t width, uint32_t height)
    {
        width_ = width;
        height_ = height;
        data_.resize(width_ * height_ * 3 / 2); // alloc for nv12 format
        return C2_OK;
    }

    void InitNV12PlaneLayout(C2PlaneLayout* plane_layout)
    {
        ::InitNV12PlaneLayout(width_, height_, plane_layout);
    }
};

class C2Block2D::Impl : public std::enable_shared_from_this<C2Block2D::Impl>
{
public:
    std::atomic_bool locked_ { false };
    GraphicSwBuffer sw_buffer_;
    buffer_handle_t handle_ {};
    std::shared_ptr<MfxGrallocAllocator> gralloc_allocator_;

public:
    ~Impl() {
        if (handle_ && gralloc_allocator_) gralloc_allocator_->Free(handle_);
    }

    C2Error Map(C2Event* event, std::unique_ptr<C2GraphicView::Impl>* view_impl);

    void Unmap() {
        if (handle_ && gralloc_allocator_) gralloc_allocator_->UnlockFrame(handle_);
        locked_.store(false);
    }
};

const C2Handle *C2Block2D::handle() const
{
    return mImpl->handle_;
};

C2Error C2Block2D::Impl::Map(C2Event* event, std::unique_ptr<C2GraphicView::Impl>* view_impl)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error error = C2_OK;
    uint8_t* data = nullptr;
    C2PlaneLayout plane_layout {};

    bool locked = locked_.exchange(true);
    if (locked) {
        error = C2_BAD_STATE;
    } else {
        if (nullptr == handle_) { // system memory block
            sw_buffer_.InitNV12PlaneLayout(&plane_layout);
            data = sw_buffer_.data();
        } else {
            MFX_DEBUG_TRACE_P(handle_);
            error = gralloc_allocator_->LockFrame(handle_, &data, &plane_layout);
        }
    }

    MFX_DEBUG_TRACE__android_C2Error(error);

    if (C2_OK == error) {
        event->fire(); // map is always ready to read
    }

    *view_impl = std::make_unique<C2GraphicView::Impl>(data, plane_layout, shared_from_this());

    return error;
}

class C2GraphicView::Impl
{
public:
    uint8_t* data_ {};
    C2PlaneLayout plane_layout_ {};
    // shared_ptr to prevent C2Block::Impl destruction before this destruction
    std::shared_ptr<C2Block2D::Impl> block_impl_ {};

public:
    Impl(uint8_t* data, const C2PlaneLayout& plane_layout, const std::shared_ptr<C2Block2D::Impl>& block_impl)
        : data_(data), plane_layout_(plane_layout), block_impl_(block_impl) { }

    ~Impl() {
        block_impl_->Unmap();
    }
};

uint8_t *C2GraphicView::data()
{
    return mImpl->data_;
}

const uint8_t *C2GraphicView::data() const
{
    return mImpl->data_;
}

const C2PlaneLayout* C2GraphicView::planeLayout() const
{
    return &(mImpl->plane_layout_);
}

C2Acquirable<const C2GraphicView> C2ConstGraphicBlock::map() const
{
    MFX_DEBUG_TRACE_FUNC;

    C2Event event;
    std::unique_ptr<C2GraphicView::Impl> view_impl;
    C2Error error = mImpl->Map(&event, &view_impl);

    C2GraphicView graphic_view(this);
    graphic_view.mImpl = std::shared_ptr<C2GraphicView::Impl>(std::move(view_impl));
    return C2Acquirable<const C2GraphicView>(error, event.fence(), graphic_view);
}

C2Acquirable<C2GraphicView> C2GraphicBlock::map()
{
    MFX_DEBUG_TRACE_FUNC;

    C2Event event;
    std::unique_ptr<C2GraphicView::Impl> view_impl;
    C2Error error = mImpl->Map(&event, &view_impl);

    C2GraphicView graphic_view(this);
    graphic_view.mImpl = std::shared_ptr<C2GraphicView::Impl>(std::move(view_impl));
    return C2Acquirable<C2GraphicView>(error, event.fence(), graphic_view);
}

C2ConstGraphicBlock C2GraphicBlock::share(const C2Rect &crop, C2Fence fence)
{
    C2ConstGraphicBlock const_graphics_block(this, fence);
    const_graphics_block.mImpl = mImpl;
    (void)crop;//const_graphics_block.setCrop(crop); crop not supported yet

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
public:
    struct CreateResult
    {
        std::shared_ptr<C2BlockAllocator> allocator;
        status_t status;
    };

public:
    static CreateResult Create();

    ~C2BlockAllocatorImpl() = default;

public:
    C2Error Map(const C2GraphicBlock* graphic_block, uint8_t** data);

private:
    C2BlockAllocatorImpl() = default;

    C2Error Init();

    MFX_CLASS_NO_COPY(C2BlockAllocatorImpl)

private: // C2BlockAllocator impl
    C2Error allocateLinearBlock(
            uint32_t capacity, C2MemoryUsage usage __unused,
            std::shared_ptr<C2LinearBlock> *block /* nonnull */) override;

    C2Error allocateGraphicBlock(
            uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
            C2MemoryUsage usage __unused,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) override;

private:
    std::shared_ptr<MfxGrallocAllocator> gralloc_allocator_;
};

C2Error C2BlockAllocatorImpl::Init()
{
    std::unique_ptr<MfxGrallocAllocator> allocator;
    C2Error res = MfxGrallocAllocator::Create(&allocator);
    if (res == C2_OK) {
        gralloc_allocator_ = std::shared_ptr<MfxGrallocAllocator>(std::move(allocator));
    }
    return res;
}

C2BlockAllocatorImpl::CreateResult C2BlockAllocatorImpl::Create()
{
    CreateResult res {};
    res.status = OK;

    C2BlockAllocatorImpl* alloc = new (std::nothrow)C2BlockAllocatorImpl();
    if (alloc) {
        res.status = alloc->Init();
        if (res.status == OK) res.allocator.reset(alloc);
    } else {
        res.status = C2_NO_MEMORY;
    }
    return res;
}

C2Error C2BlockAllocatorImpl::allocateLinearBlock(
        uint32_t capacity, C2MemoryUsage usage __unused,
        std::shared_ptr<C2LinearBlock> *block /* nonnull */) {

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

C2Error C2BlockAllocatorImpl::allocateGraphicBlock(
        uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
        C2MemoryUsage usage __unused,
        std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {

    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    if ((usage.mConsumer & C2MemoryUsage::kHardwareEncoder) ||
        (usage.mProducer & C2MemoryUsage::kHardwareDecoder)) {

        buffer_handle_t handle {};

        res = gralloc_allocator_->Alloc(width, height, &handle);

        if (C2_OK == res) {
            MFX_DEBUG_TRACE_P(handle);
            *block = std::make_shared<C2GraphicBlock>(width, height);
            (*block)->mImpl = std::make_shared<C2Block2D::Impl>();
            (*block)->mImpl->handle_ = handle;
            (*block)->mImpl->gralloc_allocator_ = gralloc_allocator_;
        }
    } else {
        if (format == 0/*nv12*/) {
            try {
                *block = std::make_shared<C2GraphicBlock>(width, height);
                (*block)->mImpl = std::make_shared<C2Block2D::Impl>();
                (*block)->mImpl->sw_buffer_.Alloc(width, height);
            }
            catch(const std::bad_alloc&) {
                res = C2_NO_MEMORY;
            }
        } else {
            res = C2_UNSUPPORTED;
        }
    }
    return res;
}

} //namespace android;

using namespace android;

status_t GetC2BlockAllocator(std::shared_ptr<C2BlockAllocator>* allocator)
{
    static C2BlockAllocatorImpl::CreateResult g_create_result =
        C2BlockAllocatorImpl::Create();

    *allocator = g_create_result.allocator;
    return g_create_result.status;
}
