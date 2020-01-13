/* ====================================================================================================================

  The copyright in this software is being made available under the License included below.
  This software may be subject to other third party and contributor rights, including patent rights, and no such
  rights are granted under this license.

  Copyright (c) 2018, HUAWEI TECHNOLOGIES CO., LTD. All rights reserved.
  Copyright (c) 2018, SAMSUNG ELECTRONICS CO., LTD. All rights reserved.
  Copyright (c) 2018, PEKING UNIVERSITY SHENZHEN GRADUATE SCHOOL. All rights reserved.
  Copyright (c) 2018, PENGCHENG LABORATORY. All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted only for
  the purpose of developing standards within Audio and Video Coding Standard Workgroup of China (AVS) and for testing and
  promoting such standards. The following conditions are required to be met:

    * Redistributions of source code must retain the above copyright notice, this list of conditions and
      the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
      the following disclaimer in the documentation and/or other materials provided with the distribution.
    * The name of HUAWEI TECHNOLOGIES CO., LTD. or SAMSUNG ELECTRONICS CO., LTD. may not be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

* ====================================================================================================================
*/

#include "com_sao.h"

void init_StateDate(SAOStatData *statsDate)
{
    int i;
    for (i = 0; i < MAX_NUM_SAO_CLASSES; i++)
    {
        statsDate->diff[i] = 0;
        statsDate->count[i] = 0;
    }
}


long long int get_distortion(int compIdx, int type, SAOStatData **saostatData, SAOBlkParam *sao_cur_param)
{
    int classIdc, bandIdx;
    long long int dist = 0;
    switch (type)
    {
    case SAO_TYPE_EO_0:
    case SAO_TYPE_EO_90:
    case SAO_TYPE_EO_135:
    case SAO_TYPE_EO_45:
    {
        for (classIdc = 0; classIdc < NUM_SAO_EO_CLASSES; classIdc++)
        {
            dist += distortion_cal(saostatData[compIdx][type].count[classIdc], sao_cur_param[compIdx].offset[classIdc],
                                   saostatData[compIdx][type].diff[classIdc]);
        }
    }
    break;
    case SAO_TYPE_BO:
    {
        for (classIdc = 0; classIdc < NUM_BO_OFFSET; classIdc++)
        {
            bandIdx = classIdc % NUM_SAO_BO_CLASSES;
            dist += distortion_cal(saostatData[compIdx][type].count[bandIdx], sao_cur_param[compIdx].offset[bandIdx],
                                   saostatData[compIdx][type].diff[bandIdx]);
        }
    }
    break;
    default:
    {
        printf("Not a supported type");
        assert(0);
        exit(-1);
    }
    }
    return dist;
}
long long int  distortion_cal(long long int count, int offset, long long int diff)
{
    return (count * (long long int)offset * (long long int)offset - diff * offset * 2);
}



void off_sao(SAOBlkParam *saoblkparam)
{
    int i;
    for (i = 0; i < N_C; i++)
    {
        saoblkparam[i].modeIdc = SAO_MODE_OFF;
        saoblkparam[i].typeIdc = -1;
        saoblkparam[i].startBand = -1;
        saoblkparam[i].startBand2 = -1;
        saoblkparam[i].deltaband = -1;
        memset(saoblkparam[i].offset, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);
    }
}
void copySAOParam_for_blk(SAOBlkParam *saopara_dst, SAOBlkParam *saopara_src)
{
    int i, j;
    for (i = 0; i < N_C; i++)
    {
        saopara_dst[i].modeIdc = saopara_src[i].modeIdc;
        saopara_dst[i].typeIdc = saopara_src[i].typeIdc;
        saopara_dst[i].startBand = saopara_src[i].startBand;
        saopara_dst[i].startBand2 = saopara_src[i].startBand2;
        saopara_dst[i].deltaband = saopara_src[i].deltaband;
        for (j = 0; j < MAX_NUM_SAO_CLASSES; j++)
        {
            saopara_dst[i].offset[j] = saopara_src[i].offset[j];
        }
    }
}
void copySAOParam_for_blk_onecomponent(SAOBlkParam *saopara_dst, SAOBlkParam *saopara_src)
{
    int  j;
    saopara_dst->modeIdc = saopara_src->modeIdc;
    saopara_dst->typeIdc = saopara_src->typeIdc;
    saopara_dst->startBand = saopara_src->startBand;
    saopara_dst->startBand2 = saopara_src->startBand2;
    saopara_dst->deltaband = saopara_src->deltaband;
    for (j = 0; j < MAX_NUM_SAO_CLASSES; j++)
    {
        saopara_dst->offset[j] = saopara_src->offset[j];
    }
}

