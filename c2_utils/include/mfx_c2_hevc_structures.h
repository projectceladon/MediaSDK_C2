// Copyright (c) 2012-2021 Intel Corporation
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

#ifndef __MFX_C2_HEVC_STRUCTURES_H__
#define __MFX_C2_HEVC_STRUCTURES_H__

#include "mfx_c2_avc_structures.h"

namespace HEVCParser
{

#define MFX_INTERNAL_CPY(dst, src, size) std::copy((const uint8_t *)(src), (const uint8_t *)(src) + (int)(size), (uint8_t *)(dst))

#define H265_FORCEINLINE __attribute__((always_inline))
#define H265_NONLINE __attribute__((noinline))

enum
{
    H265_PROFILE_MAIN   = 1,
    H265_PROFILE_MAIN10 = 2,
    H265_PROFILE_MAINSP = 3,
    H265_PROFILE_FREXT  = 4,
    H265_PROFILE_SCC    = 9,
};

enum {
    AMVP_MAX_NUM_CAND       = 2,           // max number of final candidates
    MERGE_MAX_NUM_CAND      = 5,
    MAX_CHROMA_OFFSET_ELEMENTS = 7
};

#define MAX_CPB_CNT                     32  ///< Upper bound of (cpb_cnt_minus1 + 1)

enum
{
    MAX_TEMPORAL_LAYER  = 8,

    MAX_VPS_NUM_LAYER_SETS  = 1024,
    MAX_NUH_LAYER_ID        = 1,           // currently max nuh_layer_id value is 1

    MAX_CU_DEPTH = 6,
    MAX_CU_SIZE = (1 << MAX_CU_DEPTH), // maximum allowable size of CU
    MIN_PU_SIZE = 4,
    MAX_NUM_PU_IN_ROW = (MAX_CU_SIZE/MIN_PU_SIZE)
};

enum NalUnitType
{
    NAL_UT_CODED_SLICE_TRAIL_N = 0,
    NAL_UT_CODED_SLICE_TRAIL_R = 1,
    NAL_UT_CODED_SLICE_TSA_N   = 2,
    NAL_UT_CODED_SLICE_TSA_R   = 3,
    NAL_UT_CODED_SLICE_STSA_N  = 4,
    NAL_UT_CODED_SLICE_STSA_R  = 5,
    NAL_UT_CODED_SLICE_RADL_N  = 6,
    NAL_UT_CODED_SLICE_RADL_R  = 7,
    NAL_UT_CODED_SLICE_RASL_N  = 8,
    NAL_UT_CODED_SLICE_RASL_R  = 9,

    NAL_RSV_VCL_N10 = 10,
    NAL_RSV_VCL_R11 = 11,
    NAL_RSV_VCL_N12 = 12,
    NAL_RSV_VCL_R13 = 13,
    NAL_RSV_VCL_N14 = 14,
    NAL_RSV_VCL_R15 = 15,

    NAL_UT_CODED_SLICE_BLA_W_LP   = 16,
    NAL_UT_CODED_SLICE_BLA_W_RADL = 17,
    NAL_UT_CODED_SLICE_BLA_N_LP   = 18,
    NAL_UT_CODED_SLICE_IDR_W_RADL = 19,
    NAL_UT_CODED_SLICE_IDR_N_LP   = 20,
    NAL_UT_CODED_SLICE_CRA        = 21,

    NAL_RSV_IRAP_VCL22 = 22,
    NAL_RSV_IRAP_VCL23 = 23,

    NAL_RSV_VCL_24 = 24,
    NAL_RSV_VCL_25 = 25,
    NAL_RSV_VCL_26 = 26,
    NAL_RSV_VCL_27 = 27,
    NAL_RSV_VCL_28 = 28,
    NAL_RSV_VCL_29 = 29,
    NAL_RSV_VCL_30 = 30,
    NAL_RSV_VCL_31 = 31,

    NAL_UT_VPS          = 32,
    NAL_UT_SPS          = 33,
    NAL_UT_PPS          = 34,
    NAL_UT_AU_DELIMITER = 35,
    NAL_UT_EOS          = 36,
    NAL_UT_EOB          = 37,
    NAL_UT_FILLER_DATA  = 38,
    NAL_UT_SEI          = 39,
    NAL_UT_SEI_SUFFIX   = 40,

    NAL_RSV_NVCL_41 = 41,
    NAL_RSV_NVCL_42 = 42,
    NAL_RSV_NVCL_43 = 43,
    NAL_RSV_NVCL_44 = 44,
    NAL_RSV_NVCL_45 = 45,
    NAL_RSV_NVCL_46 = 46,
    NAL_RSV_NVCL_47 = 47,

