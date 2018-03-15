#include "C2Buffer.h"
#include "C2PlatformSupport.h"
#include "mfx_defs.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_utils.h"
#include "mfx_gralloc_allocator.h"

#include <ui/Rect.h>

namespace android {

class C2BlockAllocatorImpl;

class C2LinearAllocationMock : public C2LinearAllocation
{
    using C2LinearAllocation::C2LinearAllocation;
    friend class ::android::C2BlockAllocatorImpl;
public:
    C2LinearAllocationMock(size_t capacity):
        C2LinearAllocation(capacity)
    {
        byte_array_ = std::make_shared<std::vector<uint8_t>>(capacity);
    }

    virtual c2_status_t map(size_t/* offset*/, size_t/* size*/,
        C2MemoryUsage/* usage*/, int */*fenceFd*/, void **addr) override
    {
        c2_status_t res = C2_OK;
        if (byte_array_) {
            *addr = &(byte_array_->front());
        } else {
            res = C2_BAD_STATE;
        }
        return res;
    }

    virtual c2_status_t unmap(void *addr, size_t/* size*/, int */*fenceFd *//* nullable */) override
    {
        c2_status_t res = C2_OK;
        if (byte_array_) {
            if (addr != &(byte_array_->front())) res = C2_BAD_VALUE;
        } else {
            res = C2_BAD_STATE;
        }

        return res;
    }

    virtual bool isValid() const override { return nullptr != byte_array_; }

    virtual const C2Handle *handle() const override { return nullptr; }

    virtual bool equals(const std::shared_ptr<C2LinearAllocation> &/*other*/) const override
    {
        return false;
    }

public:
    std::shared_ptr<std::vector<uint8_t>> byte_array_;
};

class C2Block1D::Impl
{
public:
    std::shared_ptr<C2LinearAllocation> allocation_;
};

class C2ReadView::Impl
{
public:
    explicit Impl(const uint8_t *data)
        : mData(data), mError(C2_OK) {}

    explicit Impl(c2_status_t error)
        : mData(nullptr), mError(error) {}

    const uint8_t *data() const {
        return mData;
    }

