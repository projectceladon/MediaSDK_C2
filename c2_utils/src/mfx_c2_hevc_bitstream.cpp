// Copyright (c) 2018-2021 Intel Corporation
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

#include "mfx_c2_hevc_bitstream.h"

namespace HEVCParser
{

// Parse HRD information in VPS or in VUI block of SPS
void HEVCHeadersBitstream::parseHrdParameters(H265HRD *hrd, uint8_t cprms_present_flag, uint32_t vps_max_sub_layers)
{
    hrd->initial_cpb_removal_delay_length = 23 + 1;
    hrd->au_cpb_removal_delay_length = 23 + 1;
    hrd->dpb_output_delay_length = 23 + 1;

    if (cprms_present_flag)
    {
        hrd->nal_hrd_parameters_present_flag = Get1Bit();
        hrd->vcl_hrd_parameters_present_flag = Get1Bit();

        if (hrd->nal_hrd_parameters_present_flag || hrd->vcl_hrd_parameters_present_flag)
        {
            hrd->sub_pic_hrd_params_present_flag = Get1Bit();
            if (hrd->sub_pic_hrd_params_present_flag)
            {
                hrd->tick_divisor = GetBits(8) + 2;
                hrd->du_cpb_removal_delay_increment_length = GetBits(5) + 1;
                hrd->sub_pic_cpb_params_in_pic_timing_sei_flag = Get1Bit();
                hrd->dpb_output_delay_du_length = GetBits(5) + 1;
            }

            hrd->bit_rate_scale = GetBits(4);
            hrd->cpb_size_scale = GetBits(4);

            if (hrd->sub_pic_cpb_params_in_pic_timing_sei_flag)
            {
                hrd->cpb_size_du_scale = GetBits(4);
            }

            hrd->initial_cpb_removal_delay_length = GetBits(5) + 1;
            hrd->au_cpb_removal_delay_length = GetBits(5) + 1;
            hrd->dpb_output_delay_length = GetBits(5) + 1;
        }
    }

    for (uint32_t i = 0; i < vps_max_sub_layers; i++)
    {
        H265HrdSubLayerInfo * hrdSubLayerInfo = hrd->GetHRDSubLayerParam(i);
        hrdSubLayerInfo->fixed_pic_rate_general_flag = Get1Bit();

        if (!hrdSubLayerInfo->fixed_pic_rate_general_flag)
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = Get1Bit();
        }
        else
        {
            hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag = 1;
        }

        // Infered to be 0 when not present
        hrdSubLayerInfo->low_delay_hrd_flag = 0;
        hrdSubLayerInfo->cpb_cnt = 1;

        if (hrdSubLayerInfo->fixed_pic_rate_within_cvs_flag)
        {
            hrdSubLayerInfo->elemental_duration_in_tc = GetVLCElementU() + 1;
        }
        else
        {
            hrdSubLayerInfo->low_delay_hrd_flag = Get1Bit();
        }

        if (!hrdSubLayerInfo->low_delay_hrd_flag)
        {
            hrdSubLayerInfo->cpb_cnt = GetVLCElementU() + 1;

            if (hrdSubLayerInfo->cpb_cnt > 32)
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
        }

        for (uint32_t nalOrVcl = 0; nalOrVcl < 2; nalOrVcl++)
        {
            if((nalOrVcl == 0 && hrd->nal_hrd_parameters_present_flag) ||
               (nalOrVcl == 1 && hrd->vcl_hrd_parameters_present_flag))
            {
                for (uint32_t j = 0 ; j < hrdSubLayerInfo->cpb_cnt; j++)
                {
                    hrdSubLayerInfo->bit_rate_value[j][nalOrVcl] = GetVLCElementU() + 1;
                    hrdSubLayerInfo->cpb_size_value[j][nalOrVcl] = GetVLCElementU() + 1;
                    if (hrd->sub_pic_hrd_params_present_flag)
                    {
                        hrdSubLayerInfo->bit_rate_du_value[j][nalOrVcl] = GetVLCElementU() + 1;
                        hrdSubLayerInfo->cpb_size_du_value[j][nalOrVcl] = GetVLCElementU() + 1;
                    }

                    hrdSubLayerInfo->cbr_flag[j][nalOrVcl] = Get1Bit();
                }
            }
        }
    }
}

// Parse scaling list data block
void HEVCHeadersBitstream::xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId)
{
    SAMPLE_ASSERT(scalingList);

    int32_t i,coefNum = MSDK_MIN(MAX_MATRIX_COEF_NUM,(int32_t)g_scalingListSize[sizeId]);
    int32_t nextCoef = SCALING_LIST_START_VALUE;
    const uint16_t *scan  = (sizeId == 0) ? ScanTableDiag4x4 : g_sigLastScanCG32x32;
    int32_t *dst = scalingList->getScalingListAddress(sizeId, listId);

    if( sizeId > SCALING_LIST_8x8 )
    {
        int32_t scaling_list_dc_coef_minus8 = GetVLCElementS();
        if (scaling_list_dc_coef_minus8 < -7 || scaling_list_dc_coef_minus8 > 247)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        scalingList->setScalingListDC(sizeId, listId, scaling_list_dc_coef_minus8 + 8);
        nextCoef = scalingList->getScalingListDC(sizeId,listId);
    }

    for(i = 0; i < coefNum; i++)
    {
        int32_t scaling_list_delta_coef = GetVLCElementS();

        if (scaling_list_delta_coef < -128 || scaling_list_delta_coef > 127)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        nextCoef = (nextCoef + scaling_list_delta_coef + 256 ) % 256;
        dst[scan[i]] = nextCoef;
    }
}

// Parse scaling list information in SPS or PPS
void HEVCHeadersBitstream::parseScalingList(H265ScalingList *scalingList)
{
    if (!scalingList)
        throw HEVC_exception(MFX_ERR_NULL_PTR);

    //for each size
    for(uint32_t sizeId = 0; sizeId < SCALING_LIST_SIZE_NUM; sizeId++)
    {
        for(uint32_t listId = 0; listId <  g_scalingListNum[sizeId]; listId++)
        {
            uint8_t scaling_list_pred_mode_flag = Get1Bit();
            if(!scaling_list_pred_mode_flag) //Copy Mode
            {
                uint32_t scaling_list_pred_matrix_id_delta = GetVLCElementU();
                if (scaling_list_pred_matrix_id_delta > listId)
                    throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                scalingList->setRefMatrixId (sizeId, listId, listId-scaling_list_pred_matrix_id_delta);
                if (sizeId > SCALING_LIST_8x8)
                {
                    scalingList->setScalingListDC(sizeId,listId,((listId == scalingList->getRefMatrixId (sizeId,listId))? 16 :scalingList->getScalingListDC(sizeId, scalingList->getRefMatrixId (sizeId,listId))));
                }

                scalingList->processRefMatrix( sizeId, listId, scalingList->getRefMatrixId (sizeId,listId));
            }
            else //DPCM Mode
            {
                xDecodeScalingList(scalingList, sizeId, listId);
            }
        }
    }
}