    NAL_UT_INVALID = 64,
};

// Slice types
enum SliceType
{
  B_SLICE,
  P_SLICE,
  I_SLICE
};

#define SCALING_LIST_NUM 6         ///< list number for quantization matrix
#define SCALING_LIST_NUM_32x32 2   ///< list number for quantization matrix 32x32
#define SCALING_LIST_REM_NUM 6     ///< remainder of QP/6
#define SCALING_LIST_START_VALUE 8 ///< start value for dpcm mode
#define MAX_MATRIX_COEF_NUM 64     ///< max coefficient number for quantization matrix
#define MAX_MATRIX_SIZE_NUM 8      ///< max size number for quantization matrix
#define SCALING_LIST_DC 16         ///< default DC value

enum ScalingListSize
{
    SCALING_LIST_4x4 = 0,
    SCALING_LIST_8x8,
    SCALING_LIST_16x16,
    SCALING_LIST_32x32,
    SCALING_LIST_SIZE_NUM
};

enum
{
    MAX_NUM_VPS_PARAM_SETS_H265 = 16,
    MAX_NUM_SEQ_PARAM_SETS_H265 = 16,
    MAX_NUM_PIC_PARAM_SETS_H265 = 64,

    MAX_NUM_REF_PICS            = 16,
    MAX_NUM_REF_PICS_CUR        = 8,

    COEFFICIENTS_BUFFER_SIZE_H265    = 16 * 51,

    MINIMAL_DATA_SIZE_H265           = 4,

    DEFAULT_NU_TAIL_VALUE       = 0xff,
    DEFAULT_NU_TAIL_SIZE        = 8
};

// Scaling list initialization scan lookup table
const uint16_t g_sigLastScanCG32x32[64] =
{
    0, 8, 1, 16, 9, 2, 24, 17,
    10, 3, 32, 25, 18, 11, 4, 40,
    33, 26, 19, 12, 5, 48, 41, 34,
    27, 20, 13, 6, 56, 49, 42, 35,
    28, 21, 14, 7, 57, 50, 43, 36,
    29, 22, 15, 58, 51, 44, 37, 30,
    23, 59, 52, 45, 38, 31, 60, 53,
    46, 39, 61, 54, 47, 62, 55, 63
};

// Scaling list initialization scan lookup table
const uint16_t ScanTableDiag4x4[16] =
{
    0, 4, 1, 8,
    5, 2, 12, 9,
    6, 3, 13, 10,
    7, 14, 11, 15
};

// Default scaling list 4x4
const int32_t g_quantTSDefault4x4[16] =
{
  16,16,16,16,
  16,16,16,16,
  16,16,16,16,
  16,16,16,16
};

// Default scaling list 8x8 for intra prediction
const int32_t g_quantIntraDefault8x8[64] =
{
  16,16,16,16,17,18,21,24,
  16,16,16,16,17,19,22,25,
  16,16,17,18,20,22,25,29,
  16,16,18,21,24,27,31,36,
  17,17,20,24,30,35,41,47,
  18,19,22,27,35,44,54,65,
  21,22,25,31,41,54,70,88,
  24,25,29,36,47,65,88,115
};

// Default scaling list 8x8 for inter prediction
const int32_t g_quantInterDefault8x8[64] =
{
  16,16,16,16,17,18,20,24,
  16,16,16,17,18,20,24,25,
  16,16,17,18,20,24,25,28,
  16,17,18,20,24,25,28,33,
  17,18,20,24,25,28,33,41,
  18,20,24,25,28,33,41,54,
  20,24,25,28,33,41,54,71,
  24,25,28,33,41,54,71,91
};

// Scaling list table sizes
const uint32_t g_scalingListSize [4] = {16, 64, 256, 1024};
// Number of possible scaling lists of different sizes
const uint32_t g_scalingListNum[SCALING_LIST_SIZE_NUM]={6, 6, 6, 2};

// Sample aspect ratios by aspect_ratio_idc index. HEVC spec E.3.1
const uint16_t SAspectRatio[17][2] =
{
    { 0,  0}, { 1,  1}, {12, 11}, {10, 11}, {16, 11}, {40, 33}, { 24, 11},
    {20, 11}, {32, 11}, {80, 33}, {18, 11}, {15, 11}, {64, 33}, {160, 99},
    {4,   3}, {3,   2}, {2,   1}
};

// Scaling list data structure
class H265ScalingList
{
MFX_CLASS_NO_COPY(H265ScalingList)
public:
    H265ScalingList() { m_bInitialized = false; }
    ~H265ScalingList()
    {
        if (m_bInitialized)
            destroy();
    }