    c2_status_t error() const {
        return mError;
    }

private:
    const uint8_t *mData;
    c2_status_t mError;
};

C2ReadView::C2ReadView(const _C2LinearCapacityAspect *parent, const uint8_t *data)
    : _C2LinearCapacityAspect(parent), mImpl(std::make_shared<Impl>(data)) {}

C2ReadView::C2ReadView(c2_status_t error)
    : _C2LinearCapacityAspect(0u), mImpl(std::make_shared<Impl>(error)) {}

const uint8_t *C2ReadView::data() const {
    return mImpl->data();
}

class C2WriteView::Impl
{
public:
    uint8_t* data_ {};
    c2_status_t error_ { C2_OK };
};

C2Block1D::C2Block1D(std::shared_ptr<C2LinearAllocation> alloc):
    _C2LinearRangeAspect(alloc.get()),
    mImpl(std::shared_ptr<Impl>(new Impl { alloc } )) {}

C2Block1D::C2Block1D(std::shared_ptr<C2LinearAllocation> alloc, size_t offset, size_t size):
    _C2LinearRangeAspect(alloc.get(), offset, size),
    mImpl(std::shared_ptr<Impl>(new Impl { alloc } )) {}

class C2ConstLinearBlockAccessor : public C2ConstLinearBlock {
    using C2ConstLinearBlock::C2ConstLinearBlock;
    friend class ::android::C2LinearBlock;
};

class C2LinearBlockAccessor : public C2LinearBlock {
    using C2LinearBlock::C2LinearBlock;
    friend class ::android::C2BlockAllocatorImpl;
};

class C2ConstLinearBlock::Impl
{
public:
    std::shared_ptr<C2LinearAllocation> allocation_;
};

C2ConstLinearBlock::C2ConstLinearBlock(std::shared_ptr<C2LinearAllocation> allocation,
    size_t offset, size_t size):
        C2Block1D(allocation, offset, size),
        mImpl(std::shared_ptr<Impl>(new Impl { allocation } )) {}

class C2LinearBlock::Impl
{
public:
    std::shared_ptr<C2LinearAllocation> allocation_;
};

C2LinearBlock::C2LinearBlock(std::shared_ptr<C2LinearAllocation> allocation):
    C2Block1D(allocation),
    mImpl(std::shared_ptr<Impl>(new Impl { allocation } )) {}

C2WriteView::C2WriteView(const _C2LinearRangeAspect *parent, uint8_t *base):
    _C2EditableLinearRange(parent),
    mImpl(std::shared_ptr<Impl>(new Impl { base, C2_OK } )) {}

C2WriteView::C2WriteView(c2_status_t error):
    _C2EditableLinearRange(nullptr),
    mImpl(std::shared_ptr<Impl>(new Impl { nullptr, error } )) {}

uint8_t *C2WriteView::data()
{
    return mImpl->data_;
}

class C2ReadViewMock : public C2ReadView
{
    using C2ReadView::C2ReadView;
    friend class ::android::C2ConstLinearBlock;
};

class C2AcquirableReadView : public C2Acquirable<C2ReadView> {
    using C2Acquirable::C2Acquirable;
    friend class ::android::C2ConstLinearBlock;
};

C2Acquirable<C2ReadView> C2ConstLinearBlock::map() const
{
    c2_status_t error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    void* data = nullptr;
    error = mImpl->allocation_->map({}, {}, {}, {}, &data);

    if (C2_OK == error) {
        C2ReadViewMock read_view(this, (uint8_t*)data);
        return C2AcquirableReadView(error, event.fence(), read_view);
    } else {
        C2ReadViewMock read_view(error);
        return C2AcquirableReadView(error, event.fence(), read_view);
    }
}

class C2WriteViewMock : public C2WriteView
{
    using C2WriteView::C2WriteView;
    friend class ::android::C2LinearBlock;
};

class C2AcquirableWriteView : public C2Acquirable<C2WriteView> {
    using C2Acquirable::C2Acquirable;
    friend class ::android::C2LinearBlock;
};

C2Acquirable<C2WriteView> C2LinearBlock::map()
{
    c2_status_t error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    void* data = nullptr;
    error = mImpl->allocation_->map({}, {}, {}, {}, &data);

    if (C2_OK == error) {
        C2WriteViewMock write_view(this, (uint8_t*)data);
        return C2AcquirableWriteView(error, event.fence(), write_view);
    } else {
        C2WriteViewMock write_view(error);
        return C2AcquirableWriteView(error, event.fence(), write_view);
    }
}

C2ConstLinearBlock C2LinearBlock::share(size_t offset, size_t size, C2Fence /*fence*/)
{
    C2ConstLinearBlockAccessor const_linear_block(mImpl->allocation_, offset, size);
    return const_linear_block;
}

class C2Block2D::Impl
{
public:
    std::shared_ptr<C2GraphicAllocation> allocation_;
};

C2Block2D::C2Block2D(const std::shared_ptr<C2GraphicAllocation>& alloc):
    _C2PlanarSection(alloc.get()),
    mImpl(std::shared_ptr<Impl>(new Impl { alloc } )) {}

class C2ConstGraphicBlock::Impl
{
public:
    std::shared_ptr<C2GraphicAllocation> allocation_;
};

class C2GraphicBlockAccessor : public C2GraphicBlock {
    using C2GraphicBlock::C2GraphicBlock;
    friend class ::android::C2BlockAllocatorImpl;
};

C2ConstGraphicBlock::C2ConstGraphicBlock(const std::shared_ptr<C2GraphicAllocation> &alloc,
    C2Fence fence):
        C2Block2D(alloc),
        mImpl(std::shared_ptr<Impl>(new Impl { alloc } )),
        mFence(fence) {}

class C2GraphicBlock::Impl
{
public:
    std::shared_ptr<C2GraphicAllocation> allocation_;
};

C2GraphicBlock::C2GraphicBlock(const std::shared_ptr<C2GraphicAllocation>& alloc):
    C2Block2D(alloc),
    mImpl(std::shared_ptr<Impl>(new Impl { alloc } )) {}

const C2Handle *C2Block2D::handle() const
{
    return mImpl->allocation_->handle();
};

class GraphicSwBuffer
{
private:
    std::vector<uint8_t> data_;
    uint32_t width_ {};
    uint32_t height_ {};

public:
    uint8_t* data() { return &data_.front(); }

    c2_status_t Alloc(uint32_t width, uint32_t height)
    {
        width_ = width;
        height_ = height;
        data_.resize(width_ * height_ * 3 / 2); // alloc for nv12 format
        return C2_OK;
    }