// Parse profile tier layers header part in VPS or SPS
void HEVCHeadersBitstream::parsePTL(H265ProfileTierLevel *rpcPTL, int32_t maxNumSubLayersMinus1 )
{
    SAMPLE_ASSERT(rpcPTL);

    parseProfileTier(rpcPTL->GetGeneralPTL());

    int32_t level_idc = GetBits(8);
    level_idc = ((level_idc*10) / 30);
    rpcPTL->GetGeneralPTL()->level_idc = level_idc;

    for(int32_t i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (Get1Bit())
            rpcPTL->sub_layer_profile_present_flags |= 1 << i;
        if (Get1Bit())
            rpcPTL->sub_layer_level_present_flag |= 1 << i;
    }

    if (maxNumSubLayersMinus1 > 0)
    {
        for (int32_t i = maxNumSubLayersMinus1; i < 8; i++)
        {
            uint32_t reserved_zero_2bits = GetBits(2);
            if (reserved_zero_2bits)
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    for(int32_t i = 0; i < maxNumSubLayersMinus1; i++)
    {
        if (rpcPTL->sub_layer_profile_present_flags & (1 << i))
        {
            parseProfileTier(rpcPTL->GetSubLayerPTL(i));
        }

        if (rpcPTL->sub_layer_level_present_flag & (1 << i))
        {
            level_idc = GetBits(8);
            level_idc = ((level_idc*10) / 30);
            rpcPTL->GetSubLayerPTL(i)->level_idc = level_idc;
        }
    }
}

// Parse one profile tier layer
void HEVCHeadersBitstream::parseProfileTier(H265PTL *ptl)
{
    SAMPLE_ASSERT(ptl);

    ptl->profile_space = GetBits(2);
    if (ptl->profile_space)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    ptl->tier_flag = Get1Bit();
    ptl->profile_idc = GetBits(5);

    for(int32_t j = 0; j < 32; j++)
    {
        if (Get1Bit())
            ptl->profile_compatibility_flags |= 1 << j;
    }

    if (!ptl->profile_idc)
    {
        ptl->profile_idc = H265_PROFILE_MAIN;
        for(int32_t j = 1; j < 32; j++)
        {
            if (ptl->profile_compatibility_flags & (1 << j))
            {
                ptl->profile_idc = j;
                break;
            }
        }
    }

    if (ptl->profile_idc > H265_PROFILE_FREXT &&
        ptl->profile_idc != H265_PROFILE_SCC)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    ptl->progressive_source_flag    = Get1Bit();
    ptl->interlaced_source_flag     = Get1Bit();
    ptl->non_packed_constraint_flag = Get1Bit();
    ptl->frame_only_constraint_flag = Get1Bit();

    uint8_t reserved_zero_bits_num = 44; //incl. general_inbld_flag
    if (ptl->profile_idc == H265_PROFILE_FREXT || (ptl->profile_compatibility_flags & (1 << 4)) ||
        ptl->profile_idc == H265_PROFILE_SCC   || (ptl->profile_compatibility_flags & (1 << 9)))
    {
        ptl->max_12bit_constraint_flag = Get1Bit();
        ptl->max_10bit_constraint_flag = Get1Bit();
        ptl->max_8bit_constraint_flag = Get1Bit();
        ptl->max_422chroma_constraint_flag = Get1Bit();
        ptl->max_420chroma_constraint_flag = Get1Bit();
        ptl->max_monochrome_constraint_flag = Get1Bit();
        ptl->intra_constraint_flag = Get1Bit();
        ptl->one_picture_only_constraint_flag = Get1Bit();
        ptl->lower_bit_rate_constraint_flag = Get1Bit();

        if (ptl->profile_idc == H265_PROFILE_SCC   || (ptl->profile_compatibility_flags & (1 << 9)))
        {
            ptl->max_14bit_constraint_flag = Get1Bit();
            reserved_zero_bits_num = 34;
        }
        else
            reserved_zero_bits_num = 35;
    }

    uint32_t reserved_zero_bits;
    while (reserved_zero_bits_num > 32)
    {
        reserved_zero_bits = GetBits(32);
        reserved_zero_bits_num -= 32;
        //if (reserved_zero_bits)
        //    throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    if (reserved_zero_bits_num)
        reserved_zero_bits = GetBits(reserved_zero_bits_num);
}

// Parse SPS header
mfxStatus HEVCHeadersBitstream::GetSequenceParamSet(H265SeqParamSet *pcSPS)
{
    if (!pcSPS)
        throw HEVC_exception(MFX_ERR_NULL_PTR);

    pcSPS->sps_video_parameter_set_id = GetBits(4);

    pcSPS->sps_max_sub_layers = GetBits(3) + 1;

    if (pcSPS->sps_max_sub_layers > 7)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->sps_temporal_id_nesting_flag = Get1Bit();

    if (pcSPS->sps_max_sub_layers == 1)
    {
        // sps_temporal_id_nesting_flag must be 1 when sps_max_sub_layers_minus1 is 0
        if (pcSPS->sps_temporal_id_nesting_flag != 1)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    parsePTL(pcSPS->getPTL(), pcSPS->sps_max_sub_layers - 1);

    pcSPS->sps_seq_parameter_set_id = (uint8_t)GetVLCElementU();
    if (pcSPS->sps_seq_parameter_set_id > 15)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->chroma_format_idc = (uint8_t)GetVLCElementU();
    if (pcSPS->chroma_format_idc > 3)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    if (pcSPS->chroma_format_idc == 3)
    {
        pcSPS->separate_colour_plane_flag = Get1Bit();
    }

    pcSPS->ChromaArrayType = pcSPS->separate_colour_plane_flag ? 0 : pcSPS->chroma_format_idc;
    pcSPS->chromaShiftW = 1;
    pcSPS->chromaShiftH = pcSPS->ChromaArrayType == CHROMA_FORMAT_422 ? 0 : 1;

    pcSPS->pic_width_in_luma_samples  = GetVLCElementU();
    pcSPS->pic_height_in_luma_samples = GetVLCElementU();
    pcSPS->conformance_window_flag = Get1Bit();

    if (pcSPS->conformance_window_flag)
    {
        pcSPS->conf_win_left_offset  = GetVLCElementU()*pcSPS->SubWidthC();
        pcSPS->conf_win_right_offset = GetVLCElementU()*pcSPS->SubWidthC();
        pcSPS->conf_win_top_offset   = GetVLCElementU()*pcSPS->SubHeightC();
        pcSPS->conf_win_bottom_offset = GetVLCElementU()*pcSPS->SubHeightC();

        if (pcSPS->conf_win_left_offset + pcSPS->conf_win_right_offset >= pcSPS->pic_width_in_luma_samples)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        if (pcSPS->conf_win_top_offset + pcSPS->conf_win_bottom_offset >= pcSPS->pic_height_in_luma_samples)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    pcSPS->bit_depth_luma = GetVLCElementU() + 8;
    if (pcSPS->bit_depth_luma > 14)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->setQpBDOffsetY(6*(pcSPS->bit_depth_luma - 8));

    pcSPS->bit_depth_chroma = GetVLCElementU() + 8;
    if (pcSPS->bit_depth_chroma > 14)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->setQpBDOffsetC(6*(pcSPS->bit_depth_chroma - 8));

    if ((pcSPS->bit_depth_luma > 8 || pcSPS->bit_depth_chroma > 8) && pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc < H265_PROFILE_MAIN10)
        pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc = H265_PROFILE_MAIN10;

    if (pcSPS->m_pcPTL.GetGeneralPTL()->profile_idc == H265_PROFILE_MAIN10 || pcSPS->bit_depth_luma > 8 || pcSPS->bit_depth_chroma > 8)
        pcSPS->need16bitOutput = 1;

    pcSPS->log2_max_pic_order_cnt_lsb = 4 + GetVLCElementU();
    if (pcSPS->log2_max_pic_order_cnt_lsb > 16)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->sps_sub_layer_ordering_info_present_flag = Get1Bit();

    for (uint32_t i = 0; i < pcSPS->sps_max_sub_layers; i++)
    {
        pcSPS->sps_max_dec_pic_buffering[i] = GetVLCElementU() + 1;

        if (pcSPS->sps_max_dec_pic_buffering[i] > 16)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        pcSPS->sps_max_num_reorder_pics[i] = GetVLCElementU();

        if (pcSPS->sps_max_num_reorder_pics[i] > pcSPS->sps_max_dec_pic_buffering[i])
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        pcSPS->sps_max_latency_increase[i] = GetVLCElementU() - 1;

        if (!pcSPS->sps_sub_layer_ordering_info_present_flag)
        {
            for (i++; i <= pcSPS->sps_max_sub_layers-1; i++)
            {
                pcSPS->sps_max_dec_pic_buffering[i] = pcSPS->sps_max_dec_pic_buffering[0];
                pcSPS->sps_max_num_reorder_pics[i] = pcSPS->sps_max_num_reorder_pics[0];
                pcSPS->sps_max_latency_increase[i] = pcSPS->sps_max_latency_increase[0];
            }
            break;
        }

        if (i > 0)
        {
            if (pcSPS->sps_max_dec_pic_buffering[i] < pcSPS->sps_max_dec_pic_buffering[i - 1] ||
                pcSPS->sps_max_num_reorder_pics[i] < pcSPS->sps_max_num_reorder_pics[i - 1])
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    uint32_t const log2_min_luma_coding_block_size_minus3 = GetVLCElementU();
    if (log2_min_luma_coding_block_size_minus3 > 3)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->log2_min_luma_coding_block_size = log2_min_luma_coding_block_size_minus3 + 3;

    uint32_t MinCbLog2SizeY = pcSPS->log2_min_luma_coding_block_size;
    uint32_t MinCbSizeY = 1 << pcSPS->log2_min_luma_coding_block_size;

    if ((pcSPS->pic_width_in_luma_samples % MinCbSizeY) || (pcSPS->pic_height_in_luma_samples % MinCbSizeY))
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    uint32_t log2_diff_max_min_coding_block_size = GetVLCElementU();
    if (log2_diff_max_min_coding_block_size > 3)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->log2_max_luma_coding_block_size = log2_diff_max_min_coding_block_size + pcSPS->log2_min_luma_coding_block_size;
    //CtbLog2SizeY = (log2_min_luma_coding_block_size_minus3 + 3) + log2_diff_max_min_luma_coding_block_size //7.3.2.2 eq. 7-10, 7-11
    //CtbLog2SizeY derived according to active SPSs for the base layer shall be in the range of 4 to 6, inclusive. (A.2, main, main10, mainsp, rext profiles)
    if (pcSPS->log2_max_luma_coding_block_size < 4 || pcSPS->log2_max_luma_coding_block_size > 6)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->MaxCUSize =  1 << pcSPS->log2_max_luma_coding_block_size;

    pcSPS->log2_min_transform_block_size = GetVLCElementU() + 2;

    if (pcSPS->log2_min_transform_block_size >= MinCbLog2SizeY)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    uint32_t log2_diff_max_min_transform_block_size = GetVLCElementU();
    pcSPS->log2_max_transform_block_size = log2_diff_max_min_transform_block_size + pcSPS->log2_min_transform_block_size;
    pcSPS->m_maxTrSize = 1 << pcSPS->log2_max_transform_block_size;

    uint32_t CtbLog2SizeY = pcSPS->log2_max_luma_coding_block_size;
    if (pcSPS->log2_max_transform_block_size > MSDK_MIN(5, CtbLog2SizeY))
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->max_transform_hierarchy_depth_inter = GetVLCElementU();
    pcSPS->max_transform_hierarchy_depth_intra = GetVLCElementU();

    if (pcSPS->max_transform_hierarchy_depth_inter > CtbLog2SizeY - pcSPS->log2_min_transform_block_size)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    if (pcSPS->max_transform_hierarchy_depth_intra > CtbLog2SizeY - pcSPS->log2_min_transform_block_size)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    uint32_t addCUDepth = 0;
    while((pcSPS->MaxCUSize >> log2_diff_max_min_coding_block_size) > (uint32_t)( 1 << ( pcSPS->log2_min_transform_block_size + addCUDepth)))
    {
        addCUDepth++;
    }

    pcSPS->AddCUDepth = addCUDepth;
    pcSPS->MaxCUDepth = log2_diff_max_min_coding_block_size + addCUDepth;
    pcSPS->MinCUSize = pcSPS->MaxCUSize >> pcSPS->MaxCUDepth;
    // BB: these parameters may be removed completly and replaced by the fixed values
    pcSPS->scaling_list_enabled_flag = Get1Bit();
    if(pcSPS->scaling_list_enabled_flag)
    {
        pcSPS->sps_scaling_list_data_present_flag = Get1Bit();
        if(pcSPS->sps_scaling_list_data_present_flag)
        {
            parseScalingList( pcSPS->getScalingList() );
        }
    }

    pcSPS->amp_enabled_flag = Get1Bit();
    pcSPS->sample_adaptive_offset_enabled_flag = Get1Bit();
    pcSPS->pcm_enabled_flag = Get1Bit();

    if(pcSPS->pcm_enabled_flag)
    {
        pcSPS->pcm_sample_bit_depth_luma = GetBits(4) + 1;
        pcSPS->pcm_sample_bit_depth_chroma = GetBits(4) + 1;

        if (pcSPS->pcm_sample_bit_depth_luma > pcSPS->bit_depth_luma ||
            pcSPS->pcm_sample_bit_depth_chroma > pcSPS->bit_depth_chroma)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        pcSPS->log2_min_pcm_luma_coding_block_size = GetVLCElementU() + 3;

        if (pcSPS->log2_min_pcm_luma_coding_block_size < MSDK_MIN(MinCbLog2SizeY, 5) || pcSPS->log2_min_pcm_luma_coding_block_size > MSDK_MIN(CtbLog2SizeY, 5))
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        pcSPS->log2_max_pcm_luma_coding_block_size = GetVLCElementU() + pcSPS->log2_min_pcm_luma_coding_block_size;

        if (pcSPS->log2_max_pcm_luma_coding_block_size > MSDK_MIN(CtbLog2SizeY, 5))
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        pcSPS->pcm_loop_filter_disabled_flag = Get1Bit();
    }

    pcSPS->num_short_term_ref_pic_sets = GetVLCElementU();
    if (pcSPS->num_short_term_ref_pic_sets > 64)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcSPS->createRPSList(pcSPS->num_short_term_ref_pic_sets);

    ReferencePictureSetList* rpsList = pcSPS->getRPSList();
    ReferencePictureSet* rps;

    for(uint32_t i = 0; i < rpsList->getNumberOfReferencePictureSets(); i++)
    {
        rps = rpsList->getReferencePictureSet(i);
        parseShortTermRefPicSet(pcSPS, rps, i);

        if (((uint32_t)rps->getNumberOfNegativePictures() > pcSPS->sps_max_dec_pic_buffering[pcSPS->sps_max_sub_layers - 1]) ||
            ((uint32_t)rps->getNumberOfPositivePictures() > pcSPS->sps_max_dec_pic_buffering[pcSPS->sps_max_sub_layers - 1] - (uint32_t)rps->getNumberOfNegativePictures()))
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    pcSPS->long_term_ref_pics_present_flag = Get1Bit();
    if (pcSPS->long_term_ref_pics_present_flag)
    {
        pcSPS->num_long_term_ref_pics_sps = GetVLCElementU();

        if (pcSPS->num_long_term_ref_pics_sps > 32)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        for (uint32_t k = 0; k < pcSPS->num_long_term_ref_pics_sps; k++)
        {
            pcSPS->lt_ref_pic_poc_lsb_sps[k] = GetBits(pcSPS->log2_max_pic_order_cnt_lsb);
            pcSPS->used_by_curr_pic_lt_sps_flag[k] = Get1Bit();
        }
    }

    pcSPS->sps_temporal_mvp_enabled_flag = Get1Bit();
    pcSPS->sps_strong_intra_smoothing_enabled_flag = Get1Bit();
    pcSPS->vui_parameters_present_flag = Get1Bit();

    if (pcSPS->vui_parameters_present_flag)
    {
        parseVUI(pcSPS);
    }

    uint8_t sps_extension_present_flag = Get1Bit();
    if (sps_extension_present_flag)
    {
        pcSPS->sps_range_extension_flag = Get1Bit();
        GetBits(2); //skip sps_extension_2bits
        pcSPS->sps_scc_extension_flag = Get1Bit();
        uint32_t sps_extension_4bits = GetBits(4);
        bool skip_extension_bits = !!sps_extension_4bits;

        if (pcSPS->sps_range_extension_flag)
        {
            pcSPS->transform_skip_rotation_enabled_flag = Get1Bit();
            pcSPS->transform_skip_context_enabled_flag  = Get1Bit();
            pcSPS->implicit_residual_dpcm_enabled_flag  = Get1Bit();
            pcSPS->explicit_residual_dpcm_enabled_flag  = Get1Bit();
            pcSPS->extended_precision_processing_flag   = Get1Bit();
            pcSPS->intra_smoothing_disabled_flag        = Get1Bit();
            pcSPS->high_precision_offsets_enabled_flag  = Get1Bit();
            pcSPS->fast_rice_adaptation_enabled_flag    = Get1Bit();
            pcSPS->cabac_bypass_alignment_enabled_flag  = Get1Bit();
        }

        if (pcSPS->sps_scc_extension_flag)
        {
            if (pcSPS->getPTL()->GetGeneralPTL()->profile_idc != H265_PROFILE_SCC)
                skip_extension_bits = true;
            else
            {
                pcSPS->sps_curr_pic_ref_enabled_flag = Get1Bit();
                pcSPS->palette_mode_enabled_flag = Get1Bit();
                if (pcSPS->palette_mode_enabled_flag)
                {
                    pcSPS->palette_max_size = GetVLCElementU();
                    if (pcSPS->palette_max_size > 64)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    pcSPS->delta_palette_max_predictor_size = GetVLCElementU();
                    if (!pcSPS->palette_max_size && pcSPS->delta_palette_max_predictor_size)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    if (pcSPS->delta_palette_max_predictor_size > 128 - pcSPS->palette_max_size)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    pcSPS->sps_palette_predictor_initializer_present_flag = Get1Bit();
                    if (!pcSPS->palette_max_size && pcSPS->sps_palette_predictor_initializer_present_flag)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    if (pcSPS->sps_palette_predictor_initializer_present_flag)
                    {
                        uint32_t const PaletteMaxPredictorSize = pcSPS->palette_max_size + pcSPS->delta_palette_max_predictor_size;
                        SAMPLE_ASSERT(PaletteMaxPredictorSize > 0);

                        uint32_t const sps_num_palette_predictor_initializer_minus1 = GetVLCElementU();
                        if (sps_num_palette_predictor_initializer_minus1 > PaletteMaxPredictorSize - 1)
                            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
                        pcSPS->sps_num_palette_predictor_initializer = sps_num_palette_predictor_initializer_minus1 + 1;

                        uint8_t const numComps = pcSPS->chroma_format_idc ? 3 : 1;
                        pcSPS->m_paletteInitializers.resize(pcSPS->sps_num_palette_predictor_initializer * numComps);

                        for (uint8_t i = 0; i < numComps; ++i)
                            for (uint32_t j = 0; j < pcSPS->sps_num_palette_predictor_initializer; ++j)
                            {
                                uint32_t const num_bits =
                                    i == 0 ? pcSPS->bit_depth_luma : pcSPS->bit_depth_chroma;
                                pcSPS->m_paletteInitializers[i * pcSPS->sps_num_palette_predictor_initializer + j] = GetBits(num_bits);
                            }
                    }

                    pcSPS->motion_vector_resolution_control_idc = GetBits(2);
                    if (pcSPS->motion_vector_resolution_control_idc == 3)
                        //value of 3 for motion_vector_resolution_control_idc is reserved for future use by spec.
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    pcSPS->intra_boundary_filtering_disabled_flag = Get1Bit();
                }
            }
        }

        if (skip_extension_bits)
        {
            while (MoreRbspData())
            {
                Get1Bit(); //skip sps_extension_data_flag
            }
        }
    }

    return MFX_ERR_NONE;
}    // GetSequenceParamSet

// Parse video usability information block in SPS
void HEVCHeadersBitstream::parseVUI(H265SeqParamSet *pSPS)
{
    SAMPLE_ASSERT(pSPS);

    pSPS->aspect_ratio_info_present_flag = Get1Bit();
    if (pSPS->aspect_ratio_info_present_flag)
    {
        pSPS->aspect_ratio_idc = GetBits(8);
        if (pSPS->aspect_ratio_idc == 255)
        {
            pSPS->sar_width = GetBits(16);
            pSPS->sar_height = GetBits(16);
        }
        else
        {
            if (!pSPS->aspect_ratio_idc || pSPS->aspect_ratio_idc >= sizeof(SAspectRatio)/sizeof(SAspectRatio[0]))
            {
                pSPS->aspect_ratio_idc = 0;
                pSPS->aspect_ratio_info_present_flag = 0;
            }
            else
            {
                pSPS->sar_width = SAspectRatio[pSPS->aspect_ratio_idc][0];
                pSPS->sar_height = SAspectRatio[pSPS->aspect_ratio_idc][1];
            }
        }
    }

    pSPS->overscan_info_present_flag = Get1Bit();
    if (pSPS->overscan_info_present_flag)
    {
        pSPS->overscan_appropriate_flag = Get1Bit();
    }

    pSPS->video_signal_type_present_flag = Get1Bit();
    if (pSPS->video_signal_type_present_flag)
    {
        pSPS->video_format = GetBits(3);
        pSPS->video_full_range_flag = Get1Bit();
        pSPS->colour_description_present_flag = Get1Bit();
        if (pSPS->colour_description_present_flag)
        {
            pSPS->colour_primaries = GetBits(8);
            pSPS->transfer_characteristics = GetBits(8);
            pSPS->matrix_coeffs = GetBits(8);
        }
    }

    pSPS->chroma_loc_info_present_flag = Get1Bit();
    if (pSPS->chroma_loc_info_present_flag)
    {
        pSPS->chroma_sample_loc_type_top_field = GetVLCElementU();
        pSPS->chroma_sample_loc_type_bottom_field = GetVLCElementU();
    }

    pSPS->neutral_chroma_indication_flag = Get1Bit();
    pSPS->field_seq_flag = Get1Bit();
    pSPS->frame_field_info_present_flag = Get1Bit();

    pSPS->default_display_window_flag = Get1Bit();
    if (pSPS->default_display_window_flag)
    {
        pSPS->def_disp_win_left_offset   = GetVLCElementU()*pSPS->SubWidthC();
        pSPS->def_disp_win_right_offset  = GetVLCElementU()*pSPS->SubWidthC();
        pSPS->def_disp_win_top_offset    = GetVLCElementU()*pSPS->SubHeightC();
        pSPS->def_disp_win_bottom_offset = GetVLCElementU()*pSPS->SubHeightC();

        if (pSPS->def_disp_win_left_offset + pSPS->def_disp_win_right_offset >= pSPS->pic_width_in_luma_samples)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        if (pSPS->def_disp_win_top_offset + pSPS->def_disp_win_bottom_offset >= pSPS->pic_height_in_luma_samples)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
    }

    H265TimingInfo *timingInfo = pSPS->getTimingInfo();
    timingInfo->vps_timing_info_present_flag = Get1Bit();
    if (timingInfo->vps_timing_info_present_flag)
    {
        timingInfo->vps_num_units_in_tick = GetBits(32);
        timingInfo->vps_time_scale = GetBits(32);

        timingInfo->vps_poc_proportional_to_timing_flag = Get1Bit();
        if (timingInfo->vps_poc_proportional_to_timing_flag)
        {
            timingInfo->vps_num_ticks_poc_diff_one = GetVLCElementU() + 1;
        }

        pSPS->vui_hrd_parameters_present_flag = Get1Bit();
        if (pSPS->vui_hrd_parameters_present_flag)
        {
            parseHrdParameters( pSPS->getHrdParameters(), 1, pSPS->sps_max_sub_layers);
        }
    }

    pSPS->bitstream_restriction_flag = Get1Bit();
    if (pSPS->bitstream_restriction_flag)
    {
        pSPS->tiles_fixed_structure_flag = Get1Bit();
        pSPS->motion_vectors_over_pic_boundaries_flag = Get1Bit();
        pSPS->restricted_ref_pic_lists_flag = Get1Bit();
        pSPS->min_spatial_segmentation_idc = GetVLCElementU();
        pSPS->max_bytes_per_pic_denom = GetVLCElementU();
        pSPS->max_bits_per_min_cu_denom = GetVLCElementU();
        pSPS->log2_max_mv_length_horizontal = GetVLCElementU();
        pSPS->log2_max_mv_length_vertical = GetVLCElementU();
    }
}

// Reserved for future header extensions
bool HEVCHeadersBitstream::MoreRbspData()
{
    return false;
}

// Parse PPS header
mfxStatus HEVCHeadersBitstream::GetPictureParamSetPart1(H265PicParamSet *pcPPS)
{
    if (!pcPPS)
        throw HEVC_exception(MFX_ERR_NULL_PTR);

    pcPPS->pps_pic_parameter_set_id = GetVLCElementU();
    if (pcPPS->pps_pic_parameter_set_id > 63)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcPPS->pps_seq_parameter_set_id = GetVLCElementU();
    if (pcPPS->pps_seq_parameter_set_id > 15)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    return MFX_ERR_NONE;
}

mfxStatus HEVCHeadersBitstream::GetPictureParamSetFull(H265PicParamSet  *pcPPS, H265SeqParamSet const* pcSPS)
{
    if (!pcPPS)
        throw HEVC_exception(MFX_ERR_NULL_PTR);

    if (!pcSPS)
        throw HEVC_exception(MFX_ERR_NULL_PTR);

    pcPPS->dependent_slice_segments_enabled_flag = Get1Bit();
    pcPPS->output_flag_present_flag = Get1Bit();
    pcPPS->num_extra_slice_header_bits = GetBits(3);
    pcPPS->sign_data_hiding_enabled_flag =  Get1Bit();
    pcPPS->cabac_init_present_flag =  Get1Bit();
    pcPPS->num_ref_idx_l0_default_active = GetVLCElementU() + 1;

    if (pcPPS->num_ref_idx_l0_default_active > 15)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcPPS->num_ref_idx_l1_default_active = GetVLCElementU() + 1;

    if (pcPPS->num_ref_idx_l1_default_active > 15)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcPPS->init_qp = (int8_t)(GetVLCElementS() + 26);

    pcPPS->constrained_intra_pred_flag = Get1Bit();
    pcPPS->transform_skip_enabled_flag = Get1Bit();

    pcPPS->cu_qp_delta_enabled_flag = Get1Bit();
    if( pcPPS->cu_qp_delta_enabled_flag )
    {
        pcPPS->diff_cu_qp_delta_depth = GetVLCElementU();
    }
    else
    {
        pcPPS->diff_cu_qp_delta_depth = 0;
    }

    pcPPS->pps_cb_qp_offset = GetVLCElementS();
    if (pcPPS->pps_cb_qp_offset < -12 || pcPPS->pps_cb_qp_offset > 12)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcPPS->pps_cr_qp_offset = GetVLCElementS();

    if (pcPPS->pps_cr_qp_offset < -12 || pcPPS->pps_cr_qp_offset > 12)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    pcPPS->pps_slice_chroma_qp_offsets_present_flag = Get1Bit();

    pcPPS->weighted_pred_flag = Get1Bit();
    pcPPS->weighted_bipred_flag = Get1Bit();

    pcPPS->transquant_bypass_enabled_flag = Get1Bit();
    pcPPS->tiles_enabled_flag = Get1Bit();
    pcPPS->entropy_coding_sync_enabled_flag = Get1Bit();

    if (pcPPS->tiles_enabled_flag)
    {
        pcPPS->num_tile_columns = GetVLCElementU() + 1;
        pcPPS->num_tile_rows = GetVLCElementU() + 1;

        if (pcPPS->num_tile_columns == 1 && pcPPS->num_tile_rows == 1)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        pcPPS->uniform_spacing_flag = Get1Bit();

        if (!pcPPS->uniform_spacing_flag)
        {
            pcPPS->column_width.resize(pcPPS->num_tile_columns);
            for (uint32_t i=0; i < pcPPS->num_tile_columns - 1; i++)
            {
                pcPPS->column_width[i] = GetVLCElementU() + 1;
            }

            pcPPS->row_height.resize(pcPPS->num_tile_rows);

            for (uint32_t i=0; i < pcPPS->num_tile_rows - 1; i++)
            {
                pcPPS->row_height[i] = GetVLCElementU() + 1;
            }
        }

        if (pcPPS->num_tile_columns - 1 != 0 || pcPPS->num_tile_rows - 1 != 0)
        {
            pcPPS->loop_filter_across_tiles_enabled_flag = Get1Bit();
        }
    }
    else
    {
        pcPPS->num_tile_columns = 1;
        pcPPS->num_tile_rows = 1;
    }

    pcPPS->pps_loop_filter_across_slices_enabled_flag = Get1Bit();
    pcPPS->deblocking_filter_control_present_flag = Get1Bit();

    if (pcPPS->deblocking_filter_control_present_flag)
    {
        pcPPS->deblocking_filter_override_enabled_flag = Get1Bit();
        pcPPS->pps_deblocking_filter_disabled_flag = Get1Bit();
        if (!pcPPS->pps_deblocking_filter_disabled_flag)
        {
            pcPPS->pps_beta_offset = GetVLCElementS() << 1;
            pcPPS->pps_tc_offset = GetVLCElementS() << 1;

            if (pcPPS->pps_beta_offset < -12 || pcPPS->pps_beta_offset > 12)
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

            if (pcPPS->pps_tc_offset < -12 || pcPPS->pps_tc_offset > 12)
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
        }
    }

    pcPPS->pps_scaling_list_data_present_flag = Get1Bit();
    if(pcPPS->pps_scaling_list_data_present_flag)
    {
        parseScalingList( pcPPS->getScalingList() );
    }

    pcPPS->lists_modification_present_flag = Get1Bit();
    pcPPS->log2_parallel_merge_level = GetVLCElementU() + 2;
    pcPPS->slice_segment_header_extension_present_flag = Get1Bit();

    uint8_t pps_extension_present_flag = Get1Bit();
    if (pps_extension_present_flag)
    {
        pcPPS->pps_range_extensions_flag = Get1Bit();
        GetBits(2); //skip pps_extension_2bits
        pcPPS->pps_scc_extension_flag = Get1Bit();
        uint32_t pps_extension_4bits = GetBits(4);
        bool skip_extension_bits = !!pps_extension_4bits;

        if (pcPPS->pps_range_extensions_flag)
        {
            if (pcPPS->transform_skip_enabled_flag)
            {
                pcPPS->log2_max_transform_skip_block_size_minus2 = GetVLCElementU();
            }

            pcPPS->cross_component_prediction_enabled_flag = Get1Bit();
            pcPPS->chroma_qp_offset_list_enabled_flag = Get1Bit();
            if (pcPPS->chroma_qp_offset_list_enabled_flag)
            {
                pcPPS->diff_cu_chroma_qp_offset_depth = GetVLCElementU();
                pcPPS->chroma_qp_offset_list_len = GetVLCElementU() + 1;

                if (pcPPS->chroma_qp_offset_list_len > 6)
                    throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                for (uint32_t i = 0; i < pcPPS->chroma_qp_offset_list_len; i++)
                {
                    pcPPS->cb_qp_offset_list[i + 1] = GetVLCElementS();
                    pcPPS->cr_qp_offset_list[i + 1] = GetVLCElementS();

                    if (pcPPS->cb_qp_offset_list[i + 1] < -12 || pcPPS->cb_qp_offset_list[i + 1] > 12)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
                    if (pcPPS->cr_qp_offset_list[i + 1] < -12 || pcPPS->cr_qp_offset_list[i + 1] > 12)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
                }
            }

            pcPPS->log2_sao_offset_scale_luma = GetVLCElementU();
            pcPPS->log2_sao_offset_scale_chroma = GetVLCElementU();
        }

        if (pcPPS->pps_scc_extension_flag)
        {
            if (pcSPS->getPTL()->GetGeneralPTL()->profile_idc != H265_PROFILE_SCC)
                skip_extension_bits = true;
            else
            {
                pcPPS->pps_curr_pic_ref_enabled_flag = Get1Bit();
                pcPPS->residual_adaptive_colour_transform_enabled_flag = Get1Bit();
                if (pcPPS->residual_adaptive_colour_transform_enabled_flag)
                {
                    pcPPS->pps_slice_act_qp_offsets_present_flag = Get1Bit();
                    int32_t const pps_act_y_qp_offset_plus5 = GetVLCElementS();
                    if (pps_act_y_qp_offset_plus5 < -7 || pps_act_y_qp_offset_plus5 > 17)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
                    pcPPS->pps_act_y_qp_offset  = pps_act_y_qp_offset_plus5 - 5;

                    int32_t const pps_act_cb_qp_offset_plus5 = GetVLCElementS();
                    if (pps_act_cb_qp_offset_plus5 < -7 || pps_act_cb_qp_offset_plus5 > 17)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
                    pcPPS->pps_act_cb_qp_offset = pps_act_cb_qp_offset_plus5 - 5;

                    int32_t const pps_act_cr_qp_offset_plus3 = GetVLCElementS();
                    if (pps_act_cr_qp_offset_plus3 < -9 || pps_act_cr_qp_offset_plus3 > 15)
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);
                    pcPPS->pps_act_cr_qp_offset = pps_act_cr_qp_offset_plus3 - 3;
                }

                pcPPS->pps_palette_predictor_initializer_present_flag = Get1Bit();
                if (pcPPS->pps_palette_predictor_initializer_present_flag)
                {
                    if (pcSPS->palette_max_size == 0 || !pcSPS->palette_mode_enabled_flag)
                        //pps_palette_predictor_initializer_present_flag shall be equal to 0 when either palette_max_size is equal to 0 or palette_mode_enabled_flag is equal to 0
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    pcPPS->pps_num_palette_predictor_initializer = GetVLCElementU();
                    if (pcPPS->pps_num_palette_predictor_initializer > 128)
                        //accord. to spec. pps_num_palette_predictor_initializer can't exceed PaletteMaxPredictorSize
                        //that's checked at [supplayer :: xDecodePPS]
                        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                    if (pcPPS->pps_num_palette_predictor_initializer)
                    {
                        pcPPS->monochrome_palette_flag = Get1Bit();

                        uint32_t const luma_bit_depth_entry_minus8 = GetVLCElementU();
                        if (luma_bit_depth_entry_minus8 > 6)
                            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                        pcPPS->luma_bit_depth_entry = luma_bit_depth_entry_minus8 + 8;

                        if (!pcPPS->monochrome_palette_flag)
                        {
                            uint32_t const chroma_bit_depth_entry_minus8 = GetVLCElementU();
                            if (chroma_bit_depth_entry_minus8 > 6)
                                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

                            pcPPS->chroma_bit_depth_entry = chroma_bit_depth_entry_minus8 + 8;
                        }

                        uint8_t const numComps = pcPPS->monochrome_palette_flag ? 1 : 3;
                        pcPPS->m_paletteInitializers.resize(pcPPS->pps_num_palette_predictor_initializer * numComps);

                        for (uint8_t i = 0; i < numComps; ++i)
                            for (uint32_t j = 0; j < pcPPS->pps_num_palette_predictor_initializer; ++j)
                            {
                                uint32_t const num_bits =
                                    i == 0 ? pcPPS->luma_bit_depth_entry : pcPPS->chroma_bit_depth_entry;
                                pcPPS->m_paletteInitializers[i * pcPPS->pps_num_palette_predictor_initializer + j] = GetBits(num_bits);
                            }
                    }
                }
            }
        }

        if (skip_extension_bits)
        {
            while (MoreRbspData())
            {
                Get1Bit(); //skip pps_extension_data_flag
            }
        }
    }

    return MFX_ERR_NONE;
}   // H265HeadersBitstream::GetPictureParamSetFull

mfxStatus HEVCHeadersBitstream::GetNALUnitType(NalUnitType &nal_unit_type, mfxU32 &nuh_temporal_id)
{
    mfxU32 forbidden_zero_bit = Get1Bit();
    if (forbidden_zero_bit)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    nal_unit_type = (NalUnitType)GetBits(6);
    mfxU32 nuh_layer_id = GetBits(6);
    if (nuh_layer_id)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 const nuh_temporal_id_plus1 = GetBits(3);
    if (!nuh_temporal_id_plus1)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    nuh_temporal_id = nuh_temporal_id_plus1 - 1;
    if (nuh_temporal_id)
    {
        SAMPLE_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_BLA_W_LP
                    && nal_unit_type != NAL_UT_CODED_SLICE_BLA_W_RADL
                    && nal_unit_type != NAL_UT_CODED_SLICE_BLA_N_LP
                    && nal_unit_type != NAL_UT_CODED_SLICE_IDR_W_RADL
                    && nal_unit_type != NAL_UT_CODED_SLICE_IDR_N_LP
                    && nal_unit_type != NAL_UT_CODED_SLICE_CRA
                    && nal_unit_type != NAL_UT_VPS
                    && nal_unit_type != NAL_UT_SPS
                    && nal_unit_type != NAL_UT_EOS
                    && nal_unit_type != NAL_UT_EOB );
    }
    else
    {
        SAMPLE_ASSERT( nal_unit_type != NAL_UT_CODED_SLICE_TSA_R
                    && nal_unit_type != NAL_UT_CODED_SLICE_TSA_N
                    && nal_unit_type != NAL_UT_CODED_SLICE_STSA_R
                    && nal_unit_type != NAL_UT_CODED_SLICE_STSA_N );
    }

    return MFX_ERR_NONE;
}

void HEVCHeadersBitstream::GetSEI(mfxPayload *spl, mfxU32 type)
{
    if (nullptr == spl)
       return;

    if ((mfxI32)BytesLeft() <= 0) // not enough bitstream
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    mfxU32 code;
    mfxPayload currentSEI;

    GetNBits(m_pbs, m_bitOffset, 8, code);

    while ((mfxI32)BytesLeft() > 0)
    {
        ParseSEI(&currentSEI);

        if (type == currentSEI.Type)
        {
            spl->Type = currentSEI.Type;
            spl->NumBit = currentSEI.NumBit;

            if ((currentSEI.NumBit / 8) > BytesLeft()) // corrupted stream
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

            if (nullptr != spl->Data)
            {
                for (mfxU32 i = 0; i < (spl->NumBit / 8); i++)
                {
                    GetNBits(m_pbs, m_bitOffset, 8, spl->Data[i]);
                }
            }
            return;
        }
        else // skip SEI data
        {
            if ((currentSEI.NumBit / 8) > BytesLeft())// corrupted stream
                throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

            mfxU8 tmp;
            for (mfxU32 i = 0; i < (currentSEI.NumBit / 8); i++)
            {
                GetNBits(m_pbs, m_bitOffset, 8, tmp);
            }
        }
    }
}

void HEVCHeadersBitstream::ParseSEI(mfxPayload *spl)
{
    if (nullptr == spl && BytesLeft() < 2)
       return;

    mfxU32 code;
    mfxI32 payloadType = 0;

    while ((mfxI32)BytesLeft() > 0)
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        PeakNextBits(m_pbs, m_bitOffset, 8, code);
        if (0xFF != code)
            break;
        GetNBits(m_pbs, m_bitOffset, 8, code);
        payloadType += 255;
    }

    if ((mfxI32)BytesLeft() > 0)
    {
        mfxI32 last_payload_type_byte = 0;
        GetNBits(m_pbs, m_bitOffset, 8, last_payload_type_byte);
        payloadType += last_payload_type_byte;
    }

    mfxU32 payloadSize = 0;

    while((mfxI32)BytesLeft() > 0)
    {
        /* fixed-pattern bit string using 8 bits written equal to 0xFF */
        PeakNextBits(m_pbs, m_bitOffset, 8, code);
        if (0xFF != code)
            break;
        GetNBits(m_pbs, m_bitOffset, 8, code);
        payloadSize += 255;
    }

    if ((mfxI32)BytesLeft() > 0)
    {
        mfxI32 last_payload_size_byte = 0;
        GetNBits(m_pbs, m_bitOffset, 8, last_payload_size_byte);
        payloadSize += last_payload_size_byte;
    }

    if (spl)
    {
        spl->NumBit = payloadSize * 8;
        spl->Type = payloadType;
    }
}

// Parse RPS part in SPS or slice header
void HEVCHeadersBitstream::parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* rps, uint32_t idx)
{
    if (!sps || !rps)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    if (idx > 0)
    {
        rps->inter_ref_pic_set_prediction_flag = Get1Bit();
    }
    else
        rps->inter_ref_pic_set_prediction_flag = 0;

    if (rps->inter_ref_pic_set_prediction_flag)
    {
        uint32_t delta_idx_minus1 = 0;

        if (idx == sps->getRPSList()->getNumberOfReferencePictureSets())
        {
            delta_idx_minus1 = GetVLCElementU();
        }

        if (delta_idx_minus1 > idx - 1)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        uint32_t rIdx = idx - 1 - delta_idx_minus1;

        if (rIdx > idx - 1)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        ReferencePictureSet*   rpsRef = const_cast<H265SeqParamSet *>(sps)->getRPSList()->getReferencePictureSet(rIdx);
        uint32_t k = 0, k0 = 0, k1 = 0;
        uint32_t delta_rps_sign = Get1Bit();
        uint32_t abs_delta_rps_minus1 = GetVLCElementU();

        int32_t deltaRPS = (1 - 2 * delta_rps_sign) * (abs_delta_rps_minus1 + 1);
        uint32_t num_pics = rpsRef->getNumberOfPictures();
        for(uint32_t j = 0 ;j <= num_pics; j++)
        {
            uint32_t used_by_curr_pic_flag = Get1Bit();
            int32_t refIdc = used_by_curr_pic_flag;
            if (refIdc == 0)
            {
                uint32_t use_delta_flag = Get1Bit();
                refIdc = use_delta_flag << 1;
            }

            if (refIdc == 1 || refIdc == 2)
            {
                int32_t deltaPOC = deltaRPS + ((j < rpsRef->getNumberOfPictures())? rpsRef->getDeltaPOC(j) : 0);
                rps->setDeltaPOC(k, deltaPOC);
                rps->used_by_curr_pic_flag[k] = refIdc == 1;

                if (deltaPOC < 0)
                {
                    k0++;
                }
                else
                {
                    k1++;
                }
                k++;
            }
        }
        rps->num_pics = k;
        rps->num_negative_pics = k0;
        rps->num_positive_pics = k1;
        rps->sortDeltaPOC();
    }
    else
    {
        rps->num_negative_pics = GetVLCElementU();
        rps->num_positive_pics = GetVLCElementU();

        if (rps->num_negative_pics >= MAX_NUM_REF_PICS || rps->num_positive_pics >= MAX_NUM_REF_PICS || (rps->num_positive_pics + rps->num_negative_pics) >= MAX_NUM_REF_PICS)
            throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

        int32_t prev = 0;
        int32_t poc;
        for(uint32_t j=0 ; j < rps->getNumberOfNegativePictures(); j++)
        {
            uint32_t delta_poc_s0_minus1 = GetVLCElementU();
            poc = prev - delta_poc_s0_minus1 - 1;
            prev = poc;
            rps->setDeltaPOC(j,poc);
            rps->used_by_curr_pic_flag[j] = Get1Bit();
        }

        prev = 0;
        for(uint32_t j=rps->getNumberOfNegativePictures(); j < rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures(); j++)
        {
            uint32_t delta_poc_s1_minus1 = GetVLCElementU();
            poc = prev + delta_poc_s1_minus1 + 1;
            prev = poc;
            rps->setDeltaPOC(j,poc);
            rps->used_by_curr_pic_flag[j] = Get1Bit();
        }
        rps->num_pics = rps->getNumberOfNegativePictures()+rps->getNumberOfPositivePictures();
    }
}

}; // namespace HEVCParser

