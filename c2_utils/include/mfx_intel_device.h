// Copyright (c) 2024 Intel Corporation
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

#include <stdint.h>

enum {
	GPU_GRP_TYPE_INTEL_IGPU_IDX = 0,
	GPU_GRP_TYPE_INTEL_DGPU_IDX = 1,
	GPU_GRP_TYPE_VIRTIO_GPU_BLOB_IDX = 2,
	// virtio-GPU with allow-p2p feature, implying its display is backed by dGPU
	GPU_GRP_TYPE_VIRTIO_GPU_BLOB_P2P_IDX = 3,
	GPU_GRP_TYPE_VIRTIO_GPU_NO_BLOB_IDX = 4,
	GPU_GRP_TYPE_VIRTIO_GPU_IVSHMEM_IDX = 5,
	GPU_GRP_TYPE_NR,
};

#define GPU_GRP_TYPE_HAS_INTEL_IGPU_BIT			(1ull << GPU_GRP_TYPE_INTEL_IGPU_IDX)
#define GPU_GRP_TYPE_HAS_INTEL_DGPU_BIT			(1ull << GPU_GRP_TYPE_INTEL_DGPU_IDX)
#define GPU_GRP_TYPE_HAS_VIRTIO_GPU_BLOB_BIT		(1ull << GPU_GRP_TYPE_VIRTIO_GPU_BLOB_IDX)
#define GPU_GRP_TYPE_HAS_VIRTIO_GPU_BLOB_P2P_BIT	(1ull << GPU_GRP_TYPE_VIRTIO_GPU_BLOB_P2P_IDX)
#define GPU_GRP_TYPE_HAS_VIRTIO_GPU_NO_BLOB_BIT		(1ull << GPU_GRP_TYPE_VIRTIO_GPU_NO_BLOB_IDX)
#define GPU_GRP_TYPE_HAS_VIRTIO_GPU_IVSHMEM_BIT		(1ull << GPU_GRP_TYPE_VIRTIO_GPU_IVSHMEM_IDX)

#define DRIVER_DEVICE_FEATURE_I915_DGPU			(1ull << 1)
#define DRIVER_DEVICE_FEATURE_VIRGL_RESOURCE_BLOB	(1ull << 2)
#define DRIVER_DEVICE_FEATURE_VIRGL_QUERY_DEV		(1ull << 3)
#define DRIVER_DEVICE_FEATURE_VIRGL_ALLOW_P2P		(1ull << 4)

bool isIntelDg2(int fd);
bool isVirtioGpuAllowP2p(int virtgpu_fd);
bool isVirtioGpuPciDevice(int virtgpu_fd);
bool isVirtioGpuWithBlob(int virtgpu_fd);

uint64_t getGpuGroupType();
bool enforceLinearBuffer();