    void InitNV12PlaneLayout(C2PlanarLayout* plane_layout)
    {
        ::InitNV12PlaneLayout(width_, plane_layout);
    }

    void InitNV12PlaneData(uint8_t** plane_data)
    {
        ::InitNV12PlaneData(width_, height_, data(), plane_data);
    }
};

class C2GraphicAllocationMock : public C2GraphicAllocation
{
public:
    using C2GraphicAllocation::C2GraphicAllocation;
    friend class C2BlockAllocatorImpl;

public:
    std::atomic_bool locked_ { false };
    GraphicSwBuffer sw_buffer_;
    buffer_handle_t handle_ {};
    std::shared_ptr<MfxGrallocAllocator> gralloc_allocator_;

public:
    ~C2GraphicAllocationMock() {
        if (handle_ && gralloc_allocator_) gralloc_allocator_->Free(handle_);
    }

    virtual c2_status_t map(
            C2Rect/* rect*/, C2MemoryUsage/* usage*/, int */*fenceFd*/,
            // TODO: return <addr, size> buffers with plane sizes
            C2PlanarLayout *layout /* nonnull */, uint8_t **addr /* nonnull */) override
    {
        MFX_DEBUG_TRACE_FUNC;

        c2_status_t error = C2_OK;

        bool locked = locked_.exchange(true);
        if (locked) {
            error = C2_BAD_STATE;
        } else {
            if (nullptr == handle_) { // system memory block
                sw_buffer_.InitNV12PlaneLayout(layout);
                sw_buffer_.InitNV12PlaneData(addr);
            } else {
                MFX_DEBUG_TRACE_P(handle_);
                error = gralloc_allocator_->LockFrame(handle_, addr, layout);
            }
        }

        MFX_DEBUG_TRACE__android_c2_status_t(error);

        return error;
    }

    virtual c2_status_t unmap(C2Fence */*fenceFd*/ /* nullable */) override
    {
        if (handle_ && gralloc_allocator_) gralloc_allocator_->UnlockFrame(handle_);
        locked_.store(false);
        return C2_OK;
    }

    virtual bool isValid() const override { return true; }

    virtual const C2Handle *handle() const override { return handle_; }

    virtual bool equals(const std::shared_ptr<const C2GraphicAllocation> &/*other*/) const override
    {
        return false;
    }
};

class C2GraphicView::Impl
{
public:
    std::vector<uint8_t*> data_ {};
    C2PlanarLayout plane_layout_ {};
    c2_status_t error { C2_OK };
    // shared_ptr to prevent C2Block::Impl destruction before this destruction
    std::shared_ptr<C2GraphicAllocation> allocation_;
public:
    ~Impl() {
        allocation_->unmap(nullptr/*C2Fence *fenceFd*/);
    }
};

C2GraphicView::C2GraphicView(
            const _C2PlanarCapacityAspect *parent,
            uint8_t *const *data,
            const C2PlanarLayout& layout,
            const std::shared_ptr<C2GraphicAllocation> &alloc):
    _C2PlanarSection(parent),
    mImpl(std::shared_ptr<Impl>(
        new Impl { { data, data + layout.numPlanes }, layout, C2_OK, { alloc } } )) {}

C2GraphicView::C2GraphicView(c2_status_t error):
    _C2PlanarSection(nullptr),
    mImpl(std::shared_ptr<Impl>(new Impl { {}, {}, error, {} } )) {}

uint8_t *const *C2GraphicView::data()
{
    return &mImpl->data_.front();
}

const uint8_t *const *C2GraphicView::data() const
{
    return &mImpl->data_.front();
}

const C2PlanarLayout C2GraphicView::layout() const
{
    return mImpl->plane_layout_;
}

class C2GraphicViewMock : public C2GraphicView
{
    using C2GraphicView::C2GraphicView;
    friend class ::android::C2ConstGraphicBlock;
    friend class ::android::C2GraphicBlock;
};

class C2AcquirableConstGraphicView : public C2Acquirable<const C2GraphicView> {
    using C2Acquirable::C2Acquirable;
    friend class ::android::C2ConstGraphicBlock;
};

class C2AcquirableGraphicView : public C2Acquirable<C2GraphicView> {
    using C2Acquirable::C2Acquirable;
    friend class ::android::C2GraphicBlock;
};

class C2ConstGraphicBlockAccessor : public C2ConstGraphicBlock {
    using C2ConstGraphicBlock::C2ConstGraphicBlock;
    friend class ::android::C2GraphicBlock;
};

C2Acquirable<const C2GraphicView> C2ConstGraphicBlock::map() const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    std::vector<uint8_t*> data;
    data.resize(C2PlanarLayout::MAX_NUM_PLANES);
    C2PlanarLayout layout;