    int*      getScalingListAddress   (unsigned sizeId, unsigned listId)          { return m_nScalingListCoef[sizeId][listId]; }
    const int* getScalingListAddress  (unsigned sizeId, unsigned listId) const    { return m_nScalingListCoef[sizeId][listId]; }
    void     setRefMatrixId           (unsigned sizeId, unsigned listId, unsigned u)   { m_uRefMatrixId[sizeId][listId] = u; }
    unsigned getRefMatrixId           (unsigned sizeId, unsigned listId)           { return m_uRefMatrixId[sizeId][listId]; }
    void     setScalingListDC         (unsigned sizeId, unsigned listId, unsigned u)   { m_nScalingListDC[sizeId][listId] = u; }
    // Copy data from predefined scaling matrixes
    void     processRefMatrix(unsigned sizeId, unsigned listId, unsigned refListId)
    {
        MFX_INTERNAL_CPY(getScalingListAddress(sizeId, listId),
            ((listId == refListId) ? getScalingListDefaultAddress(sizeId, refListId) : getScalingListAddress(sizeId, refListId)),
            sizeof(int)*MSDK_MIN(MAX_MATRIX_COEF_NUM, (int)g_scalingListSize[sizeId]));
    }
    int      getScalingListDC         (unsigned sizeId, unsigned listId) const     { return m_nScalingListDC[sizeId][listId]; }

    // Allocate and initialize scaling list tables
    void init();
    bool is_initialized(void) { return m_bInitialized; }
    // Initialize scaling list with default data
    void initFromDefaultScalingList(void);
    // Calculated coefficients used for dequantization
    void calculateDequantCoef(void);

    // Deallocate scaling list tables
    void destroy()
    {
        if (!m_bInitialized)
            return;

        for (uint32_t sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
        {
            delete [] m_dequantCoef[sizeId][0][0];
            m_dequantCoef[sizeId][0][0] = 0;
        }

        m_bInitialized = false;
    }

    int16_t *m_dequantCoef[SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][SCALING_LIST_REM_NUM];

private:
    H265_FORCEINLINE int16_t* getDequantCoeff(uint32_t list, uint32_t qp, uint32_t size)
    {
        return m_dequantCoef[size][list][qp];
    }
    // Calculated coefficients used for dequantization in one scaling list matrix
    __inline void processScalingListDec(int32_t *coeff, int16_t *dequantcoeff, int32_t invQuantScales, uint32_t height, uint32_t width, uint32_t ratio, uint32_t sizuNum, uint32_t dc);
    // Returns default scaling matrix for specified parameters
    static const int *getScalingListDefaultAddress(unsigned sizeId, unsigned listId)
    {
        const int *src = 0;
        switch(sizeId)
        {
        case SCALING_LIST_4x4:
            src = g_quantTSDefault4x4;
            break;
        case SCALING_LIST_8x8:
            src = (listId<3) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
            break;
        case SCALING_LIST_16x16:
            src = (listId<3) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
            break;
        case SCALING_LIST_32x32:
            src = (listId<1) ? g_quantIntraDefault8x8 : g_quantInterDefault8x8;
            break;
        default:
            SAMPLE_ASSERT(0);
            src = NULL;
            break;
        }
        return src;
    }

    int      m_nScalingListDC               [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];
    unsigned m_uRefMatrixId                 [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM];
    int      m_nScalingListCoef             [SCALING_LIST_SIZE_NUM][SCALING_LIST_NUM][MAX_MATRIX_COEF_NUM];
    bool     m_bInitialized;
};

// One profile, tier, level data structure
struct H265PTL
{
    uint32_t      profile_space;
    uint8_t       tier_flag;
    uint32_t      profile_idc;
    uint32_t      profile_compatibility_flags;    // bitfield, 32 flags
    uint8_t       progressive_source_flag;
    uint8_t       interlaced_source_flag;
    uint8_t       non_packed_constraint_flag;
    uint8_t       frame_only_constraint_flag;
    uint32_t      level_idc;

    uint8_t       max_12bit_constraint_flag;
    uint8_t       max_10bit_constraint_flag;
    uint8_t       max_8bit_constraint_flag;
    uint8_t       max_422chroma_constraint_flag;
    uint8_t       max_420chroma_constraint_flag;
    uint8_t       max_monochrome_constraint_flag;
    uint8_t       intra_constraint_flag;
    uint8_t       one_picture_only_constraint_flag;
    uint8_t       lower_bit_rate_constraint_flag;
    uint8_t       max_14bit_constraint_flag;

    H265PTL()   { memset(this, 0, sizeof(*this)); }
};

// Stream profile, tiler, level data structure
#define H265_MAX_SUBLAYER_PTL   6
struct H265ProfileTierLevel
{
    H265PTL generalPTL;
    H265PTL subLayerPTL[H265_MAX_SUBLAYER_PTL];
    uint32_t  sub_layer_profile_present_flags;       // bitfield [0:H265_MAX_SUBLAYER_PTL]
    uint32_t  sub_layer_level_present_flag;          // bitfield [0:H265_MAX_SUBLAYER_PTL]