void Copy_LCU_for_SAO(COM_PIC * pic_dst, COM_PIC * pic_src, int pix_y, int pix_x, int lcu_pix_height, int lcu_pix_width)
{
    int j;
    pel* src;
    pel* dst;
    int src_Stride, dst_Stride;
    int src_offset, dst_offset;
    //assert(pic_src->stride_luma == pic_dst->stride_luma);
    src_Stride = pic_src->stride_luma;
    dst_Stride = pic_dst->stride_luma;
    src_offset = pix_y*src_Stride + pix_x;
    dst_offset = pix_y*dst_Stride + pix_x;
    src = pic_src->y + src_offset;
    dst = pic_dst->y + dst_offset;
    for (j = 0; j < lcu_pix_height; j++)
    {
        for (int i = 0; i < lcu_pix_width; i++)
        {
            dst[i] = src[i];
        }
        //com_mcpy(dst, src, sizeof(pel) *lcu_pix_width);
        dst += dst_Stride;
        src += src_Stride;
    }
    src_Stride = pic_src->stride_chroma;
    dst_Stride = pic_dst->stride_chroma;
    src_offset = (pix_y >> 1)*src_Stride + (pix_x >> 1);
    dst_offset = (pix_y >> 1)*dst_Stride + (pix_x >> 1);
    src = pic_src->u + src_offset;
    dst = pic_dst->u + dst_offset;
    for (j = 0; j < (lcu_pix_height >> 1); j++)
    {
        for (int i = 0; i < (lcu_pix_width >> 1); i++)
        {
            dst[i] = src[i];
        }
        //com_mcpy(dst, src, sizeof(pel) * (lcu_pix_width >> 1));
        dst += dst_Stride;
        src += src_Stride;
    }
    src = pic_src->v + src_offset;
    dst = pic_dst->v + dst_offset;
    for (j = 0; j < (lcu_pix_height >> 1); j++)
    {
        for (int i = 0; i < (lcu_pix_width >> 1); i++)
        {
            dst[i] = src[i];
        }
        //com_mcpy(dst, src, sizeof(pel) *(lcu_pix_width >> 1));
        dst += dst_Stride;
        src += src_Stride;
    }
}

void Copy_frame_for_SAO(COM_PIC * pic_dst, COM_PIC * pic_src)
{
    int i, j;
    int src_Stride, dst_Stride;
    pel* src;
    pel* dst;
    //assert(pic_src->stride_luma == pic_dst->stride_luma);
    src_Stride = pic_src->stride_luma;
    dst_Stride = pic_dst->stride_luma;
    src = pic_src->y;
    dst = pic_dst->y;
    for (j = 0; j < pic_src->height_luma; j++)
    {
        for (i = 0; i < pic_src->width_luma; i++)
        {
            dst[i] = src[i];
        }
        dst += dst_Stride;
        src += src_Stride;
    }
    //assert(pic_src->stride_luma == pic_dst->stride_luma);
    src_Stride = pic_src->stride_chroma;
    dst_Stride = pic_dst->stride_chroma;
    src = pic_src->u;
    dst = pic_dst->u;
    for (j = 0; j < pic_src->height_chroma; j++)
    {
        for (i = 0; i < pic_src->width_chroma; i++)
        {
            dst[i] = src[i];
        }
        dst += dst_Stride;
        src += src_Stride;
    }
    src = pic_src->v;
    dst = pic_dst->v;
    for (j = 0; j < pic_src->height_chroma; j++)
    {
        for (i = 0; i < pic_src->width_chroma; i++)
        {
            dst[i] = src[i];
        }
        dst += dst_Stride;
        src += src_Stride;
    }
}

BOOL is_same_patch(s8* map_patch_idx, int mb_nr1, int mb_nr2)
{
    assert(mb_nr1 >= 0);
    assert(mb_nr2 >= 0);
    return (map_patch_idx[mb_nr1] == map_patch_idx[mb_nr2]);
}

void getSaoMergeNeighbor(COM_INFO *info, s8* map_patch_idx, int pic_width_scu, int pic_width_lcu, int lcu_pos, int mb_y, int mb_x,
                         SAOBlkParam **rec_saoBlkParam, int *MergeAvail,
                         SAOBlkParam sao_merge_param[][N_C])
{
    int mb_nr;
    int mergeup_avail, mergeleft_avail;
    SAOBlkParam *sao_left_param;
    SAOBlkParam *sao_up_param;
    mb_nr = mb_y * pic_width_scu + mb_x;
    mergeup_avail = (mb_y == 0) ? 0 : is_same_patch(map_patch_idx, mb_nr, mb_nr - pic_width_scu) ? 1 : 0;
    mergeleft_avail = (mb_x == 0) ? 0 : is_same_patch(map_patch_idx, mb_nr, mb_nr - 1) ? 1 : 0;
    if (mergeleft_avail)
    {
        sao_left_param = rec_saoBlkParam[lcu_pos - 1];
        copySAOParam_for_blk(sao_merge_param[SAO_MERGE_LEFT], sao_left_param);
    }
    if (mergeup_avail)
    {
        sao_up_param = rec_saoBlkParam[lcu_pos - pic_width_lcu];
        copySAOParam_for_blk(sao_merge_param[SAO_MERGE_ABOVE], sao_up_param);
    }
    MergeAvail[SAO_MERGE_LEFT] = mergeleft_avail;
    MergeAvail[SAO_MERGE_ABOVE] = mergeup_avail;
}

