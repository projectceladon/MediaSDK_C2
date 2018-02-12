#pragma once

#include <C2Buffer.h>
#include <C2Component.h>

namespace android {

c2_status_t GetCodec2BlockPool(
        C2BlockPool::local_id_t id, std::shared_ptr<const C2Component> component,
        std::shared_ptr<C2BlockPool> *pool);

} // namespace android