    H265ProfileTierLevel()
        : sub_layer_profile_present_flags(0)
        , sub_layer_level_present_flag(0)
    {
    }

    const H265PTL* GetGeneralPTL() const        { return &generalPTL; }
    const H265PTL* GetSubLayerPTL(int32_t i) const  { return &subLayerPTL[i]; }

    H265PTL* GetGeneralPTL()       { return &generalPTL; }
    H265PTL* GetSubLayerPTL(int32_t i) { return &subLayerPTL[i]; }
};

// HRD information data structure
struct H265HrdSubLayerInfo
{
    uint8_t       fixed_pic_rate_general_flag;
    uint8_t       fixed_pic_rate_within_cvs_flag;
    uint32_t      elemental_duration_in_tc;
    uint8_t       low_delay_hrd_flag;
    uint32_t      cpb_cnt;

    // sub layer hrd params
    uint32_t      bit_rate_value[MAX_CPB_CNT][2];
    uint32_t      cpb_size_value[MAX_CPB_CNT][2];
    uint32_t      cpb_size_du_value[MAX_CPB_CNT][2];
    uint32_t      bit_rate_du_value[MAX_CPB_CNT][2];
    uint8_t       cbr_flag[MAX_CPB_CNT][2];
};

// HRD VUI information
struct H265HRD
{
    uint8_t       nal_hrd_parameters_present_flag;
    uint8_t       vcl_hrd_parameters_present_flag;
    uint8_t       sub_pic_hrd_params_present_flag;

    // sub_pic_hrd_params_present_flag
    uint32_t      tick_divisor;
    uint32_t      du_cpb_removal_delay_increment_length;
    uint8_t       sub_pic_cpb_params_in_pic_timing_sei_flag;
    uint32_t      dpb_output_delay_du_length;

    uint32_t      bit_rate_scale;
    uint32_t      cpb_size_scale;
    uint32_t      cpb_size_du_scale;
    uint32_t      initial_cpb_removal_delay_length;
    uint32_t      au_cpb_removal_delay_length;
    uint32_t      dpb_output_delay_length;

    H265HrdSubLayerInfo m_HRD[MAX_TEMPORAL_LAYER];

    H265HRD()
    {
        ::memset(this, 0, sizeof(*this));
    }

    H265HrdSubLayerInfo * GetHRDSubLayerParam(uint32_t i) { return &m_HRD[i]; }
};

// VUI timing information
struct H265TimingInfo
{
    uint8_t   vps_timing_info_present_flag;
    uint32_t  vps_num_units_in_tick;
    uint32_t  vps_time_scale;
    uint8_t   vps_poc_proportional_to_timing_flag;
    int32_t  vps_num_ticks_poc_diff_one;

public:
    H265TimingInfo()
        : vps_timing_info_present_flag(false)
        , vps_num_units_in_tick(1000)
        , vps_time_scale(30000)
        , vps_poc_proportional_to_timing_flag(false)
        , vps_num_ticks_poc_diff_one(0)
    {}
};

// RPS data structure
struct ReferencePictureSet
{
    uint8_t inter_ref_pic_set_prediction_flag;

    uint32_t num_negative_pics;
    uint32_t num_positive_pics;

    uint32_t num_pics;

    uint32_t num_lt_pics;

    uint32_t num_long_term_pics;
    uint32_t num_long_term_sps;

    int32_t m_DeltaPOC[MAX_NUM_REF_PICS];
    int32_t m_POC[MAX_NUM_REF_PICS];
    uint8_t used_by_curr_pic_flag[MAX_NUM_REF_PICS];

    uint8_t delta_poc_msb_present_flag[MAX_NUM_REF_PICS];
    uint8_t delta_poc_msb_cycle_lt[MAX_NUM_REF_PICS];
    int32_t poc_lbs_lt[MAX_NUM_REF_PICS];

    ReferencePictureSet()
    {
        ::memset(this, 0, sizeof(*this));
    }