void checkBoundaryPara(COM_INFO *info, COM_MAP *map, int mb_y, int mb_x, int lcu_pix_height, int lcu_pix_width,
                       int *lcu_process_left, int *lcu_process_right, int *lcu_process_up, int *lcu_process_down, int *lcu_process_upleft,
                       int *lcu_process_upright, int *lcu_process_leftdown, int *lcu_process_rightdwon, int filter_on)
{
    int pic_width_scu = info->pic_width_in_scu;
    s8* map_patch_idx = map->map_patch_idx;
    int mb_nr = mb_y * pic_width_scu + mb_x;
    int lcu_mb_width = lcu_pix_width >> MIN_CU_LOG2;
    int lcu_mb_height = lcu_pix_height >> MIN_CU_LOG2;
    int pic_mb_height = info->pic_height >> MIN_CU_LOG2;
    int pic_mb_width = info->pic_width >> MIN_CU_LOG2;

    *lcu_process_up = (mb_y == 0) ? 0 : 1;
    *lcu_process_down = (mb_y >= pic_mb_height - lcu_mb_height) ? 0 : 1;
    *lcu_process_left = (mb_x == 0) ? 0 : 1;
    *lcu_process_right = (mb_x >= pic_mb_width - lcu_mb_width) ? 0 : 1;
    *lcu_process_upleft = (mb_x == 0 || mb_y == 0) ? 0 : 1;
    *lcu_process_upright = (mb_x >= pic_mb_width - lcu_mb_width || mb_y == 0) ? 0 : 1;
    *lcu_process_leftdown = (mb_x == 0 || mb_y >= pic_mb_height - lcu_mb_height) ? 0 : 1;
    *lcu_process_rightdwon = (mb_x >= pic_mb_width - lcu_mb_width || mb_y >= pic_mb_height - lcu_mb_height) ? 0 : 1;
    if (!filter_on)
    {
        *lcu_process_down = 0;
        *lcu_process_right = 0;
        *lcu_process_leftdown = 0;
        *lcu_process_rightdwon = 0;
    }
}

void checkBoundaryProc(COM_INFO *info, COM_MAP *map, int pix_y, int pix_x, int lcu_pix_height, int lcu_pix_width, int comp,
                       int *lcu_process_left, int *lcu_process_right, int *lcu_process_up, int *lcu_process_down,
                       int *lcu_process_upleft, int *lcu_process_upright, int *lcu_process_leftdown, int *lcu_process_rightdwon, int filter_on)
{
    int pic_width = comp ? (info->pic_width >> 1) : info->pic_width;
    int pic_height = comp ? (info->pic_height >> 1) : info->pic_height;
    int pic_mb_width = info->pic_width_in_scu;
    int mb_size_in_bit = comp ? (MIN_CU_LOG2 - 1) : MIN_CU_LOG2;
    int mb_nr_cur, mb_nr_up, mb_nr_down, mb_nr_left, mb_nr_right, mb_nr_upleft, mb_nr_upright, mb_nr_leftdown,
        mb_nr_rightdown;
    s8 *map_patch_idx = map->map_patch_idx;
    int cross_patch_flag = info->sqh.cross_patch_loop_filter;

    mb_nr_cur = (pix_y >> mb_size_in_bit) * pic_mb_width + (pix_x >> mb_size_in_bit);
    mb_nr_up = ((pix_y - 1) >> mb_size_in_bit) * pic_mb_width + (pix_x >> mb_size_in_bit);
    mb_nr_down = ((pix_y + lcu_pix_height) >> mb_size_in_bit) * pic_mb_width + (pix_x >> mb_size_in_bit);
    mb_nr_left = (pix_y >> mb_size_in_bit) * pic_mb_width + ((pix_x - 1) >> mb_size_in_bit);
    mb_nr_right = (pix_y >> mb_size_in_bit) * pic_mb_width + ((pix_x + lcu_pix_width) >> mb_size_in_bit);
    mb_nr_upleft = ((pix_y - 1) >> mb_size_in_bit) * pic_mb_width + ((pix_x - 1) >> mb_size_in_bit);
    mb_nr_upright = ((pix_y - 1) >> mb_size_in_bit) * pic_mb_width + ((pix_x + lcu_pix_width) >> mb_size_in_bit);
    mb_nr_leftdown = ((pix_y + lcu_pix_height) >> mb_size_in_bit) * pic_mb_width + ((pix_x - 1) >> mb_size_in_bit);
    mb_nr_rightdown = ((pix_y + lcu_pix_height) >> mb_size_in_bit) * pic_mb_width + ((pix_x + lcu_pix_width) >>
                      mb_size_in_bit);
    *lcu_process_up = (pix_y == 0) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_up) ? 1 :
                      cross_patch_flag;
    *lcu_process_down = (pix_y >= pic_height - lcu_pix_height) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_down) ? 1 : cross_patch_flag;
    *lcu_process_left = (pix_x == 0) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_left) ? 1 :
                        cross_patch_flag;
    *lcu_process_right = (pix_x >= pic_width - lcu_pix_width) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_right) ? 1 : cross_patch_flag;
    *lcu_process_upleft = (pix_x == 0 ||
                           pix_y == 0) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_upleft) ? 1 : cross_patch_flag;
    *lcu_process_upright = (pix_x >= pic_width - lcu_pix_width ||
                            pix_y == 0) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_upright) ? 1 : cross_patch_flag;
    *lcu_process_leftdown = (pix_x == 0 ||
                             pix_y >= pic_height - lcu_pix_height) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_leftdown)
                            ? 1 : cross_patch_flag;
    *lcu_process_rightdwon = (pix_x >= pic_width - lcu_pix_width ||
                              pix_y >= pic_height - lcu_pix_height) ? 0 : is_same_patch(map_patch_idx, mb_nr_cur, mb_nr_rightdown)
                             ? 1 : cross_patch_flag;
    if (!filter_on)
    {
        *lcu_process_down = 0;
        *lcu_process_right = 0;
        *lcu_process_leftdown = 0;
        *lcu_process_rightdwon = 0;
    }
}