    error = mImpl->allocation_->map(C2Rect { 0, 0 }, {}, {}, &layout, &data.front());
    if (C2_OK == error) {
        data.resize(layout.numPlanes);
        C2GraphicViewMock graphic_view(this, &data.front(), layout, mImpl->allocation_);
        return C2AcquirableConstGraphicView(error, event.fence(), graphic_view);
    } else {
        C2GraphicViewMock graphic_view(error);
        return C2AcquirableConstGraphicView(error, event.fence(), graphic_view);
    }
}

C2Acquirable<C2GraphicView> C2GraphicBlock::map()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t error = C2_OK; // no error of mapping for now
    C2Event event;
    event.fire(); // map is always ready to read

    std::vector<uint8_t*> data;
    data.resize(C2PlanarLayout::MAX_NUM_PLANES);
    C2PlanarLayout layout;

    error = mImpl->allocation_->map(C2Rect { 0, 0 }, {}, {}, &layout, &data.front());
    if (C2_OK == error) {
        data.resize(layout.numPlanes);
        C2GraphicViewMock graphic_view(this, &data.front(), layout, mImpl->allocation_);
        return C2AcquirableGraphicView(error, event.fence(), graphic_view);
    } else {
        C2GraphicViewMock graphic_view(error);
        return C2AcquirableGraphicView(error, event.fence(), graphic_view);
    }
}

C2ConstGraphicBlock C2GraphicBlock::share(const C2Rect &crop, C2Fence fence)
{
    C2ConstGraphicBlockAccessor const_graphics_block(mImpl->allocation_, fence);
    const_graphics_block.setCrop(crop);

    return const_graphics_block;
}

class C2BufferDataAccessor : public C2BufferData
{
    using C2BufferData::C2BufferData;
    friend class C2Buffer;
};

class C2Buffer::Impl
{
public:
    C2BufferDataAccessor data_;
};

C2Buffer::C2Buffer(const std::vector<C2ConstLinearBlock> &blocks)
{
    mImpl = std::shared_ptr<C2Buffer::Impl>(
        new C2Buffer::Impl { C2BufferDataAccessor(blocks) } );
}

C2Buffer::C2Buffer(const std::vector<C2ConstGraphicBlock> &blocks)
{
    mImpl = std::shared_ptr<C2Buffer::Impl>(
        new C2Buffer::Impl { C2BufferDataAccessor(blocks) } );
}

const C2BufferData C2Buffer::data() const
{
    return mImpl->data_;
}

class C2BufferData::Impl
{
public:
    explicit Impl(const std::vector<C2ConstLinearBlock> &blocks)
        : type_(blocks.size() == 1 ? LINEAR : LINEAR_CHUNKS),
          linear_blocks(blocks) {
    }

    explicit Impl(const std::vector<C2ConstGraphicBlock> &blocks)
        : type_(blocks.size() == 1 ? GRAPHIC : GRAPHIC_CHUNKS),
          graphic_blocks(blocks) {
    }
public:
    Type type_;
    std::vector<C2ConstLinearBlock> linear_blocks;
    std::vector<C2ConstGraphicBlock> graphic_blocks;
};

const std::vector<C2ConstLinearBlock> C2BufferData::linearBlocks() const
{
    return mImpl->linear_blocks;
}

const std::vector<C2ConstGraphicBlock> C2BufferData::graphicBlocks() const
{
    return mImpl->graphic_blocks;
}

C2BufferData::Type C2BufferData::type() const
{
    return mImpl->type_;
}

C2BufferData::C2BufferData(const std::vector<C2ConstLinearBlock> &blocks) : mImpl(new Impl(blocks)) {}
C2BufferData::C2BufferData(const std::vector<C2ConstGraphicBlock> &blocks) : mImpl(new Impl(blocks)) {}

class C2BlockAllocatorImpl : public C2BlockPool
{
public:
    struct CreateResult
    {
        std::shared_ptr<C2BlockPool> allocator;
        c2_status_t status;
    };

public:
    static CreateResult Create();

    ~C2BlockAllocatorImpl() = default;

public:
    c2_status_t Map(const C2GraphicBlock* graphic_block, uint8_t** data);

private:
    C2BlockAllocatorImpl() = default;