    void sortDeltaPOC()
    {
        for (uint32_t j = 1; j < num_pics; j++)
        {
            int32_t deltaPOC = m_DeltaPOC[j];
            uint8_t Used = used_by_curr_pic_flag[j];
            for (int32_t k = j - 1; k >= 0; k--)
            {
                int32_t temp = m_DeltaPOC[k];
                if (deltaPOC < temp)
                {
                    m_DeltaPOC[k + 1] = temp;
                    used_by_curr_pic_flag[k + 1] = used_by_curr_pic_flag[k];
                    m_DeltaPOC[k] = deltaPOC;
                    used_by_curr_pic_flag[k] = Used;
                }
            }
        }
        int32_t NumNegPics = (int32_t) num_negative_pics;
        for (int32_t j = 0, k = NumNegPics - 1; j < NumNegPics >> 1; j++, k--)
        {
            int32_t deltaPOC = m_DeltaPOC[j];
            uint8_t Used = used_by_curr_pic_flag[j];
            m_DeltaPOC[j] = m_DeltaPOC[k];
            used_by_curr_pic_flag[j] = used_by_curr_pic_flag[k];
            m_DeltaPOC[k] = deltaPOC;
            used_by_curr_pic_flag[k] = Used;
        }
    }

    void setInterRPSPrediction(bool f)      { inter_ref_pic_set_prediction_flag = f; }
    uint32_t getNumberOfPictures() const    { return num_pics; }
    uint32_t getNumberOfNegativePictures() const    { return num_negative_pics; }
    uint32_t getNumberOfPositivePictures() const    { return num_positive_pics; }
    uint32_t getNumberOfLongtermPictures() const    { return num_lt_pics; }
    void setNumberOfLongtermPictures(uint32_t val)  { num_lt_pics = val; }
    int getDeltaPOC(int index) const        { return m_DeltaPOC[index]; }
    void setDeltaPOC(int index, int val)    { m_DeltaPOC[index] = val; }
    uint8_t getUsed(int index) const           { return used_by_curr_pic_flag[index]; }

    void setPOC(int bufferNum, int POC)     { m_POC[bufferNum] = POC; }
    int getPOC(int index) const             { return m_POC[index]; }

    uint8_t getCheckLTMSBPresent(int32_t bufferNum) const { return delta_poc_msb_present_flag[bufferNum]; }

    uint32_t getNumberOfUsedPictures() const;
};

// Reference picture list data structure
struct ReferencePictureSetList
{
    unsigned m_NumberOfReferencePictureSets;

    ReferencePictureSetList()
    : m_NumberOfReferencePictureSets(0)
    , referencePictureSet{}
    {
    }

    void allocate(unsigned NumberOfReferencePictureSets)
    {
        if (m_NumberOfReferencePictureSets == NumberOfReferencePictureSets)
            return;

        m_NumberOfReferencePictureSets = NumberOfReferencePictureSets;
        referencePictureSet.resize(NumberOfReferencePictureSets);
    }

    ReferencePictureSet* getReferencePictureSet(int index) const { return &referencePictureSet[index]; }
    unsigned getNumberOfReferencePictureSets() const { return m_NumberOfReferencePictureSets; }

private:
    mutable std::vector<ReferencePictureSet> referencePictureSet;
};

// Sequence parameter set structure, corresponding to the HEVC bitstream definition.
struct H265SeqParamSetBase
{
    // bitstream params
    int32_t  sps_video_parameter_set_id;
    uint32_t  sps_max_sub_layers;
    uint8_t   sps_temporal_id_nesting_flag;

    H265ProfileTierLevel     m_pcPTL;

    uint8_t   sps_seq_parameter_set_id;
    uint8_t   chroma_format_idc;

    uint8_t   separate_colour_plane_flag;

    uint32_t  pic_width_in_luma_samples;
    uint32_t  pic_height_in_luma_samples;

    // cropping params
    uint8_t   conformance_window_flag;
    uint32_t  conf_win_left_offset;
    uint32_t  conf_win_right_offset;
    uint32_t  conf_win_top_offset;
    uint32_t  conf_win_bottom_offset;

    uint32_t  bit_depth_luma;
    uint32_t  bit_depth_chroma;

    uint32_t  log2_max_pic_order_cnt_lsb;
    uint8_t   sps_sub_layer_ordering_info_present_flag;

    uint32_t  sps_max_dec_pic_buffering[MAX_TEMPORAL_LAYER];
    uint32_t  sps_max_num_reorder_pics[MAX_TEMPORAL_LAYER];
    uint32_t  sps_max_latency_increase[MAX_TEMPORAL_LAYER];

    uint32_t  log2_min_luma_coding_block_size;
    uint32_t  log2_max_luma_coding_block_size;
    uint32_t  log2_min_transform_block_size;
    uint32_t  log2_max_transform_block_size;
    uint32_t  max_transform_hierarchy_depth_inter;
    uint32_t  max_transform_hierarchy_depth_intra;

    uint8_t   scaling_list_enabled_flag;
    uint8_t   sps_scaling_list_data_present_flag;

    uint8_t   amp_enabled_flag;
    uint8_t   sample_adaptive_offset_enabled_flag;

    uint8_t   pcm_enabled_flag;

