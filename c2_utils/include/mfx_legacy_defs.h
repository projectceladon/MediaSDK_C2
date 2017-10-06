/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

// This file contains definitions copied from OMX headers

#pragma once

/**
 * AVC profile types, each profile indicates support for various
 * performance bounds and different annexes.
 */
typedef enum LEGACY_VIDEO_AVCPROFILETYPE {
    LEGACY_VIDEO_AVCProfileBaseline = 0x01,   /**< Baseline profile */
    LEGACY_VIDEO_AVCProfileMain     = 0x02,   /**< Main profile */
    LEGACY_VIDEO_AVCProfileExtended = 0x04,   /**< Extended profile */
    LEGACY_VIDEO_AVCProfileHigh     = 0x08,   /**< High profile */
    LEGACY_VIDEO_AVCProfileHigh10   = 0x10,   /**< High 10 profile */
    LEGACY_VIDEO_AVCProfileHigh422  = 0x20,   /**< High 4:2:2 profile */
    LEGACY_VIDEO_AVCProfileHigh444  = 0x40,   /**< High 4:4:4 profile */
    LEGACY_VIDEO_AVCProfileKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    LEGACY_VIDEO_AVCProfileVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    LEGACY_VIDEO_AVCProfileMax      = 0x7FFFFFFF
} LEGACY_VIDEO_AVCPROFILETYPE;


/**
 * AVC level types, each level indicates support for various frame sizes,
 * bit rates, decoder frame rates.  No need
 */
typedef enum LEGACY_VIDEO_AVCLEVELTYPE {
    LEGACY_VIDEO_AVCLevel1   = 0x01,     /**< Level 1 */
    LEGACY_VIDEO_AVCLevel1b  = 0x02,     /**< Level 1b */
    LEGACY_VIDEO_AVCLevel11  = 0x04,     /**< Level 1.1 */
    LEGACY_VIDEO_AVCLevel12  = 0x08,     /**< Level 1.2 */
    LEGACY_VIDEO_AVCLevel13  = 0x10,     /**< Level 1.3 */
    LEGACY_VIDEO_AVCLevel2   = 0x20,     /**< Level 2 */
    LEGACY_VIDEO_AVCLevel21  = 0x40,     /**< Level 2.1 */
    LEGACY_VIDEO_AVCLevel22  = 0x80,     /**< Level 2.2 */
    LEGACY_VIDEO_AVCLevel3   = 0x100,    /**< Level 3 */
    LEGACY_VIDEO_AVCLevel31  = 0x200,    /**< Level 3.1 */
    LEGACY_VIDEO_AVCLevel32  = 0x400,    /**< Level 3.2 */
    LEGACY_VIDEO_AVCLevel4   = 0x800,    /**< Level 4 */
    LEGACY_VIDEO_AVCLevel41  = 0x1000,   /**< Level 4.1 */
    LEGACY_VIDEO_AVCLevel42  = 0x2000,   /**< Level 4.2 */
    LEGACY_VIDEO_AVCLevel5   = 0x4000,   /**< Level 5 */
    LEGACY_VIDEO_AVCLevel51  = 0x8000,   /**< Level 5.1 */
    LEGACY_VIDEO_AVCLevel52  = 0x10000,  /**< Level 5.2 */
    LEGACY_VIDEO_AVCLevelKhronosExtensions = 0x6F000000, /**< Reserved region for introducing Khronos Standard Extensions */
    LEGACY_VIDEO_AVCLevelVendorStartUnused = 0x7F000000, /**< Reserved region for introducing Vendor Extensions */
    LEGACY_VIDEO_AVCLevelMax = 0x7FFFFFFF
} LEGACY_VIDEO_AVCLEVELTYPE;

