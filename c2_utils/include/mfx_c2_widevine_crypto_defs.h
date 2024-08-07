// Copyright (c) 2017-2024 Intel Corporation
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

#define SEC_VIDEO_BUFFER_SIZE       (3*1024*1024)
#define WV_AES_IV_SIZE 16
#define DRM_TYPE_CLASSIC_WV 0x0
#define DRM_TYPE_MDRM 0x1
#define PROTECTED_DATA_BUFFER_MAGIC   (0UL | ('E' << 24) | ('B' << 16) | ('D' << 8) | 'P')

#define INPORT_BUFFER_SIZE 1650000
typedef struct {
    uint32_t offset;
    uint32_t size;
} sec_partition_t;
typedef struct {
    sec_partition_t src;
    sec_partition_t dest;
    sec_partition_t metadata;
    sec_partition_t headers;
} video_partition_t;
typedef struct {
    uint8_t config[64];
    uint8_t config_len;
    uint32_t config_frame_offset;
    uint8_t key_id[16];
    uint32_t key_id_len;
    uint8_t session_id;
} mdrm_meta;
typedef struct {
    uint32_t size;
    uint32_t base_offset;
    video_partition_t partitions;
    uint32_t frame_size;
    uint32_t src_fill;
    uint8_t pes_packet_count;
    uint8_t clear;
    mdrm_meta mdrm_info;
    uint8_t drm_type; //0 -> Classic, 1 -> MDRM
    uint8_t iv[WV_AES_IV_SIZE];

    uint32_t first_sub_sample;               //used by liboemcrypto
    uint32_t configdata_multiple_sub_sample; //used by liboemcrypto
    uint32_t sps_pps_sample;                 //used by liboemcrypto
    uint32_t nf_stream;                      //used by liboemcrypto
    uint32_t meta_offset;                    //used by liboemcrypto
    uint32_t parsed_data_size               = 0;
    uint32_t m_PAVPAppID;
    bool skipFailedFrame = false;
    bool waitForIDR = false;
    uint8_t  m_config_nalu[1024];
    uint32_t m_config_nalu_len = 0;
    uint8_t base[SEC_VIDEO_BUFFER_SIZE];
} SECVideoBuffer;

#define MAX_SUPPORTED_PACKETS 50                    // Can be scaled based on the need
#define PAVP_APPID_INVALID 0xffff                   // OMX to validate against this AppID

// Below structure is inline as per libVA doc and is applicable only for encrypted packets (both Classic as well as MDRM)
typedef struct {
    uint32_t uiSegmentStartOffset;
    uint32_t uiSegmentLength;
    uint32_t uiPartialAesBlockSizeInBytes;
    uint32_t uiInitByteLength;
    uint8_t  uiAesIV[WV_AES_IV_SIZE];
} segment_info;
typedef struct {
    bool     clear;                                 // true - clear, false - enc (in which case segmentinfo struct would be populated)
    uint32_t clearPacketSize;                       // Applicable only for clear packet
    uint32_t clearPacketOffset;                     // Populated only for clear packet, offset within SECVideoBuffer->base ptr. For encrypted packet refer to segment_info struct
    uint8_t  configData;                            // Currently:  0 - No Config data(actual clear frame), 1 - Config data (SPS/PPS), will be enum
    segment_info sSegmentData;
} packet_info;
typedef struct {
    uint32_t appID;                                 // OMX to use this AppID
    uint32_t frame_size;                            // Total frame length filled by Oemcrypto into base ptr, Should be <= INPORT_BUFFER_SIZE
    uint8_t  uiNumPackets;                          // Subsample for MDRM and PES packets for Classic, Should be <= MAX_SUPPORTED_PACKETS (i.e., 20)
    packet_info sPacketData[MAX_SUPPORTED_PACKETS]; // Packet details for max supported packets
    uint8_t  drm_type;                              // 0 - Classic, 1 - MDRM
    uint8_t  ucSizeOfLength;                        // For MDRM purpose, This would be '0' as we use only start code based NALU (as discussed in our call).
    uint32_t uiCurrSegPartialAesBlockSizeInBytes;   // Internal Member to calc Partial Aes block size required in case of MDRM enc Nalus
    uint32_t uiEncDataSize;                         // Internal Member to calc whole Aes blocks for IV updation in case of MDRM enc nalus
    uint8_t base[INPORT_BUFFER_SIZE];               // Input buffer allocated by OMX, Oemcrypto fills it and shared it for processing
} HUCVideoBuffer;


typedef struct {
    uint8_t          iv[WV_AES_IV_SIZE];
    uint32_t         mode;
    uint32_t         app_id;
} pr_pavp_info_t;
typedef struct __DRM_SubSamples {
    uint32_t         numClearBytes;
    uint32_t         numEncryptedBytes;
} DRM_SubSamples;
typedef struct {
    uint32_t         pr_magic;  //PR_MAGIC
    pr_pavp_info_t   pavp_info;
    uint32_t         pavp_frame_data_size;
    uint32_t         nalu_info_data_size;
    uint32_t         num_nalus;
    uint32_t         isSPSPPSAvailable;
    uint32_t         total_data_size;
    uint32_t         total_encr_bytes;
    uint32_t         sps_length;
    uint32_t         pps_length;
    size_t           numSubSamples;
    DRM_SubSamples   pVideoSubSamples[16];
    uint8_t*         pVideoDecryptContext;
    uint64_t         pr_iv;
    uint8_t          databuffer[INPORT_BUFFER_SIZE];
} pr_metadata_buffer;

typedef struct {
    uint32_t magic;
    uint32_t index;
    union{
        HUCVideoBuffer hucBuffer;                  // Allocated in Oemcrypto based on #input buffers and shared to OMX
        SECVideoBuffer secBuffer;                  // same
    };
    pr_metadata_buffer pr_data;     //This data struct is defined to hold whole pavp encrypted frame data.
} C2SecureBuffer;