void SAO_on_block(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_rec, COM_PIC  *pic_sao, SAOBlkParam *saoBlkParam, int compIdx, int pix_y, int pix_x, int lcu_pix_height,
                  int lcu_pix_width, int lcu_available_left, int lcu_available_right, int lcu_available_up, int lcu_available_down,
                  int lcu_available_upleft, int lcu_available_upright, int lcu_available_leftdown, int lcu_available_rightdwon,
                  int sample_bit_depth)
{
    int type;
    int start_x, end_x, start_y, end_y;
    int start_x_r0, end_x_r0, start_x_r, end_x_r, start_x_rn, end_x_rn;
    int x, y;
    pel *Src, *Dst;
    char leftsign, rightsign, upsign, downsign;
    long diff;
    char *signupline, *signupline1;
    int reg = 0;
    int edgetype, bandtype;
    int offset0, offset1, offset2;
    int srcStride, DstStride;
    Src = Dst = NULL;
    offset0 = offset1 = offset2 = 0;

    if ((lcu_pix_height <= 0) || (lcu_pix_width <= 0))
    {
        return;
    }

    switch (compIdx)
    {
    case Y_C:
        srcStride = pic_sao->stride_luma;
        DstStride = pic_rec->stride_luma;
        Src = pic_sao->y;
        Dst = pic_rec->y;
        break;
    case U_C:
        srcStride = pic_sao->stride_chroma;
        DstStride = pic_rec->stride_chroma;
        Src = pic_sao->u;
        Dst = pic_rec->u;
        break;
    case V_C:
        srcStride = pic_sao->stride_chroma;
        DstStride = pic_rec->stride_chroma;
        Src = pic_sao->v;
        Dst = pic_rec->v;
        break;
    default:
        srcStride = 0;
        DstStride = 0;
        Src = NULL;
        Dst = NULL;
        assert(0);
    }
    signupline = (char *)malloc((lcu_pix_width + 1) * sizeof(char));
    assert(saoBlkParam->modeIdc == SAO_MODE_NEW);
    type = saoBlkParam->typeIdc;
    switch (type)
    {
    case SAO_TYPE_EO_0:
    {
        start_y = 0;
        end_y = lcu_pix_height;
        start_x = lcu_available_left ? 0 : 1;
        end_x = lcu_available_right ? lcu_pix_width : (lcu_pix_width - 1);
        for (y = start_y; y < end_y; y++)
        {
            diff = Src[(pix_y + y) * srcStride + pix_x + start_x] - Src[(pix_y + y) * srcStride + pix_x + start_x - 1];
            leftsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            for (x = start_x; x < end_x; x++)
            {
                diff = Src[(pix_y + y) * srcStride + pix_x + x] - Src[(pix_y + y) * srcStride + pix_x + x + 1];
                rightsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                edgetype = leftsign + rightsign;
                leftsign = -rightsign;
                Dst[(pix_y + y)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                        Src[(pix_y + y) * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
            }
        }
    }
    break;
    case SAO_TYPE_EO_90:
    {
        start_x = 0;
        end_x = lcu_pix_width;
        start_y = lcu_available_up ? 0 : 1;
        end_y = lcu_available_down ? lcu_pix_height : (lcu_pix_height - 1);
        for (x = start_x; x < end_x; x++)
        {
            diff = Src[(pix_y + start_y) * srcStride + pix_x + x] - Src[(pix_y + start_y - 1) * srcStride + pix_x + x];
            upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            for (y = start_y; y < end_y; y++)
            {
                diff = Src[(pix_y + y) * srcStride + pix_x + x] - Src[(pix_y + y + 1) * srcStride + pix_x + x];
                downsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                edgetype = downsign + upsign;
                upsign = -downsign;
                Dst[(pix_y + y)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                        Src[(pix_y + y) * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
            }
        }
    }
    break;
    case SAO_TYPE_EO_135:
    {
        start_x_r0 = lcu_available_upleft ? 0 : 1;
        end_x_r0 = lcu_available_up ? (lcu_available_right ? lcu_pix_width : (lcu_pix_width - 1)) : 1;
        start_x_r = lcu_available_left ? 0 : 1;
        end_x_r = lcu_available_right ? lcu_pix_width : (lcu_pix_width - 1);
        start_x_rn = lcu_available_down ? (lcu_available_left ? 0 : 1) : (lcu_pix_width - 1);
        end_x_rn = lcu_available_rightdwon ? lcu_pix_width : (lcu_pix_width - 1);
        //init the line buffer
        for (x = start_x_r; x < end_x_r + 1; x++)
        {
            diff = Src[(pix_y + 1) * srcStride + pix_x + x] - Src[pix_y * srcStride + pix_x + x - 1];
            upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            signupline[x] = upsign;
        }
        //first row
        for (x = start_x_r0; x < end_x_r0; x++)
        {
            diff = Src[pix_y * srcStride + pix_x + x] - Src[(pix_y - 1) * srcStride + pix_x + x - 1];
            upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            edgetype = upsign - signupline[x + 1];
            Dst[pix_y * DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                                                 Src[pix_y * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
        }
        //middle rows
        for (y = 1; y < lcu_pix_height - 1; y++)
        {
            for (x = start_x_r; x < end_x_r; x++)
            {
                if (x == start_x_r)
                {
                    diff = Src[(pix_y + y) * srcStride + pix_x + x] - Src[(pix_y + y - 1) * srcStride + pix_x + x - 1];
                    upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                    signupline[x] = upsign;
                }
                diff = Src[(pix_y + y) * srcStride + pix_x + x] - Src[(pix_y + y + 1) * srcStride + pix_x + x + 1];
                downsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                edgetype = downsign + signupline[x];
                Dst[(pix_y + y)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                        Src[(pix_y + y) * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
                signupline[x] = (char)reg;
                reg = -downsign;
            }
        }
        //last row
        for (x = start_x_rn; x < end_x_rn; x++)
        {
            if (x == start_x_r)
            {
                diff = Src[(pix_y + lcu_pix_height - 1) * srcStride + pix_x + x] - Src[(pix_y + lcu_pix_height - 2) * srcStride + pix_x
                        + x - 1];
                upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                signupline[x] = upsign;
            }
            diff = Src[(pix_y + lcu_pix_height - 1) * srcStride + pix_x + x] - Src[(pix_y + lcu_pix_height) * srcStride + pix_x + x
                    + 1];
            downsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            edgetype = downsign + signupline[x];
            Dst[(pix_y + lcu_pix_height - 1)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                    Src[(pix_y + lcu_pix_height - 1) * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
        }
    }
    break;
    case SAO_TYPE_EO_45:
    {
        start_x_r0 = lcu_available_up ? (lcu_available_left ? 0 : 1) : (lcu_pix_width - 1);
        end_x_r0 = lcu_available_upright ? lcu_pix_width : (lcu_pix_width - 1);
        start_x_r = lcu_available_left ? 0 : 1;
        end_x_r = lcu_available_right ? lcu_pix_width : (lcu_pix_width - 1);
        start_x_rn = lcu_available_leftdown ? 0 : 1;
        end_x_rn = lcu_available_down ? (lcu_available_right ? lcu_pix_width : (lcu_pix_width - 1)) : 1;
        //init the line buffer
        signupline1 = signupline + 1;
        for (x = start_x_r - 1; x < end_x_r; x++)
        {
            diff = Src[(pix_y + 1) * srcStride + pix_x + x] - Src[pix_y * srcStride + pix_x + x + 1];
            upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            signupline1[x] = upsign;
        }
        //first row
        for (x = start_x_r0; x < end_x_r0; x++)
        {
            diff = Src[pix_y * srcStride + pix_x + x] - Src[(pix_y - 1) * srcStride + pix_x + x + 1];
            upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            edgetype = upsign - signupline1[x - 1];
            Dst[pix_y * DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                                                 Src[pix_y * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
        }
        //middle rows
        for (y = 1; y < lcu_pix_height - 1; y++)
        {
            for (x = start_x_r; x < end_x_r; x++)
            {
                if (x == end_x_r - 1)
                {
                    diff = Src[(pix_y + y) * srcStride + pix_x + x] - Src[(pix_y + y - 1) * srcStride + pix_x + x + 1];
                    upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                    signupline1[x] = upsign;
                }
                diff = Src[(pix_y + y) * srcStride + pix_x + x] - Src[(pix_y + y + 1) * srcStride + pix_x + x - 1];
                downsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                edgetype = downsign + signupline1[x];
                Dst[(pix_y + y)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                        Src[(pix_y + y) * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
                signupline1[x - 1] = -downsign;
            }
        }
        for (x = start_x_rn; x < end_x_rn; x++)
        {
            if (x == end_x_r - 1)
            {
                diff = Src[(pix_y + lcu_pix_height - 1) * srcStride + pix_x + x] - Src[(pix_y + lcu_pix_height - 2) * srcStride + pix_x
                        + x + 1];
                upsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
                signupline1[x] = upsign;
            }
            diff = Src[(pix_y + lcu_pix_height - 1) * srcStride + pix_x + x] - Src[(pix_y + lcu_pix_height) * srcStride + pix_x + x
                    - 1];
            downsign = diff > 0 ? 1 : (diff < 0 ? -1 : 0);
            edgetype = downsign + signupline1[x];
            Dst[(pix_y + lcu_pix_height - 1)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                    Src[(pix_y + lcu_pix_height - 1) * srcStride + pix_x + x] + saoBlkParam->offset[edgetype + 2]);
        }
    }
    break;
    case SAO_TYPE_BO:
    {
        start_x = 0;
        end_x = lcu_pix_width;
        start_y = 0;
        end_y = lcu_pix_height;
        for (x = start_x; x < end_x; x++)
        {
            for (y = start_y; y < end_y; y++)
            {
                bandtype = Src[(pix_y + y) * srcStride + pix_x + x] >> (sample_bit_depth - NUM_SAO_BO_CLASSES_IN_BIT);
                Dst[(pix_y + y)*DstStride + pix_x + x] = (pel)COM_CLIP3(0, ((1 << sample_bit_depth) - 1),
                        Src[(pix_y + y) * srcStride + pix_x + x] + saoBlkParam->offset[bandtype]);
            }
        }
    }
    break;
    default:
    {
        printf("Not a supported SAO types\n");
        assert(0);
        exit(-1);
    }
    }
    free(signupline);
}

void SAO_on_smb(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_rec, COM_PIC  *pic_sao, int pix_y, int pix_x, int lcu_pix_width, int lcu_pix_height,
                SAOBlkParam *saoBlkParam, int sample_bit_depth)
{
    int compIdx;
    int mb_y = pix_y >> MIN_CU_LOG2;
    int mb_x = pix_x >> MIN_CU_LOG2;
    int  isLeftAvail, isRightAvail, isAboveAvail, isBelowAvail, isAboveLeftAvail, isAboveRightAvail, isBelowLeftAvail,
         isBelowRightAvail;
    int lcu_pix_height_t, lcu_pix_width_t, pix_x_t, pix_y_t;
    int  isLeftProc, isRightProc, isAboveProc, isBelowProc, isAboveLeftProc, isAboveRightProc, isBelowLeftProc,
         isBelowRightProc;
    checkBoundaryPara(info, map, mb_y, mb_x, lcu_pix_height, lcu_pix_width, &isLeftAvail, &isRightAvail,
                      &isAboveAvail, &isBelowAvail, &isAboveLeftAvail, &isAboveRightAvail, &isBelowLeftAvail, &isBelowRightAvail, 1);

    if ((saoBlkParam[Y_C].modeIdc == SAO_MODE_OFF) && (saoBlkParam[U_C].modeIdc == SAO_MODE_OFF) &&
            (saoBlkParam[V_C].modeIdc == SAO_MODE_OFF))
    {
        return;
    }
    for (compIdx = 0; compIdx < N_C; compIdx++)
    {
        if (saoBlkParam[compIdx].modeIdc != SAO_MODE_OFF)
        {
            /**
            * HSUAN: AREA 5
            */
            lcu_pix_width_t = compIdx ? ((lcu_pix_width >> 1) - SAO_SHIFT_PIX_NUM) : (lcu_pix_width - SAO_SHIFT_PIX_NUM);
            lcu_pix_height_t = compIdx ? ((lcu_pix_height >> 1) - SAO_SHIFT_PIX_NUM) : (lcu_pix_height - SAO_SHIFT_PIX_NUM);
            pix_x_t = compIdx ? (pix_x >> 1) : pix_x;
            pix_y_t = compIdx ? (pix_y >> 1) : pix_y;
            checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                              &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

            //it's supposed that chroma has the same result as luma!!!
            SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                         isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                         isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            if (isAboveLeftAvail)
            {
                /**
                * HSUAN: AREA 1
                */
                lcu_pix_width_t = SAO_SHIFT_PIX_NUM;
                lcu_pix_height_t = SAO_SHIFT_PIX_NUM;
                pix_x_t = compIdx ? ((pix_x >> 1) - SAO_SHIFT_PIX_NUM) : (pix_x - SAO_SHIFT_PIX_NUM);
                pix_y_t = compIdx ? ((pix_y >> 1) - SAO_SHIFT_PIX_NUM) : (pix_y - SAO_SHIFT_PIX_NUM);
                checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                  &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                //it's supposed that chroma has the same result as luma!!!
                SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                             isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                             isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            }
            if (isLeftAvail)
            {
                /**
                * HSUAN: AREA 4
                */
                lcu_pix_width_t = SAO_SHIFT_PIX_NUM;
                lcu_pix_height_t = compIdx ? ((lcu_pix_height >> 1) - SAO_SHIFT_PIX_NUM) : (lcu_pix_height - SAO_SHIFT_PIX_NUM);
                pix_x_t = compIdx ? ((pix_x >> 1) - SAO_SHIFT_PIX_NUM) : (pix_x - SAO_SHIFT_PIX_NUM);
                pix_y_t = compIdx ? (pix_y >> 1) : pix_y;
                checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                  &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                //it's supposed that chroma has the same result as luma!!!
                SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                             isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                             isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            }
            if (isAboveAvail)
            {
                /**
                * HSUAN: AREA 2
                */
                lcu_pix_width_t = compIdx ? ((lcu_pix_width >> 1) - SAO_SHIFT_PIX_NUM) : (lcu_pix_width - SAO_SHIFT_PIX_NUM);
                lcu_pix_height_t = SAO_SHIFT_PIX_NUM;
                pix_x_t = compIdx ? (pix_x >> 1) : pix_x;
                pix_y_t = compIdx ? ((pix_y >> 1) - SAO_SHIFT_PIX_NUM) : (pix_y - SAO_SHIFT_PIX_NUM);
                checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                  &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                //it's supposed that chroma has the same result as luma!!!
                SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                             isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                             isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            }
            if (!isRightAvail)
            {
                if (isAboveAvail && !isAboveRightAvail)
                {
                    /**
                    * HSUAN: AREA 3
                    */
                    lcu_pix_width_t = SAO_SHIFT_PIX_NUM;
                    lcu_pix_height_t = SAO_SHIFT_PIX_NUM;
                    pix_x_t = compIdx ? ((pix_x >> 1) + (lcu_pix_width >> 1) - SAO_SHIFT_PIX_NUM) : (pix_x + lcu_pix_width -
                              SAO_SHIFT_PIX_NUM);
                    pix_y_t = compIdx ? ((pix_y >> 1) - SAO_SHIFT_PIX_NUM) : (pix_y - SAO_SHIFT_PIX_NUM);
                    checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                      &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                    //it's supposed that chroma has the same result as luma!!!
                    SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                                 isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                                 isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
                }
                /**
                * HSUAN: AREA 6
                */
                lcu_pix_width_t = SAO_SHIFT_PIX_NUM;
                lcu_pix_height_t = compIdx ? ((lcu_pix_height >> 1) - SAO_SHIFT_PIX_NUM) : (lcu_pix_height - SAO_SHIFT_PIX_NUM);
                pix_x_t = compIdx ? ((pix_x >> 1) + (lcu_pix_width >> 1) - SAO_SHIFT_PIX_NUM) : (pix_x + lcu_pix_width -
                          SAO_SHIFT_PIX_NUM);
                pix_y_t = compIdx ? (pix_y >> 1) : pix_y;
                checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                  &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                //it's supposed that chroma has the same result as luma!!!
                SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                             isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                             isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            }
            if (!isBelowAvail)
            {
                if (isLeftAvail)
                {
                    /**
                    * HSUAN: AREA 7
                    */
                    lcu_pix_width_t = SAO_SHIFT_PIX_NUM;
                    lcu_pix_height_t = SAO_SHIFT_PIX_NUM;
                    pix_x_t = compIdx ? ((pix_x >> 1) - SAO_SHIFT_PIX_NUM) : (pix_x - SAO_SHIFT_PIX_NUM);
                    pix_y_t = compIdx ? ((pix_y >> 1) + (lcu_pix_height >> 1) - SAO_SHIFT_PIX_NUM) : (pix_y + lcu_pix_height -
                              SAO_SHIFT_PIX_NUM);
                    assert(!isBelowLeftAvail);
                    checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                      &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                    //it's supposed that chroma has the same result as luma!!!
                    SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                                 isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                                 isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
                }
                /**
                * HSUAN: AREA 8
                */
                lcu_pix_width_t = compIdx ? ((lcu_pix_width >> 1) - SAO_SHIFT_PIX_NUM) : (lcu_pix_width - SAO_SHIFT_PIX_NUM);
                lcu_pix_height_t = SAO_SHIFT_PIX_NUM;
                pix_x_t = compIdx ? (pix_x >> 1) : pix_x;
                pix_y_t = compIdx ? ((pix_y >> 1) + (lcu_pix_height >> 1) - SAO_SHIFT_PIX_NUM) : (pix_y + lcu_pix_height -
                          SAO_SHIFT_PIX_NUM);
                checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                  &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                //it's supposed that chroma has the same result as luma!!!
                SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                             isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                             isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            }
            if (!isBelowRightAvail && !isRightAvail && !isBelowAvail)
            {
                /**
                * HSUAN: AREA 9
                */
                lcu_pix_width_t = SAO_SHIFT_PIX_NUM;
                lcu_pix_height_t = SAO_SHIFT_PIX_NUM;
                pix_x_t = compIdx ? ((pix_x >> 1) + (lcu_pix_width >> 1) - SAO_SHIFT_PIX_NUM) : (pix_x + lcu_pix_width -
                          SAO_SHIFT_PIX_NUM);
                pix_y_t = compIdx ? ((pix_y >> 1) + (lcu_pix_height >> 1) - SAO_SHIFT_PIX_NUM) : (pix_y + lcu_pix_height -
                          SAO_SHIFT_PIX_NUM);
                checkBoundaryProc(info, map,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t, compIdx, &isLeftProc,
                                  &isRightProc, &isAboveProc, &isBelowProc, &isAboveLeftProc, &isAboveRightProc, &isBelowLeftProc, &isBelowRightProc, 1);

                //it's supposed that chroma has the same result as luma!!!
                SAO_on_block(info, map, pic_rec, pic_sao, &(saoBlkParam[compIdx]), compIdx,  pix_y_t, pix_x_t, lcu_pix_height_t, lcu_pix_width_t,
                             isLeftProc /*Left*/, isRightProc/*Right*/, isAboveProc/*Above*/, isBelowProc/*Below*/, isAboveLeftProc/*AboveLeft*/,
                             isAboveRightProc/*AboveRight*/, isBelowLeftProc/*BelowLeft*/, isBelowRightProc/*BelowRight*/, sample_bit_depth);
            }
        }
    }
}

void SAOFrame(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_rec, COM_PIC  *pic_sao, SAOBlkParam **rec_saoBlkParam)
{
    int pic_pix_height = info->pic_height;
    int pic_pix_width = info->pic_width;
    int input_MaxSizeInBit = info->log2_max_cuwh;
    int bit_depth = info->bit_depth_internal;
    int pix_y, pix_x;
    int lcu_pix_height, lcu_pix_width;

    for (pix_y = 0; pix_y < pic_pix_height; pix_y += lcu_pix_height)
    {
        lcu_pix_height = min(1 << (input_MaxSizeInBit), (pic_pix_height - pix_y));
        for (pix_x = 0; pix_x < pic_pix_width; pix_x += lcu_pix_width)
        {
            int x_in_lcu = pix_x >> info->log2_max_cuwh;
            int y_in_lcu = pix_y >> info->log2_max_cuwh;
            int lcu_pos = x_in_lcu + y_in_lcu * info->pic_width_in_lcu;
            lcu_pix_width = min(1 << (input_MaxSizeInBit), (pic_pix_width - pix_x));

            SAO_on_smb(info, map, pic_rec, pic_sao, pix_y, pix_x, lcu_pix_width, lcu_pix_height, rec_saoBlkParam[lcu_pos], bit_depth);
        }
    }
}

int com_malloc_3d_SAOstatdate(SAOStatData ** **array3D, int num_SMB, int num_comp, int num_class)
{
    int i, j;
    if (((*array3D) = (SAOStatData ** *)calloc(num_SMB, sizeof(SAOStatData **))) == NULL)
    {
        printf("MALLOC FAILED: get_mem3DSAOstatdate: array3D");
        assert(0);
        exit(-1);
    }
    for (i = 0; i < num_SMB; i++)
    {
        if ((*(*array3D + i) = (SAOStatData **)calloc(num_comp, sizeof(SAOStatData *))) == NULL)
        {
            printf("MALLOC FAILED: get_mem2DSAOstatdate: array2D");
            assert(0);
            exit(-1);
        }
        for (j = 0; j < num_comp; j++)
        {
            if ((*(*(*array3D + i) + j) = (SAOStatData *)calloc(num_class, sizeof(SAOStatData))) == NULL)
            {
                printf("MALLOC FAILED: get_mem1DSAOstatdate: arrayD");
                assert(0);
                exit(-1);
            }
        }
    }
    return num_SMB * num_comp * num_class * sizeof(SAOStatData);
}

int com_malloc_2d_SAOParameter(SAOBlkParam *** array2D, int num_SMB, int num_comp)
{
    int i;
    if (((*array2D) = (SAOBlkParam **)calloc(num_SMB, sizeof(SAOBlkParam *))) == NULL)
    {
        printf("MALLOC FAILED: get_mem2DSAOBlkParam: array2D");
        assert(0);
        exit(-1);
    }
    for (i = 0; i < num_SMB; i++)
    {
        if ((*(*array2D + i) = (SAOBlkParam *)calloc(num_comp, sizeof(SAOBlkParam))) == NULL)
        {
            printf("MALLOC FAILED: get_mem1DSAOBlkParam: array1D");
            assert(0);
            exit(-1);
        }
    }
    return num_SMB * num_comp * sizeof(SAOBlkParam);
}

void com_free_3d_SAOstatdate(SAOStatData *** array3D, int num_SMB, int num_comp)
{
    int i, j;
    if (array3D)
    {
        for (i = 0; i < num_SMB; i++)
        {
            if (array3D[i])
            {
                for (j = 0; j < num_comp; j++)
                {
                    if (array3D[i][j])
                    {
                        free(array3D[i][j]);
                        array3D[i][j] = NULL;
                    }
                }
                free(array3D[i]);
                array3D[i] = NULL;
            }
        }
        //array3D is not freed totally, and this leads to some memory leak, but not a big deal
    }
}

void com_free_2d_SAOParameter(SAOBlkParam **array2D, int num_SMB)
{
    int i;
    if (array2D)
    {
        for (i = 0; i < num_SMB; i++)
        {
            if (array2D[i])
            {
                free(array2D[i]);
                array2D[i] = NULL;
            }
        }
    }
    //array2D is not freed totally, this leads to some memory leak, but not a big deal
}