    // pcm params
    uint32_t  pcm_sample_bit_depth_luma;
    uint32_t  pcm_sample_bit_depth_chroma;
    uint32_t  log2_min_pcm_luma_coding_block_size;
    uint32_t  log2_max_pcm_luma_coding_block_size;
    uint8_t   pcm_loop_filter_disabled_flag;

    uint32_t  num_short_term_ref_pic_sets;
    ReferencePictureSetList m_RPSList;

    uint8_t   long_term_ref_pics_present_flag;
    uint32_t  num_long_term_ref_pics_sps;
    uint32_t  lt_ref_pic_poc_lsb_sps[33];
    uint8_t   used_by_curr_pic_lt_sps_flag[33];

    uint8_t   sps_temporal_mvp_enabled_flag;
    uint8_t   sps_strong_intra_smoothing_enabled_flag;

    // vui part
    uint8_t   vui_parameters_present_flag;         // Zero indicates default VUI parameters

    uint8_t   aspect_ratio_info_present_flag;
    uint32_t  aspect_ratio_idc;
    uint32_t  sar_width;
    uint32_t  sar_height;

    uint8_t   overscan_info_present_flag;
    uint8_t   overscan_appropriate_flag;

    uint8_t   video_signal_type_present_flag;
    uint32_t  video_format;
    uint8_t   video_full_range_flag;
    uint8_t   colour_description_present_flag;
    uint32_t  colour_primaries;
    uint32_t  transfer_characteristics;
    uint32_t  matrix_coeffs;

    uint8_t   chroma_loc_info_present_flag;
    uint32_t  chroma_sample_loc_type_top_field;
    uint32_t  chroma_sample_loc_type_bottom_field;

    uint8_t   neutral_chroma_indication_flag;
    uint8_t   field_seq_flag;
    uint8_t   frame_field_info_present_flag;

    uint8_t   default_display_window_flag;
    uint32_t  def_disp_win_left_offset;
    uint32_t  def_disp_win_right_offset;
    uint32_t  def_disp_win_top_offset;
    uint32_t  def_disp_win_bottom_offset;

    uint8_t           vui_timing_info_present_flag;
    H265TimingInfo  m_timingInfo;

    uint8_t           vui_hrd_parameters_present_flag;
    H265HRD         m_hrdParameters;

    uint8_t   bitstream_restriction_flag;
    uint8_t   tiles_fixed_structure_flag;
    uint8_t   motion_vectors_over_pic_boundaries_flag;
    uint8_t   restricted_ref_pic_lists_flag;
    uint32_t  min_spatial_segmentation_idc;
    uint32_t  max_bytes_per_pic_denom;
    uint32_t  max_bits_per_min_cu_denom;
    uint32_t  log2_max_mv_length_horizontal;
    uint32_t  log2_max_mv_length_vertical;

    // sps extension
    uint8_t sps_range_extension_flag;
    uint8_t sps_scc_extension_flag;

    //range extention
    uint8_t transform_skip_rotation_enabled_flag;
    uint8_t transform_skip_context_enabled_flag;
    uint8_t implicit_residual_dpcm_enabled_flag;
    uint8_t explicit_residual_dpcm_enabled_flag;
    uint8_t extended_precision_processing_flag;
    uint8_t intra_smoothing_disabled_flag;
    uint8_t high_precision_offsets_enabled_flag;
    uint8_t fast_rice_adaptation_enabled_flag;
    uint8_t cabac_bypass_alignment_enabled_flag;

    //scc extention
    uint8_t sps_curr_pic_ref_enabled_flag;
    uint8_t palette_mode_enabled_flag;
    uint32_t palette_max_size;
    uint32_t delta_palette_max_predictor_size;
    uint8_t sps_palette_predictor_initializer_present_flag;
    uint32_t sps_num_palette_predictor_initializer;
    uint32_t motion_vector_resolution_control_idc;
    uint8_t intra_boundary_filtering_disabled_flag;

    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////

    uint32_t MaxCUSize;
    uint32_t MaxCUDepth;
    uint32_t MinCUSize;
    int32_t AddCUDepth;
    uint32_t WidthInCU;
    uint32_t HeightInCU;
    uint32_t NumPartitionsInCU, NumPartitionsInCUSize, NumPartitionsInFrameWidth;
    uint32_t m_maxTrSize;

    int32_t m_AMPAcc[MAX_CU_DEPTH]; //AMP Accuracy

    int m_QPBDOffsetY;
    int m_QPBDOffsetC;

    uint32_t ChromaArrayType;

    uint32_t chromaShiftW;
    uint32_t chromaShiftH;

    uint32_t need16bitOutput;

    void Reset()
    {
        *this = {};
    }

};    // H265SeqParamSetBase

// Sequence parameter set structure, corresponding to the HEVC bitstream definition.
struct H265SeqParamSet : public AVCParser::HeapObject, public H265SeqParamSetBase
{
    H265ScalingList     m_scalingList;
    std::vector<uint32_t> m_paletteInitializers;
    bool                m_changed;

