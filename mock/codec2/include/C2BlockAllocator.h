#pragma once

#include <C2Buffer.h>

android::c2_status_t GetC2BlockAllocator(std::shared_ptr<android::C2BlockPool>* allocator);