    c2_status_t Init();

    MFX_CLASS_NO_COPY(C2BlockAllocatorImpl)

private: // C2BlockPool impl
    virtual local_id_t getLocalId() const override
    {
        return 0;
    }

    virtual C2Allocator::id_t getAllocatorId() const override
    {
        return 0;
    }

    virtual c2_status_t fetchLinearBlock(
            uint32_t capacity, C2MemoryUsage usage __unused,
            std::shared_ptr<C2LinearBlock> *block /* nonnull */) override;

    virtual c2_status_t fetchGraphicBlock(
            uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
            C2MemoryUsage usage __unused,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) override;

private:
    std::shared_ptr<MfxGrallocAllocator> gralloc_allocator_;
};

c2_status_t C2BlockAllocatorImpl::Init()
{
    std::unique_ptr<MfxGrallocAllocator> allocator;
    c2_status_t res = MfxGrallocAllocator::Create(&allocator);
    if (res == C2_OK) {
        gralloc_allocator_ = std::shared_ptr<MfxGrallocAllocator>(std::move(allocator));
    }
    return res;
}

C2BlockAllocatorImpl::CreateResult C2BlockAllocatorImpl::Create()
{
    CreateResult res {};
    res.status = C2_OK;

    C2BlockAllocatorImpl* alloc = new (std::nothrow)C2BlockAllocatorImpl();
    if (alloc) {
        res.status = alloc->Init();
        if (res.status == C2_OK) res.allocator.reset(alloc);
    } else {
        res.status = C2_NO_MEMORY;
    }
    return res;
}

c2_status_t C2BlockAllocatorImpl::fetchLinearBlock(
        uint32_t capacity, C2MemoryUsage usage __unused,
        std::shared_ptr<C2LinearBlock> *block /* nonnull */) {

    c2_status_t res = C2_OK;
    try {
        std::shared_ptr<C2LinearAllocation> allocation(new C2LinearAllocationMock(capacity));

        *block = std::shared_ptr<C2LinearBlock>(new C2LinearBlockAccessor(allocation));
    }
    catch(const std::bad_alloc&) {
        res = C2_NO_MEMORY;
    }
    return res;
}

c2_status_t C2BlockAllocatorImpl::fetchGraphicBlock(
        uint32_t width __unused, uint32_t height __unused, uint32_t format __unused,
        C2MemoryUsage usage __unused,
        std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {

    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    C2GraphicAllocationMock* allocation {};

    try {
        if ((usage.consumer & C2MemoryUsage::HW_CODEC_READ) ||
            (usage.producer & C2MemoryUsage::HW_CODEC_WRITE)) {

            buffer_handle_t handle {};

            res = gralloc_allocator_->Alloc(width, height, &handle);

            if (C2_OK == res) {
                MFX_DEBUG_TRACE_P(handle);

                allocation = new C2GraphicAllocationMock(width, height);
                allocation->handle_ = handle;
                allocation->gralloc_allocator_ = gralloc_allocator_;
            }
        } else {
            if (format == HAL_PIXEL_FORMAT_NV12_TILED_INTEL) {
                allocation = new C2GraphicAllocationMock(width, height);
                allocation->sw_buffer_.Alloc(width, height);
            } else {
                res = C2_CANNOT_DO;
            }
        }

        if (C2_OK == res) {
            *block = std::shared_ptr<C2GraphicBlock>(
                new C2GraphicBlockAccessor(std::shared_ptr<C2GraphicAllocation>(allocation)));
        }
    } catch(const std::bad_alloc&) {
        res = C2_NO_MEMORY;
    }
    return res;
}

c2_status_t GetCodec2BlockPool(
        C2BlockPool::local_id_t id, std::shared_ptr<const C2Component> component,
        std::shared_ptr<C2BlockPool> *pool) {

    (void)component;

    pool->reset();

    c2_status_t res = C2_OK;

    switch (id) {
        case C2BlockPool::BASIC_LINEAR:
        case C2BlockPool::BASIC_GRAPHIC:
            static C2BlockAllocatorImpl::CreateResult g_create_result =
                C2BlockAllocatorImpl::Create();

            *pool = g_create_result.allocator;
            res = g_create_result.status;

            break;
        default:
            res = C2_NOT_FOUND;
            break;
    }
    return res;
}

} //namespace android;