    H265SeqParamSet()
        : HeapObject()
        , H265SeqParamSetBase()
    {
        Reset();
    }

    ~H265SeqParamSet()
    {}

    int32_t GetID() const
    {
        return sps_seq_parameter_set_id;
    }

    virtual void Reset()
    {
        H265SeqParamSetBase::Reset();

        m_RPSList.m_NumberOfReferencePictureSets = 0;
        m_paletteInitializers.clear();

        sps_video_parameter_set_id = MAX_NUM_VPS_PARAM_SETS_H265;
        sps_seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS_H265;

        // set some parameters by default
        video_format = 5; // unspecified
        video_full_range_flag = 0;
        colour_primaries = 2; // unspecified
        transfer_characteristics = 2; // unspecified
        matrix_coeffs = 2; // unspecified

        conformance_window_flag = 0;
        conf_win_left_offset = 0;
        conf_win_right_offset = 0;
        conf_win_top_offset = 0;
        conf_win_bottom_offset = 0;

        m_scalingList.destroy();
        m_changed = false;
    }

    int SubWidthC() const
    {
        static int32_t subWidth[] = {1, 2, 2, 1};
        SAMPLE_ASSERT (chroma_format_idc >= 0 && chroma_format_idc <= 4);
        return subWidth[chroma_format_idc];
    }

    int SubHeightC() const
    {
        static int32_t subHeight[] = {1, 2, 1, 1};
        SAMPLE_ASSERT (chroma_format_idc >= 0 && chroma_format_idc <= 4);
        return subHeight[chroma_format_idc];
    }

    int getQpBDOffsetY() const                  { return m_QPBDOffsetY; }
    void setQpBDOffsetY(int val)                { m_QPBDOffsetY = val; }
    int getQpBDOffsetC() const                  { return m_QPBDOffsetC; }
    void setQpBDOffsetC(int val)                { m_QPBDOffsetC = val; }

    void createRPSList(int32_t numRPS)
    {
        m_RPSList.allocate(numRPS);
    }

    H265ScalingList* getScalingList()           { return &m_scalingList; }
    H265ScalingList* getScalingList() const     { return const_cast<H265ScalingList *>(&m_scalingList); }
    ReferencePictureSetList *getRPSList()       { return &m_RPSList; }
    const ReferencePictureSetList *getRPSList() const       { return &m_RPSList; }

    H265ProfileTierLevel* getPTL() { return &m_pcPTL; }
    const H265ProfileTierLevel* getPTL() const    { return &m_pcPTL; }

    H265HRD* getHrdParameters                 ()             { return &m_hrdParameters; }

    const H265TimingInfo* getTimingInfo() const { return &m_timingInfo; }
    H265TimingInfo* getTimingInfo() { return &m_timingInfo; }
};    // H265SeqParamSet

// Tiles description
struct TileInfo
{
    int32_t firstCUAddr;
    int32_t endCUAddr;
    int32_t width;
};

// Picture parameter set structure, corresponding to the HEVC bitstream definition.
struct H265PicParamSetBase
{
    uint32_t  pps_pic_parameter_set_id;
    uint32_t  pps_seq_parameter_set_id;

    uint8_t   dependent_slice_segments_enabled_flag;
    uint8_t   output_flag_present_flag;
    uint32_t  num_extra_slice_header_bits;
    uint8_t   sign_data_hiding_enabled_flag;
    uint8_t   cabac_init_present_flag;

    uint32_t  num_ref_idx_l0_default_active;
    uint32_t  num_ref_idx_l1_default_active;

    int8_t   init_qp;                     // default QP for I,P,B slices
    uint8_t   constrained_intra_pred_flag;
    uint8_t   transform_skip_enabled_flag;
    uint8_t   cu_qp_delta_enabled_flag;
    uint32_t  diff_cu_qp_delta_depth;
    int32_t  pps_cb_qp_offset;
    int32_t  pps_cr_qp_offset;

    uint8_t   pps_slice_chroma_qp_offsets_present_flag;
    uint8_t   weighted_pred_flag;              // Nonzero indicates weighted prediction applied to P and SP slices
    uint8_t   weighted_bipred_flag;            // 0: no weighted prediction in B slices,  1: explicit weighted prediction
    uint8_t   transquant_bypass_enabled_flag;
    uint8_t   tiles_enabled_flag;
    uint8_t   entropy_coding_sync_enabled_flag;  // presence of wavefronts flag

    // tiles info
    uint32_t  num_tile_columns;
    uint32_t  num_tile_rows;
    uint32_t  uniform_spacing_flag;
    std::vector<uint32_t> column_width;
    std::vector<uint32_t> row_height;
    uint8_t   loop_filter_across_tiles_enabled_flag;

    uint8_t   pps_loop_filter_across_slices_enabled_flag;

    uint8_t   deblocking_filter_control_present_flag;
    uint8_t   deblocking_filter_override_enabled_flag;
    uint8_t   pps_deblocking_filter_disabled_flag;
    int32_t  pps_beta_offset;
    int32_t  pps_tc_offset;

    uint8_t   pps_scaling_list_data_present_flag;

    uint8_t   lists_modification_present_flag;
    uint32_t  log2_parallel_merge_level;
    uint8_t   slice_segment_header_extension_present_flag;

    // pps extension
    uint8_t  pps_range_extensions_flag;
    uint8_t  pps_scc_extension_flag;

    // pps range extension
    uint32_t log2_max_transform_skip_block_size_minus2;

    uint8_t cross_component_prediction_enabled_flag;
    uint8_t chroma_qp_offset_list_enabled_flag;
    uint32_t diff_cu_chroma_qp_offset_depth;
    uint32_t chroma_qp_offset_list_len;
    int32_t cb_qp_offset_list[MAX_CHROMA_OFFSET_ELEMENTS];
    int32_t cr_qp_offset_list[MAX_CHROMA_OFFSET_ELEMENTS];

    uint32_t log2_sao_offset_scale_luma;
    uint32_t log2_sao_offset_scale_chroma;

    // scc extension
    uint8_t pps_curr_pic_ref_enabled_flag;
    uint8_t residual_adaptive_colour_transform_enabled_flag;
    uint8_t pps_slice_act_qp_offsets_present_flag;
    int32_t pps_act_y_qp_offset;
    int32_t pps_act_cb_qp_offset;
    int32_t pps_act_cr_qp_offset;

    uint8_t pps_palette_predictor_initializer_present_flag;
    uint32_t pps_num_palette_predictor_initializer;
    uint8_t monochrome_palette_flag;
    uint32_t luma_bit_depth_entry;
    uint32_t chroma_bit_depth_entry;

    ///////////////////////////////////////////////////////
    // calculated params
    // These fields are calculated from values above.  They are not written to the bitstream
    ///////////////////////////////////////////////////////
    uint32_t getColumnWidth(uint32_t columnIdx) { return column_width[columnIdx]; }
    uint32_t getRowHeight(uint32_t rowIdx)    { return row_height[rowIdx]; }

    std::vector<uint32_t> m_CtbAddrRStoTS;
    std::vector<uint32_t> m_CtbAddrTStoRS;
    std::vector<uint32_t> m_TileIdx;

    void Reset()
    {
        *this = {};
    }
};  // H265PicParamSetBase

// Picture parameter set structure, corresponding to the H.264 bitstream definition.
struct H265PicParamSet : public AVCParser::HeapObject, public H265PicParamSetBase
{
    H265ScalingList       m_scalingList;
    std::vector<TileInfo> tilesInfo;
    std::vector<uint32_t>   m_paletteInitializers;
    bool                  m_changed;

    H265PicParamSet()
        : H265PicParamSetBase()
    {
        Reset();
    }

    void Reset()
    {
        column_width.clear();
        row_height.clear();
        m_CtbAddrRStoTS.clear();
        m_CtbAddrTStoRS.clear();
        m_TileIdx.clear();

        H265PicParamSetBase::Reset();

        tilesInfo.clear();
        m_paletteInitializers.clear();
        m_scalingList.destroy();

        pps_pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS_H265;
        pps_seq_parameter_set_id = MAX_NUM_SEQ_PARAM_SETS_H265;

        loop_filter_across_tiles_enabled_flag = true;
        pps_loop_filter_across_slices_enabled_flag = true;

        m_changed = false;
    }

    ~H265PicParamSet()
    {
    }

    int32_t GetID() const
    {
        return pps_pic_parameter_set_id;
    }

    uint32_t getNumTiles() const { return num_tile_rows*num_tile_columns; }
    H265ScalingList* getScalingList()               { return &m_scalingList; }
    H265ScalingList* getScalingList() const         { return const_cast<H265ScalingList *>(&m_scalingList); }
};    // H265PicParamSet

class HEVC_exception : public AVCParser::AVC_exception
{
public:
    HEVC_exception(mfxI32 status = -1) : AVC_exception(status) {}
};

enum
{
    CHROMA_FORMAT_400       = 0,
    CHROMA_FORMAT_420       = 1,
    CHROMA_FORMAT_422       = 2,
    CHROMA_FORMAT_444       = 3
};

} // namespace HEVCParser

#endif // __MFX_C2_HEVC_STRUCTURES_H__
