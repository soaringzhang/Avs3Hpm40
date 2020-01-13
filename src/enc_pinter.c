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

#include "enc_def.h"
#include <math.h>

/* Define the Search Range for int-pel */
#define SEARCH_RANGE_IPEL_RA               384
#define SEARCH_RANGE_IPEL_LD               64
/* Define the Search Range for sub-pel ME */
#define SEARCH_RANGE_SPEL                  3

#define MV_COST(pi, mv_bits) (u32)(((pi)->lambda_mv * mv_bits + (1 << 15)) >> 16)
#define SWAP(a, b, t) { (t) = (a); (a) = (b); (b) = (t); }

#if SIMD_AFFINE
#define CALC_EQUAL_COEFF_8PXLS(x1,x2,y1,y2,tmp0,tmp1,tmp2,tmp3,inter0,inter1,inter2,inter3,load_location)      \
{                                                                                                              \
inter0 = _mm_mul_epi32(x1, y1);                                                                                \
inter1 = _mm_mul_epi32(tmp0, tmp2);                                                                            \
inter2 = _mm_mul_epi32(x2, y2);                                                                                \
inter3 = _mm_mul_epi32(tmp1, tmp3);                                                                            \
inter2 = _mm_add_epi64(inter0, inter2);                                                                        \
inter3 = _mm_add_epi64(inter1, inter3);                                                                        \
inter0 = _mm_loadl_epi64(load_location);                                                                       \
inter3 = _mm_add_epi64(inter2, inter3);                                                                        \
inter1 = _mm_srli_si128(inter3, 8);                                                                            \
inter3 = _mm_add_epi64(inter1, inter3);                                                                        \
inter3 = _mm_add_epi64(inter0, inter3);                                                                        \
}

#define CALC_EQUAL_COEFF_4PXLS(x1,y1,tmp0,tmp1,inter0,inter1,inter2,load_location)        \
{                                                                                         \
inter0 = _mm_mul_epi32(x1, y1);                                                           \
inter1 = _mm_mul_epi32(tmp0, tmp1);                                                       \
inter2 = _mm_loadl_epi64(load_location);                                                  \
inter1 = _mm_add_epi64(inter0, inter1);                                                   \
inter0 = _mm_srli_si128(inter1, 8);                                                       \
inter0 = _mm_add_epi64(inter0, inter1);                                                   \
inter0 = _mm_add_epi64(inter2, inter0);                                                   \
}
#endif

/* q-pel search pattern */
static s8 tbl_search_pattern_qpel_8point[8][2] =
{
    {-1,  0}, { 0,  1}, { 1,  0}, { 0, -1},
    {-1,  1}, { 1,  1}, {-1, -1}, { 1, -1}
};

static const s8 tbl_diapos_partial[2][16][2] =
{
    {
        {-2, 0}, {-1, 1}, {0, 2}, {1, 1}, {2, 0}, {1, -1}, {0, -2}, {-1, -1}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}
    },
    {
        {-4, 0}, {-3, 1}, {-2, 2}, {-1, 3}, {0, 4}, {1, 3}, {2, 2}, {3, 1}, {4, 0}, {3, -1}, {2, -2}, {1, -3}, {0, -4}, {-1, -3}, {-2, -2}, {-3, -1}
    }
};

static s8 tbl_search_pattern_hpel_partial[8][2] =
{
    {-2, 0}, {-2, 2}, {0, 2}, {2, 2}, {2, 0}, {2, -2}, {0, -2}, {-2, -2}
};

__inline static u32 get_exp_golomb_bits(u32 abs_mvd)
{
    int bits = 0;
    int len_i, len_c, nn;
    /* abs(mvd) */
    nn = ((abs_mvd + 1) >> 1);
    for (len_i = 0; len_i < 16 && nn != 0; len_i++)
    {
        nn >>= 1;
    }
    len_c = (len_i << 1) + 1;
    bits += len_c;
    /* sign */
    if (abs_mvd)
    {
        bits++;
    }
    return bits;
}

static int get_mv_bits_with_mvr(int mvd_x, int mvd_y, int num_refp, int refi, u8 mvr_idx)
{
    int bits = 0;
    bits = ((mvd_x >> mvr_idx) > 2048 || (mvd_x >> mvr_idx) <= -2048) ? get_exp_golomb_bits(COM_ABS(mvd_x) >> mvr_idx) : enc_tbl_mv_bits[mvd_x >> mvr_idx];
    bits += ((mvd_y >> mvr_idx) > 2048 || (mvd_y >> mvr_idx) <= -2048) ? get_exp_golomb_bits(COM_ABS(mvd_y) >> mvr_idx) : enc_tbl_mv_bits[mvd_y >> mvr_idx];
    bits += enc_tbl_refi_bits[num_refp][refi];
    if (mvr_idx == MAX_NUM_MVR - 1)
    {
        bits += mvr_idx;
    }
    else
    {
        bits += mvr_idx + 1;
    }
    return bits;
}

static void get_range_ipel(ENC_PINTER * pi, s16 mvc[MV_D], s16 range[MV_RANGE_DIM][MV_D], int ref_idx, int lidx)
{
    int offset = pi->gop_size >> 1;

    int max_search_range = COM_CLIP3(pi->max_search_range >> 2, pi->max_search_range, (pi->max_search_range * COM_ABS(pi->ptr - (int)pi->refp[ref_idx][lidx].ptr) + offset) / pi->gop_size);
    int offset_x, offset_y, rangexy;
    int range_offset = 3 * (1 << (pi->curr_mvr - 1));
    if (pi->curr_mvr == 0)
    {
        int max_qpel_sr = pi->max_search_range >> 3;
        rangexy = COM_CLIP3(max_qpel_sr >> 2, max_qpel_sr, (max_qpel_sr * COM_ABS(pi->ptr - (int)pi->refp[ref_idx][lidx].ptr) + offset) / pi->gop_size);
    }
    else if (pi->curr_mvr == 1)
    {
        int max_hpel_sr = pi->max_search_range >> 2;
        rangexy = COM_CLIP3(max_hpel_sr >> 2, max_hpel_sr, (max_hpel_sr * COM_ABS(pi->ptr - (int)pi->refp[ref_idx][lidx].ptr) + offset) / pi->gop_size);
    }
    else if (pi->curr_mvr == 2)
    {
        int max_ipel_sr = pi->max_search_range >> 1;
        rangexy = COM_CLIP3(max_ipel_sr >> 2, max_ipel_sr, (max_ipel_sr * COM_ABS(pi->ptr - (int)pi->refp[ref_idx][lidx].ptr) + offset) / pi->gop_size);
    }
    else
    {
        int max_spel_sr = pi->max_search_range;
        rangexy = COM_CLIP3(max_spel_sr >> 2, max_spel_sr, (max_spel_sr * COM_ABS(pi->ptr - (int)pi->refp[ref_idx][lidx].ptr) + offset) / pi->gop_size);
    }

    assert(rangexy <= max_search_range);

    if (pi->curr_mvr > 0)
    {
        if ((abs(mvc[MV_X] - pi->max_imv[MV_X]) + range_offset) > rangexy)
        {
            offset_x = rangexy;
        }
        else
        {
            offset_x = abs(mvc[MV_X] - pi->max_imv[MV_X]) + range_offset;
        }
        if ((abs(mvc[MV_Y] - pi->max_imv[MV_Y]) + range_offset) > rangexy)
        {
            offset_y = rangexy;
        }
        else
        {
            offset_y = abs(mvc[MV_Y] - pi->max_imv[MV_Y]) + range_offset;
        }
    }
    else
    {
        offset_x = rangexy;
        offset_y = rangexy;
    }
    /* define search range for int-pel search and clip it if needs */
    range[MV_RANGE_MIN][MV_X] = COM_CLIP3(pi->min_mv_offset[MV_X], pi->max_mv_offset[MV_X], mvc[MV_X] - (s16)offset_x);
    range[MV_RANGE_MAX][MV_X] = COM_CLIP3(pi->min_mv_offset[MV_X], pi->max_mv_offset[MV_X], mvc[MV_X] + (s16)offset_x);
    range[MV_RANGE_MIN][MV_Y] = COM_CLIP3(pi->min_mv_offset[MV_Y], pi->max_mv_offset[MV_Y], mvc[MV_Y] - (s16)offset_y);
    range[MV_RANGE_MAX][MV_Y] = COM_CLIP3(pi->min_mv_offset[MV_Y], pi->max_mv_offset[MV_Y], mvc[MV_Y] + (s16)offset_y);

    com_assert(range[MV_RANGE_MIN][MV_X] <= range[MV_RANGE_MAX][MV_X]);
    com_assert(range[MV_RANGE_MIN][MV_Y] <= range[MV_RANGE_MAX][MV_Y]);
}

#if TB_SPLIT_EXT
static u32 calc_sad_16b(int pu_w, int pu_h, void *src1, void *src2, int s_src1, int s_src2, int bit_depth)
{
    u32 cost = 0;
    int num_seg_in_pu_w = 1, num_seg_in_pu_h = 1;
    int seg_w_log2 = com_tbl_log2[pu_w];
    int seg_h_log2 = com_tbl_log2[pu_h];
    s16 *src1_seg, *src2_seg;
    s16* s1 = (s16 *)src1;
    s16* s2 = (s16 *)src2;

    if (seg_w_log2 == -1)
    {
        num_seg_in_pu_w = 3;
        seg_w_log2 = (pu_w == 48) ? 4 : (pu_w == 24 ? 3 : 2);
    }

    if (seg_h_log2 == -1)
    {
        num_seg_in_pu_h = 3;
        seg_h_log2 = (pu_h == 48) ? 4 : (pu_h == 24 ? 3 : 2);
    }

    if (num_seg_in_pu_w == 1 && num_seg_in_pu_h == 1)
    {
        cost += enc_sad_16b(seg_w_log2, seg_h_log2, s1, s2, s_src1, s_src2, bit_depth);
        return cost;
    }

    for (int j = 0; j < num_seg_in_pu_h; j++)
    {
        for (int i = 0; i < num_seg_in_pu_w; i++)
        {
            src1_seg = s1 + (1 << seg_w_log2) * i + (1 << seg_h_log2) * j * s_src1;
            src2_seg = s2 + (1 << seg_w_log2) * i + (1 << seg_h_log2) * j * s_src2;
            cost += enc_sad_16b(seg_w_log2, seg_h_log2, src1_seg, src2_seg, s_src1, s_src2, bit_depth);
        }
    }
    return cost;
}

static u32 calc_satd_16b(int pu_w, int pu_h, void *src1, void *src2, int s_src1, int s_src2, int bit_depth)
{
    u32 cost = 0;
    int num_seg_in_pu_w = 1, num_seg_in_pu_h = 1;
    int seg_w_log2 = com_tbl_log2[pu_w];
    int seg_h_log2 = com_tbl_log2[pu_h];
    s16 *src1_seg, *src2_seg;
    s16* s1 = (s16 *)src1;
    s16* s2 = (s16 *)src2;

    if (seg_w_log2 == -1)
    {
        num_seg_in_pu_w = 3;
        seg_w_log2 = (pu_w == 48) ? 4 : (pu_w == 24 ? 3 : 2);
    }

    if (seg_h_log2 == -1)
    {
        num_seg_in_pu_h = 3;
        seg_h_log2 = (pu_h == 48) ? 4 : (pu_h == 24 ? 3 : 2);
    }

    if (num_seg_in_pu_w == 1 && num_seg_in_pu_h == 1)
    {
        cost += enc_satd_16b(seg_w_log2, seg_h_log2, s1, s2, s_src1, s_src2, bit_depth);
        return cost;
    }

    for (int j = 0; j < num_seg_in_pu_h; j++)
    {
        for (int i = 0; i < num_seg_in_pu_w; i++)
        {
            src1_seg = s1 + (1 << seg_w_log2) * i + (1 << seg_h_log2) * j * s_src1;
            src2_seg = s2 + (1 << seg_w_log2) * i + (1 << seg_h_log2) * j * s_src2;
            cost += enc_satd_16b(seg_w_log2, seg_h_log2, src1_seg, src2_seg, s_src1, s_src2, bit_depth);
        }
    }
    return cost;
}
#endif

/* Get original dummy buffer for bi prediction */
static void get_org_bi(pel * org, pel * pred, int s_o, int cu_width, int cu_height, s16 * org_bi, int s_pred, int x_offset, int y_offset)
{
    int i, j;
    //it's safer not to change input pointers
    pel *org2 = org + x_offset + y_offset * s_o;
    pel *pred2 = pred + x_offset + y_offset * s_pred;
    s16 * org_bi2 = org_bi + x_offset + y_offset * s_pred;

    for (j = 0; j < cu_height; j++)
    {
        for (i = 0; i < cu_width; i++)
        {
            org_bi2[i] = ((s16)(org2[i]) << 1) - (s16)pred2[i];
        }
        org2 += s_o;
        pred2 += s_pred;
        org_bi2 += s_pred;
    }
}

static u32 me_raster(ENC_PINTER * pi, int x, int y, int w, int h, s8 refi, int lidx, s16 range[MV_RANGE_DIM][MV_D], s16 gmvp[MV_D], s16 mv[MV_D])
{
    int bit_depth = pi->bit_depth;
    COM_PIC *ref_pic;
    pel      *org, *ref;
    u32        mv_bits, best_mv_bits;
    u32       cost_best, cost;
    int       i, j;
    s16       mv_x, mv_y;
    s32       search_step_x = max(RASTER_SEARCH_STEP, (w >> 1)); /* Adaptive step size : Half of CU dimension */
    s32       search_step_y = max(RASTER_SEARCH_STEP, (h >> 1)); /* Adaptive step size : Half of CU dimension */
    s16       center_mv[MV_D];
    s32       search_step;
    search_step_x = search_step_y = max(RASTER_SEARCH_STEP, min(w >> 1, h >> 1));
    org = pi->Yuv_org[Y_C] + y * pi->stride_org[Y_C] + x;
    ref_pic = pi->refp[refi][lidx].pic;
    best_mv_bits = 0;
    cost_best = COM_UINT32_MAX;
#if MULTI_REF_ME_STEP
    for (i = range[MV_RANGE_MIN][MV_Y]; i <= range[MV_RANGE_MAX][MV_Y]; i += (search_step_y * (refi + 1)))
    {
        for (j = range[MV_RANGE_MIN][MV_X]; j <= range[MV_RANGE_MAX][MV_X]; j += (search_step_x * (refi + 1)))
#else
    for (i = range[MV_RANGE_MIN][MV_Y]; i <= range[MV_RANGE_MAX][MV_Y]; i += search_step_y)
    {
        for (j = range[MV_RANGE_MIN][MV_X]; j <= range[MV_RANGE_MAX][MV_X]; j += search_step_x)
#endif
        {
            mv_x = (s16)j;
            mv_y = (s16)i;
            if (pi->curr_mvr > 2)
            {
                com_mv_rounding_s16(mv_x, mv_y, &mv_x, &mv_y, pi->curr_mvr - 2, pi->curr_mvr - 2);
            }
            /* get MVD bits */
            mv_bits = (u32)get_mv_bits_with_mvr((mv_x << 2) - gmvp[MV_X], (mv_y << 2) - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
            mv_bits += 2; // add inter_dir bits, raster me is only performed for uni-prediction
            /* get MVD cost_best */
            cost = MV_COST(pi, mv_bits);
            ref = ref_pic->y + mv_x + mv_y * ref_pic->stride_luma;
            /* get sad */
            cost += calc_sad_16b(w, h, org, ref, pi->stride_org[Y_C], ref_pic->stride_luma, bit_depth);
            /* check if motion cost_best is less than minimum cost_best */
            if (cost < cost_best)
            {
                mv[MV_X] = ((mv_x - (s16)x) << 2);
                mv[MV_Y] = ((mv_y - (s16)y) << 2);
                cost_best = cost;
                best_mv_bits = mv_bits;
            }
        }
    }
    /* Grid search around best mv for all dyadic step sizes till integer pel */
#if MULTI_REF_ME_STEP
    search_step = (refi + 1) * max(search_step_x, search_step_y) >> 1;
#else
    search_step = max(search_step_x, search_step_y) >> 1;
#endif
    while (search_step > 0)
    {
        center_mv[MV_X] = mv[MV_X];
        center_mv[MV_Y] = mv[MV_Y];
        for (i = -search_step; i <= search_step; i += search_step)
        {
            for (j = -search_step; j <= search_step; j += search_step)
            {
                mv_x = (center_mv[MV_X] >> 2) + (s16)x + (s16)j;
                mv_y = (center_mv[MV_Y] >> 2) + (s16)y + (s16)i;
                if ((mv_x < range[MV_RANGE_MIN][MV_X]) || (mv_x > range[MV_RANGE_MAX][MV_X]))
                    continue;
                if ((mv_y < range[MV_RANGE_MIN][MV_Y]) || (mv_y > range[MV_RANGE_MAX][MV_Y]))
                    continue;

                if (pi->curr_mvr > 2)
                {
                    com_mv_rounding_s16(mv_x, mv_y, &mv_x, &mv_y, pi->curr_mvr - 2, pi->curr_mvr - 2);
                }
                /* get MVD bits */
                mv_bits = get_mv_bits_with_mvr((mv_x << 2) - gmvp[MV_X], (mv_y << 2) - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
                mv_bits += 2; // add inter_dir bits
                /* get MVD cost_best */
                cost = MV_COST(pi, mv_bits);
                ref = ref_pic->y + mv_x + mv_y * ref_pic->stride_luma;
                /* get sad */
                cost += calc_sad_16b(w, h, org, ref, pi->stride_org[Y_C], ref_pic->stride_luma, bit_depth);
                /* check if motion cost_best is less than minimum cost_best */
                if (cost < cost_best)
                {
                    mv[MV_X] = ((mv_x - (s16)x) << 2);
                    mv[MV_Y] = ((mv_y - (s16)y) << 2);
                    cost_best = cost;
                    best_mv_bits = mv_bits;
                }
            }
        }
        /* Halve the step size */
        search_step >>= 1;
    }
    if (best_mv_bits > 0)
    {
        pi->mot_bits[lidx] = best_mv_bits;
    }
    return cost_best;
}

static u32 me_ipel_diamond(ENC_PINTER *pi, int x, int y, int w, int h, int cu_x, int cu_y, int cu_stride, s8 refi, int lidx, s16 range[MV_RANGE_DIM][MV_D], s16 gmvp[MV_D], s16 mvi[MV_D], s16 mv[MV_D], int bi, int *beststep, int faststep)
{
    int bit_depth = pi->bit_depth;
    COM_PIC      *ref_pic;
    pel           *org, *ref;
    u32            cost, cost_best = COM_UINT32_MAX;
    int            mv_bits, best_mv_bits = 0;
    s16            mv_x, mv_y, mv_best_x, mv_best_y;
    int            lidx_r = (lidx == REFP_0) ? REFP_1 : REFP_0;
    s16           *org_bi = pi->org_bi + (x - cu_x) + (y - cu_y) * cu_stride;
    s16            mvc[MV_D];
    int            step = 0, i, j;
    int            min_cmv_x, min_cmv_y, max_cmv_x, max_cmv_y;
    s16            imv_x, imv_y;
    int            mvsize = 1;

    org = pi->Yuv_org[Y_C] + y * pi->stride_org[Y_C] + x;
    ref_pic = pi->refp[refi][lidx].pic;
    mv_best_x = (mvi[MV_X] >> 2);
    mv_best_y = (mvi[MV_Y] >> 2);
    mv_best_x = COM_CLIP3(pi->min_mv_offset[MV_X], pi->max_mv_offset[MV_X], mv_best_x);
    mv_best_y = COM_CLIP3(pi->min_mv_offset[MV_Y], pi->max_mv_offset[MV_Y], mv_best_y);

    if (pi->curr_mvr > 2)
    {
        com_mv_rounding_s16(mv_best_x, mv_best_y, &mv_best_x, &mv_best_y, pi->curr_mvr - 2, pi->curr_mvr - 2);
    }

    imv_x = mv_best_x;
    imv_y = mv_best_y;
    while (1)
    {
        if (step <= 2)
        {
            if (pi->curr_mvr > 2)
            {
                min_cmv_x = (mv_best_x <= range[MV_RANGE_MIN][MV_X]) ? mv_best_x : mv_best_x - ((bi ? (BI_STEP - 2) : 1) << (pi->curr_mvr - 1));
                min_cmv_y = (mv_best_y <= range[MV_RANGE_MIN][MV_Y]) ? mv_best_y : mv_best_y - ((bi ? (BI_STEP - 2) : 1) << (pi->curr_mvr - 1));
                max_cmv_x = (mv_best_x >= range[MV_RANGE_MAX][MV_X]) ? mv_best_x : mv_best_x + ((bi ? (BI_STEP - 2) : 1) << (pi->curr_mvr - 1));
                max_cmv_y = (mv_best_y >= range[MV_RANGE_MAX][MV_Y]) ? mv_best_y : mv_best_y + ((bi ? (BI_STEP - 2) : 1) << (pi->curr_mvr - 1));
            }
            else
            {
                min_cmv_x = (mv_best_x <= range[MV_RANGE_MIN][MV_X]) ? mv_best_x : mv_best_x - (bi ? BI_STEP : 2);
                min_cmv_y = (mv_best_y <= range[MV_RANGE_MIN][MV_Y]) ? mv_best_y : mv_best_y - (bi ? BI_STEP : 2);
                max_cmv_x = (mv_best_x >= range[MV_RANGE_MAX][MV_X]) ? mv_best_x : mv_best_x + (bi ? BI_STEP : 2);
                max_cmv_y = (mv_best_y >= range[MV_RANGE_MAX][MV_Y]) ? mv_best_y : mv_best_y + (bi ? BI_STEP : 2);
            }
            if (pi->curr_mvr > 2)
            {
                mvsize = 1 << (pi->curr_mvr - 2);
            }
            else
            {
                mvsize = 1;
            }
            for (i = min_cmv_y; i <= max_cmv_y; i += mvsize)
            {
                for (j = min_cmv_x; j <= max_cmv_x; j += mvsize)
                {
                    mv_x = (s16)j;
                    mv_y = (s16)i;
                    if (mv_x > range[MV_RANGE_MAX][MV_X] ||
                            mv_x < range[MV_RANGE_MIN][MV_X] ||
                            mv_y > range[MV_RANGE_MAX][MV_Y] ||
                            mv_y < range[MV_RANGE_MIN][MV_Y])
                    {
                        cost = COM_UINT32_MAX;
                    }
                    else
                    {
                        /* get MVD bits */
                        mv_bits = get_mv_bits_with_mvr((mv_x << 2) - gmvp[MV_X], (mv_y << 2) - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
                        mv_bits += bi ? 1 : 2; // add inter_dir bits
                        if (bi)
                        {
                            mv_bits += pi->mot_bits[lidx_r];
                        }
                        /* get MVD cost_best */
                        cost = MV_COST(pi, mv_bits);
                        ref = ref_pic->y + mv_x + mv_y * ref_pic->stride_luma;
                        if (bi)
                        {
                            /* get sad */
                            cost += calc_sad_16b(w, h, org_bi, ref, cu_stride, ref_pic->stride_luma, bit_depth) >> 1;
                        }
                        else
                        {
                            /* get sad */
                            cost += calc_sad_16b(w, h, org, ref, pi->stride_org[Y_C], ref_pic->stride_luma, bit_depth);
                        }
                        /* check if motion cost_best is less than minimum cost_best */
                        if (cost < cost_best)
                        {
                            mv_best_x = mv_x;
                            mv_best_y = mv_y;
                            *beststep = 2;
                            cost_best = cost;
                            best_mv_bits = mv_bits;
                        }
                    }
                }
            }
            mvc[MV_X] = mv_best_x;
            mvc[MV_Y] = mv_best_y;
            get_range_ipel(pi, mvc, range, refi, lidx);
            step += 2;
        }
        else
        {
            int meidx = step > 8 ? 2 : 1;
            int multi = pi->curr_mvr > 2 ? (step * (1 << (pi->curr_mvr - 2))) : step;
            for (i = 0; i < 16; i++)
            {
                if (meidx == 1 && i > 8)
                {
                    continue;
                }
                if ((step == 4) && (i == 1 || i == 3 || i == 5 || i == 7))
                {
                    continue;
                }

                mv_x = imv_x + ((s16)(multi >> meidx) * tbl_diapos_partial[meidx - 1][i][MV_X]);
                mv_y = imv_y + ((s16)(multi >> meidx) * tbl_diapos_partial[meidx - 1][i][MV_Y]);

                if (mv_x > range[MV_RANGE_MAX][MV_X] ||
                        mv_x < range[MV_RANGE_MIN][MV_X] ||
                        mv_y > range[MV_RANGE_MAX][MV_Y] ||
                        mv_y < range[MV_RANGE_MIN][MV_Y])
                {
                    cost = COM_UINT32_MAX;
                }
                else
                {
                    /* get MVD bits */
                    mv_bits = get_mv_bits_with_mvr((mv_x << 2) - gmvp[MV_X], (mv_y << 2) - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
                    mv_bits += bi ? 1 : 2; // add inter_dir bits
                    if (bi)
                    {
                        mv_bits += pi->mot_bits[lidx_r];
                    }
                    /* get MVD cost_best */
                    cost = MV_COST(pi, mv_bits);
                    ref = ref_pic->y + mv_x + mv_y * ref_pic->stride_luma;
                    if (bi)
                    {
                        /* get sad */
                        cost += calc_sad_16b(w, h, org_bi, ref, cu_stride, ref_pic->stride_luma, bit_depth) >> 1;
                    }
                    else
                    {
                        /* get sad */
                        cost += calc_sad_16b(w, h, org, ref, pi->stride_org[Y_C], ref_pic->stride_luma, bit_depth);
                    }
                    /* check if motion cost_best is less than minimum cost_best */
                    if (cost < cost_best)
                    {
                        mv_best_x = mv_x;
                        mv_best_y = mv_y;
                        *beststep = step;
                        cost_best = cost;
                        best_mv_bits = mv_bits;
                    }
                }
            }
        }
        if (step >= faststep)
        {
            break;
        }
        if (bi)
        {
            break;
        }
        step <<= 1;
    }
    /* set best MV */
    mv[MV_X] = ((mv_best_x - (s16)x) << 2);
    mv[MV_Y] = ((mv_best_y - (s16)y) << 2);
    if (!bi && best_mv_bits > 0)
    {
        pi->mot_bits[lidx] = best_mv_bits;
    }
    return cost_best;
}

static u32 me_spel_pattern(ENC_PINTER *pi, int x, int y, int w, int h, int cu_x, int cu_y, int cu_stride, s8 refi, int lidx, s16 gmvp[MV_D], s16 mvi[MV_D], s16 mv[MV_D], int bi)
{
    int bit_depth = pi->bit_depth;
    pel     *org, *ref, *pred;
    s16     *org_bi;
    u32      cost, cost_best = COM_UINT32_MAX;
    s16      mv_x, mv_y, cx, cy;
    int      lidx_r = (lidx == REFP_0) ? REFP_1 : REFP_0;
    int      i, mv_bits, s_org, s_ref, best_mv_bits;
    s_org = pi->stride_org[Y_C];
    org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];
    s_ref = pi->refp[refi][lidx].pic->stride_luma;
    ref = pi->refp[refi][lidx].pic->y;
    org_bi = pi->org_bi + (x - cu_x) + (y - cu_y) * cu_stride;
    pred = pi->pred_buf + (x - cu_x) + (y - cu_y) * cu_stride;
    best_mv_bits = 0;
    /* make MV to be global coordinate */
    cx = mvi[MV_X] + ((s16)x << 2);
    cy = mvi[MV_Y] + ((s16)y << 2);
    /* intial value */
    mv[MV_X] = mvi[MV_X];
    mv[MV_Y] = mvi[MV_Y];

    // get initial satd cost as cost_best
    mv_bits = get_mv_bits_with_mvr(cx - gmvp[MV_X], cy - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
    mv_bits += bi ? 1 : 2; // add inter_dir bits
    if (bi)
    {
        mv_bits += pi->mot_bits[lidx_r];
    }
    /* get MVD cost_best */
    cost_best = MV_COST(pi, mv_bits);
    pel * ref_tmp = ref + (cx >> 2) + (cy >> 2)* s_ref;
    if (bi)
    {
        /* get satd */
        cost_best += calc_satd_16b(w, h, org_bi, ref_tmp, cu_stride, s_ref, bit_depth) >> 1;
    }
    else
    {
        /* get satd */
        cost_best += calc_satd_16b(w, h, org, ref_tmp, s_org, s_ref, bit_depth);
    }

    /* search upto hpel-level from here */
    /* search of large diamond pattern */
    for (i = 0; i < pi->search_pattern_hpel_cnt; i++)
    {
        mv_x = cx + pi->search_pattern_hpel[i][0];
        mv_y = cy + pi->search_pattern_hpel[i][1];
        /* get MVD bits */
        mv_bits = get_mv_bits_with_mvr(mv_x - gmvp[MV_X], mv_y - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
        mv_bits += bi ? 1 : 2; // add inter_dir bits
        if (bi)
        {
            mv_bits += pi->mot_bits[lidx_r];
        }
        /* get MVD cost_best */
        cost = MV_COST(pi, mv_bits);
        /* get the interpolated(predicted) image */
        com_mc_l(mv_x, mv_y, ref, mv_x, mv_y, s_ref, cu_stride, pred, w, h, bit_depth);

        if (bi)
        {
            /* get sad */
            cost += calc_satd_16b(w, h, org_bi, pred, cu_stride, cu_stride, bit_depth) >> 1;
        }
        else
        {
            /* get sad */
            cost += calc_satd_16b(w, h, org, pred, s_org, cu_stride, bit_depth);
        }
        /* check if motion cost_best is less than minimum cost_best */
        if (cost < cost_best)
        {
            mv[MV_X] = mv_x - ((s16)x << 2);
            mv[MV_Y] = mv_y - ((s16)y << 2);
            cost_best = cost;
            best_mv_bits = mv_bits;
        }
    }

    /* search qpel-level motion vector*/
    if (pi->me_level > ME_LEV_HPEL && pi->curr_mvr == 0)
    {
        /* make MV to be absolute coordinate */
        cx = mv[MV_X] + ((s16)x << 2);
        cy = mv[MV_Y] + ((s16)y << 2);
        for (i = 0; i < pi->search_pattern_qpel_cnt; i++)
        {
            mv_x = cx + pi->search_pattern_qpel[i][0];
            mv_y = cy + pi->search_pattern_qpel[i][1];
            /* get MVD bits */
            mv_bits = get_mv_bits_with_mvr(mv_x - gmvp[MV_X], mv_y - gmvp[MV_Y], pi->num_refp, refi, pi->curr_mvr);
            mv_bits += bi ? 1 : 2; // add inter_dir bits
            if (bi)
            {
                mv_bits += pi->mot_bits[lidx_r];
            }
            /* get MVD cost_best */
            cost = MV_COST(pi, mv_bits);
            /* get the interpolated(predicted) image */
            com_mc_l(mv_x, mv_y, ref, mv_x, mv_y, s_ref, cu_stride, pred, w, h, bit_depth);

            if (bi)
            {
                /* get sad */
                cost += calc_satd_16b(w, h, org_bi, pred, cu_stride, cu_stride, bit_depth) >> 1;
            }
            else
            {
                /* get sad */
                cost += calc_satd_16b(w, h, org, pred, s_org, cu_stride, bit_depth);
            }
            /* check if motion cost_best is less than minimum cost_best */
            if (cost < cost_best)
            {
                mv[MV_X] = mv_x - ((s16)x << 2);
                mv[MV_Y] = mv_y - ((s16)y << 2);
                cost_best = cost;
                best_mv_bits = mv_bits;
            }
        }
    }
    if (!bi && best_mv_bits > 0)
    {
        pi->mot_bits[lidx] = best_mv_bits;
    }
    return cost_best;
}

static u32 pinter_me_epzs(ENC_PINTER * pi, int x, int y, int w, int h, int cu_x, int cu_y, int cu_stride, s8 * refi, int lidx, s16 mvp[MV_D], s16 mv[MV_D], int bi)
{
    s16 mvc[MV_D];  /* MV center for search */
    s16 gmvp[MV_D]; /* MVP in frame coordinate */
    s16 range[MV_RANGE_DIM][MV_D]; /* search range after clipping */
    s16 mvi[MV_D];
    s16 mvt[MV_D];
    u32 cost, cost_best = COM_UINT32_MAX;
    s8 ref_idx = 0;  /* reference buffer index */
    int tmpstep = 0;
    int beststep = 0;
    gmvp[MV_X] = mvp[MV_X] + ((s16)x << 2);
    gmvp[MV_Y] = mvp[MV_Y] + ((s16)y << 2);

    if (bi)
    {
        mvi[MV_X] = mv[MV_X] + ((s16)x << 2);
        mvi[MV_Y] = mv[MV_Y] + ((s16)y << 2);
        mvc[MV_X] = (s16)x + (mv[MV_X] >> 2);
        mvc[MV_Y] = (s16)y + (mv[MV_Y] >> 2);
    }
    else
    {
        mvi[MV_X] = mvp[MV_X] + ((s16)x << 2);
        mvi[MV_Y] = mvp[MV_Y] + ((s16)y << 2);
        mvc[MV_X] = (s16)x + (mvp[MV_X] >> 2);
        mvc[MV_Y] = (s16)y + (mvp[MV_Y] >> 2);
    }

    ref_idx = *refi;
    mvc[MV_X] = COM_CLIP3(pi->min_mv_offset[MV_X], pi->max_mv_offset[MV_X], mvc[MV_X]);
    mvc[MV_Y] = COM_CLIP3(pi->min_mv_offset[MV_Y], pi->max_mv_offset[MV_Y], mvc[MV_Y]);

    get_range_ipel(pi, mvc, range, ref_idx, lidx);
    cost = me_ipel_diamond(pi, x, y, w, h, cu_x, cu_y, cu_stride, ref_idx, lidx, range, gmvp, mvi, mvt, bi, &tmpstep, MAX_FIRST_SEARCH_STEP);
    if (cost < cost_best)
    {
        cost_best = cost;
        mv[MV_X] = mvt[MV_X];
        mv[MV_Y] = mvt[MV_Y];
        if (abs(mvp[MV_X] - mv[MV_X]) < 2 && abs(mvp[MV_Y] - mv[MV_Y]) < 2)
        {
            beststep = 0;
        }
        else
        {
            beststep = tmpstep;
        }
    }
    if (!bi && beststep > RASTER_SEARCH_THD)
    {
        cost = me_raster(pi, x, y, w, h, ref_idx, lidx, range, gmvp, mvt);
        if (cost < cost_best)
        {
            beststep = RASTER_SEARCH_THD;
            cost_best = cost;
            mv[MV_X] = mvt[MV_X];
            mv[MV_Y] = mvt[MV_Y];
        }
    }
    if (!bi && beststep > REFINE_SEARCH_THD)
    {
        mvc[MV_X] = (s16)x + (mv[MV_X] >> 2);
        mvc[MV_Y] = (s16)y + (mv[MV_Y] >> 2);
        get_range_ipel(pi, mvc, range, ref_idx, lidx);
        mvi[MV_X] = mv[MV_X] + ((s16)x << 2);
        mvi[MV_Y] = mv[MV_Y] + ((s16)y << 2);
        cost = me_ipel_diamond(pi, x, y, w, h, cu_x, cu_y, cu_stride, ref_idx, lidx, range, gmvp, mvi, mvt, bi, &tmpstep, MAX_REFINE_SEARCH_STEP);
        if (cost < cost_best)
        {
            cost_best = cost;
            mv[MV_X] = mvt[MV_X];
            mv[MV_Y] = mvt[MV_Y];
        }
    }

    if (pi->me_level > ME_LEV_IPEL && (pi->curr_mvr == 0 || pi->curr_mvr == 1))
    {
        /* sub-pel ME */
        cost = me_spel_pattern(pi, x, y, w, h, cu_x, cu_y, cu_stride, ref_idx, lidx, gmvp, mv, mvt, bi);
        cost_best = cost;
        mv[MV_X] = mvt[MV_X];
        mv[MV_Y] = mvt[MV_Y];
    }
    return cost_best;
}

static void make_cand_list(ENC_CORE *core, ENC_CTX *ctx, int *mode_list, double *cost_list, int num_cands_woUMVE, int num_cands_all, int num_rdo, s16 pmv_skip_cand[MAX_SKIP_NUM][REFP_NUM][MV_D],  s8 refi_skip_cand[MAX_SKIP_NUM][REFP_NUM])
{
    ENC_PINTER *pi = &ctx->pinter;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int bit_depth = ctx->info.bit_depth_internal;

    int skip_idx, i;
    //s16 mvp[REFP_NUM][MV_D];
    //s8 refi[REFP_NUM];
    u32 cy/*, cu, cv*/;
    double cost_y;

    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width  = mod_info_curr->cu_width;
    int cu_height = mod_info_curr->cu_height;
    int cu_width_log2  = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;

    pel* y_org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];
    pel* u_org = pi->Yuv_org[U_C] + (x >> 1) + ((y >> 1) * pi->stride_org[U_C]);
    pel* v_org = pi->Yuv_org[V_C] + (x >> 1) + ((y >> 1) * pi->stride_org[V_C]);

    mod_info_curr->cu_mode = MODE_SKIP;
    for (i = 0; i < num_rdo; i++)
    {
        mode_list[i] = 0;
        cost_list[i] = MAX_COST;
    }

    for (skip_idx = 0; skip_idx < num_cands_all; skip_idx++)
    {
        int bit_cnt, shift = 0;

        mod_info_curr->mv[REFP_0][MV_X] = pmv_skip_cand[skip_idx][REFP_0][MV_X];
        mod_info_curr->mv[REFP_0][MV_Y] = pmv_skip_cand[skip_idx][REFP_0][MV_Y];
        mod_info_curr->mv[REFP_1][MV_X] = pmv_skip_cand[skip_idx][REFP_1][MV_X];
        mod_info_curr->mv[REFP_1][MV_Y] = pmv_skip_cand[skip_idx][REFP_1][MV_Y];
        mod_info_curr->refi[REFP_0] = refi_skip_cand[skip_idx][REFP_0];
        mod_info_curr->refi[REFP_1] = refi_skip_cand[skip_idx][REFP_1];

        if (!REFI_IS_VALID(mod_info_curr->refi[REFP_0]) && !REFI_IS_VALID(mod_info_curr->refi[REFP_1]))
        {
            continue;
        }

        if (skip_idx < num_cands_woUMVE)
        {
            mod_info_curr->umve_flag = 0;
            mod_info_curr->skip_idx = skip_idx;
        }
        else
        {
            mod_info_curr->umve_flag = 1;
            mod_info_curr->umve_idx = skip_idx - num_cands_woUMVE;
        }

        // skip index 1 and 2 for P slice
        if ((ctx->info.pic_header.slice_type == SLICE_P) && (mod_info_curr->skip_idx == 1 || mod_info_curr->skip_idx == 2) && (mod_info_curr->umve_flag == 0))
        {
            continue;
        }

        com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, CHANNEL_L, bit_depth);
        //com_mc(x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, refi, mvp, pi->refp, mod_info_curr->pred, cu_width, CHANNEL_L, bit_depth);

        cy = calc_satd_16b(cu_width, cu_height, y_org, mod_info_curr->pred[Y_C], pi->stride_org[Y_C], cu_width, bit_depth);
        cost_y = (double)cy;

        SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
        bit_cnt = enc_get_bit_number(&core->s_temp_run);

        enc_bit_est_inter(ctx, core, ctx->info.pic_header.slice_type);

        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
        SBAC_STORE(core->s_temp_best, core->s_temp_run);

        cost_y += RATE_TO_COST_SQRT_LAMBDA(ctx->sqrt_lambda[0], bit_cnt);
        while (shift < num_rdo && cost_y < cost_list[num_rdo - 1 - shift])
        {
            shift++;
        }
        if (shift != 0)
        {
            for (i = 1; i < shift; i++)
            {
                mode_list[num_rdo - i] = mode_list[num_rdo - 1 - i];
                cost_list[num_rdo - i] = cost_list[num_rdo - 1 - i];
            }
            mode_list[num_rdo - shift] = skip_idx;
            cost_list[num_rdo - shift] = cost_y;
        }
    }

}

void check_best_mode(ENC_CORE *core, ENC_PINTER *pi, const double cost_curr, double *cost_best)
{
    COM_MODE *bst_info = &core->mod_info_best;
    COM_MODE *cur_info = &core->mod_info_curr;
#if PRINT_CU_LEVEL_2
    if (cur_info->cu_mode != MODE_INTRA)
        printf("\nMODE %d skipIdx %2d UMVEIdx %2d mvrIdx %2d cost_curr %10.1f ", cur_info->cu_mode, cur_info->skip_idx, cur_info->umve_idx, cur_info->mvr_idx, cost_curr);
    double val = 2786890.7;
    if (cost_curr - val > -0.1 && cost_curr - val < 0.1)
    {
        int a = 0;
    }
#endif
    if (cost_curr < *cost_best)
    {
        int j, lidx;
        int cu_width_log2 = cur_info->cu_width_log2;
        int cu_height_log2 = cur_info->cu_height_log2;
        int cu_width = 1 << cu_width_log2;
        int cu_height = 1 << cu_height_log2;
        bst_info->cu_mode = cur_info->cu_mode;

#if TB_SPLIT_EXT
        check_tb_part(cur_info);
        bst_info->pb_part = cur_info->pb_part;
        bst_info->tb_part = cur_info->tb_part;
        memcpy(&bst_info->pb_info, &cur_info->pb_info, sizeof(COM_PART_INFO));
        memcpy(&bst_info->tb_info, &cur_info->tb_info, sizeof(COM_PART_INFO));
#endif
        bst_info->umve_flag = cur_info->umve_flag;

        *cost_best = cost_curr;
        core->cost_best = cost_curr;

        SBAC_STORE(core->s_next_best[cu_width_log2 - 2][cu_height_log2 - 2], core->s_temp_best);
        if (bst_info->cu_mode != MODE_INTRA)
        {
            if (cur_info->affine_flag)
            {
#if BD_AFFINE_AMVR
                assert(pi->curr_mvr < MAX_NUM_AFFINE_MVR);
#else
                assert(pi->curr_mvr == 0);
#endif
            }

            if (bst_info->cu_mode == MODE_SKIP || bst_info->cu_mode == MODE_DIR)
            {
                bst_info->mvr_idx = 0;
            }
            else
            {
#if EXT_AMVR_HMVP
                bst_info->mvp_from_hmvp_flag = cur_info->mvp_from_hmvp_flag;
#endif
                bst_info->mvr_idx = pi->curr_mvr;
            }

            bst_info->refi[REFP_0] = cur_info->refi[REFP_0];
            bst_info->refi[REFP_1] = cur_info->refi[REFP_1];
            for (lidx = 0; lidx < REFP_NUM; lidx++)
            {
                bst_info->mv[lidx][MV_X] = cur_info->mv[lidx][MV_X];
                bst_info->mv[lidx][MV_Y] = cur_info->mv[lidx][MV_Y];
                bst_info->mvd[lidx][MV_X] = cur_info->mvd[lidx][MV_X];
                bst_info->mvd[lidx][MV_Y] = cur_info->mvd[lidx][MV_Y];
            }
#if SMVD
            bst_info->smvd_flag = cur_info->smvd_flag;
            if ( cur_info->smvd_flag )
            {
                assert( cur_info->affine_flag == 0 );
            }
#endif

            bst_info->affine_flag = cur_info->affine_flag;
            if (bst_info->affine_flag)
            {
                int vertex;
                int vertex_num = bst_info->affine_flag + 1;
                for (lidx = 0; lidx < REFP_NUM; lidx++)
                {
                    for (vertex = 0; vertex < vertex_num; vertex++)
                    {
                        bst_info->affine_mv[lidx][vertex][MV_X] = cur_info->affine_mv[lidx][vertex][MV_X];
                        bst_info->affine_mv[lidx][vertex][MV_Y] = cur_info->affine_mv[lidx][vertex][MV_Y];
                        bst_info->affine_mvd[lidx][vertex][MV_X] = cur_info->affine_mvd[lidx][vertex][MV_X];
                        bst_info->affine_mvd[lidx][vertex][MV_Y] = cur_info->affine_mvd[lidx][vertex][MV_Y];
                    }
                }
            }

            if (bst_info->cu_mode == MODE_SKIP)
            {
                if (bst_info->umve_flag != 0)
                    bst_info->umve_idx = cur_info->umve_idx;
                else
                    bst_info->skip_idx = cur_info->skip_idx;

                for (j = 0; j < N_C; j++)
                {
                    int size_tmp = (cu_width * cu_height) >> (j == 0 ? 0 : 2);
                    com_mcpy(bst_info->pred[j], cur_info->pred[j], size_tmp * sizeof(pel));
                }
                com_mset(bst_info->num_nz, 0, sizeof(int)*N_C*MAX_NUM_TB);
#if PLATFORM_GENERAL_DEBUG
                com_mset(bst_info->coef[Y_C], 0, sizeof(s16)*cu_width*cu_height);
                com_mset(bst_info->coef[U_C], 0, sizeof(s16)*cu_width*cu_height / 4);
                com_mset(bst_info->coef[V_C], 0, sizeof(s16)*cu_width*cu_height / 4);
#endif
                assert(bst_info->pb_part == SIZE_2Nx2N);
            }
            else
            {
                if (bst_info->cu_mode == MODE_DIR)
                {
                    if (bst_info->umve_flag)
                        bst_info->umve_idx = cur_info->umve_idx;
                    else
                        bst_info->skip_idx = cur_info->skip_idx;
                }
                for (j = 0; j < N_C; j++)
                {
                    int size_tmp = (cu_width * cu_height) >> (j == 0 ? 0 : 2);
                    cu_plane_nz_cpy(bst_info->num_nz, cur_info->num_nz, j);
                    com_mcpy(bst_info->pred[j], cur_info->pred[j], size_tmp * sizeof(pel));
                    com_mcpy(bst_info->coef[j], cur_info->coef[j], size_tmp * sizeof(s16));
                }
            }
        }
    }
#if EXT_AMVR_HMVP
    double skip_mode_2_thread = pi->qp_y / 60.0 * (THRESHOLD_MVPS_CHECK - 1) + 1;
    if (bst_info->cu_mode == MODE_SKIP && cur_info->cu_mode == MODE_INTER && ((*cost_best)*skip_mode_2_thread) > cost_curr)
    {
        core->skip_mvps_check = 0;
    }
#endif
}

static s16    resi_t[N_C][MAX_CU_DIM];

#if TR_SAVE_LOAD
static u8 search_inter_tr_info(ENC_CORE *core, u16 cu_ssd)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    u8 tb_part_size = 255;
    ENC_BEF_DATA* p_data = &core->bef_data[mod_info_curr->cu_width_log2 - 2][mod_info_curr->cu_height_log2 - 2][core->cup];

    for (int idx = 0; idx < p_data->num_inter_pred; idx++)
    {
        if (p_data->inter_pred_dist[idx] == cu_ssd)
            return p_data->inter_tb_part[idx];
    }
    return tb_part_size;
}

static void save_inter_tr_info(ENC_CORE *core, u16 cu_ssd, u8 tb_part_size)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_BEF_DATA* p_data = &core->bef_data[mod_info_curr->cu_width_log2 - 2][mod_info_curr->cu_height_log2 - 2][core->cup];
    if (p_data->num_inter_pred == NUM_SL_INTER)
        return;

    p_data->inter_pred_dist[p_data->num_inter_pred] = cu_ssd;
    p_data->inter_tb_part[p_data->num_inter_pred] = tb_part_size;
    p_data->num_inter_pred++;
}
#endif

static double pinter_residue_rdo(ENC_CTX *ctx, ENC_CORE *core, int bForceAllZero)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    s16 (*coef)[MAX_CU_DIM] = mod_info_curr->coef;
    s16 (*mv)[MV_D] = mod_info_curr->mv;
    s8 *refi = mod_info_curr->refi;
    ENC_PINTER *pi = &ctx->pinter;
    pel(*pred)[MAX_CU_DIM] = mod_info_curr->pred;
    int bit_depth = ctx->info.bit_depth_internal;
    int(*num_nz_coef)[N_C], tnnz, width[N_C], height[N_C], log2_w[N_C], log2_h[N_C];
    pel(*rec)[MAX_CU_DIM];
    s64    dist[2][N_C], dist_pred[N_C];
    double cost, cost_best = MAX_COST;
    int    cbf_best[N_C], nnz_store[MAX_NUM_TB][N_C], tb_part_store;
    int    bit_cnt;
    int    i, cbf_y, cbf_u, cbf_v;
    pel   *org[N_C];
    double cost_comp_best = MAX_COST;
    int    cbf_comps[N_C] = { 0, };
    int    j;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = mod_info_curr->cu_width;
    int cu_height = mod_info_curr->cu_height;
    int use_secTrans[MAX_NUM_TB] = { 0, 0, 0, 0 }; // secondary transform is disabled for inter coding
    int use_alt4x4Trans = 0;

#if RDO_DBK
    u8  is_from_mv_field = 0;
#endif

#if !BD_AFFINE_AMVR
    if (mod_info_curr->affine_flag)
    {
        pi->curr_mvr = 0;
    }
#endif

    rec = mod_info_curr->rec;
    num_nz_coef = mod_info_curr->num_nz;
    width [Y_C] = 1 << cu_width_log2 ;
    height[Y_C] = 1 << cu_height_log2;
    width [U_C] = width[V_C] = 1 << (cu_width_log2 - 1);
    height[U_C] = height[V_C] = 1 << (cu_height_log2 - 1);
    log2_w[Y_C] = cu_width_log2;
    log2_h[Y_C] = cu_height_log2;
    log2_w[U_C] = log2_w[V_C] = cu_width_log2 - 1;
    log2_h[U_C] = log2_h[V_C] = cu_height_log2 - 1;
    org[Y_C] = pi->Yuv_org[Y_C] + (y * pi->stride_org[Y_C]) + x;
    org[U_C] = pi->Yuv_org[U_C] + ((y >> 1) * pi->stride_org[U_C]) + (x >> 1);
    org[V_C] = pi->Yuv_org[V_C] + ((y >> 1) * pi->stride_org[V_C]) + (x >> 1);

    /* prediction */
    if (ctx->info.pic_header.slice_type == SLICE_P)
    {
        assert(REFI_IS_VALID(mod_info_curr->refi[REFP_0]) && !REFI_IS_VALID(mod_info_curr->refi[REFP_1]));
    }

    if (mod_info_curr->affine_flag)
    {
        com_affine_mc(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, bit_depth);

#if RDO_DBK
        com_set_affine_mvf(&ctx->info, mod_info_curr, ctx->refp, &ctx->map);
        is_from_mv_field = 1;
#endif
    }
    else
    {
        assert(mod_info_curr->pb_info.sub_scup[0] == mod_info_curr->scup);
        com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, ctx->tree_status, bit_depth);
        //com_mc(x, y, ctx->info.pic_width, ctx->info.pic_height, width[0], height[0], refi, mv, pi->refp, pred, width[0], ctx->tree_status, bit_depth);
    }

    /* get residual */
    enc_diff_pred(x, y, cu_width_log2, cu_height_log2, pi->pic_org, pred, resi_t);
    for (i = 0; i < N_C; i++)
    {
        dist[0][i] = dist_pred[i] = enc_ssd_16b(log2_w[i], log2_h[i], pred[i], org[i], width[i], pi->stride_org[i], bit_depth);
    }
#if RDO_DBK
    calc_delta_dist_filter_boundary(ctx, core, PIC_REC(ctx), PIC_ORG(ctx), cu_width, cu_height, pred, cu_width, x, y, 0, 0, refi, mv, is_from_mv_field);
    dist[0][Y_C] += ctx->delta_dist;
#endif

    /* test all zero case */
    memset(cbf_best, 0, sizeof(int) * N_C);
    if (mod_info_curr->cu_mode != MODE_DIR) // do not check forced zero for direct mode
    {
        memset(num_nz_coef, 0, sizeof(int) * MAX_NUM_TB * N_C);
        mod_info_curr->tb_part = SIZE_2Nx2N;
        if (ctx->tree_status == TREE_LC)
        {
            cost_best = (double)dist[0][Y_C] + ((dist[0][U_C] + dist[0][V_C]) * ctx->dist_chroma_weight[0]);
        }
        else
        {
            assert(ctx->tree_status == TREE_L);
            cost_best = (double)dist[0][Y_C];
        }

        SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
        bit_cnt = enc_get_bit_number(&core->s_temp_run);
        enc_bit_est_inter(ctx, core, ctx->info.pic_header.slice_type);
        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
        cost_best += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
        SBAC_STORE(core->s_temp_best, core->s_temp_run);
    }

    /* transform and quantization */
    bForceAllZero |= (mod_info_curr->cu_mode == MODE_SKIP);
    //force all zero (no residual) for CU size larger than 64x64
    bForceAllZero |= cu_width_log2 > 6 || cu_height_log2 > 6;

    if (!bForceAllZero)
    {
#if TR_EARLY_TERMINATE
        core->dist_pred_luma = dist_pred[Y_C];
#endif
#if TR_SAVE_LOAD //load
        s64 cu_ssd_s64 = dist_pred[Y_C] + dist_pred[U_C] + dist_pred[V_C];
        u16 cu_ssd_u16 = 0;
        core->best_tb_part_hist = 255;
        if (ctx->info.sqh.position_based_transform_enable_flag &&
                is_tb_avaliable(ctx->info, cu_width_log2, cu_height_log2, mod_info_curr->pb_part, MODE_INTER) && mod_info_curr->pb_part == SIZE_2Nx2N)
        {
            int shift_val = min(cu_width_log2 + cu_height_log2, 9);
            cu_ssd_u16 = (u16)(cu_ssd_s64 + (s64)(1 << (shift_val - 1))) >> shift_val;
            core->best_tb_part_hist = search_inter_tr_info(core, cu_ssd_u16);
            //core->best_tb_part_hist = 255; //enable this line to bypass the save load mechanism
        }
#endif

        tnnz = enc_tq_yuv_nnz(ctx, core, mod_info_curr, coef, resi_t, pi->slice_type, 0, use_secTrans, use_alt4x4Trans, refi, mv, is_from_mv_field);

        if (tnnz)
        {
            com_itdq_yuv(mod_info_curr, coef, resi_t, ctx->wq, cu_width_log2, cu_height_log2, pi->qp_y, pi->qp_u, pi->qp_v, bit_depth, use_secTrans, use_alt4x4Trans);
            for (i = 0; i < N_C; i++)
            {
                com_recon(i == Y_C ? mod_info_curr->tb_part : SIZE_2Nx2N, resi_t[i], pred[i], num_nz_coef, i, width[i], height[i], width[i], rec[i], bit_depth);

                if (is_cu_plane_nz(num_nz_coef, i))
                {
                    dist[1][i] = enc_ssd_16b(log2_w[i], log2_h[i], rec[i], org[i], width[i], pi->stride_org[i], bit_depth);
                }
                else
                {
                    dist[1][i] = dist_pred[i];
                }
            }

#if RDO_DBK
            //filter rec and calculate ssd
            calc_delta_dist_filter_boundary(ctx, core, PIC_REC(ctx), PIC_ORG(ctx), cu_width, cu_height, rec, cu_width, x, y, 0, 1, refi, mv, is_from_mv_field);
            dist[1][Y_C] += ctx->delta_dist;
#endif

            cbf_y = is_cu_plane_nz(num_nz_coef, Y_C) > 0 ? 1 : 0;
            cbf_u = is_cu_plane_nz(num_nz_coef, U_C) > 0 ? 1 : 0;
            cbf_v = is_cu_plane_nz(num_nz_coef, V_C) > 0 ? 1 : 0;

            if (ctx->tree_status == TREE_LC)
            {
                cost = (double)dist[cbf_y][Y_C] + ((dist[cbf_u][U_C] + dist[cbf_v][V_C]) * ctx->dist_chroma_weight[0]);
            }
            else
            {
                assert(ctx->tree_status == TREE_L);
                cost = (double)dist[cbf_y][Y_C];
            }

            SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
            //enc_sbac_bit_reset(&core->s_temp_run);
            bit_cnt = enc_get_bit_number(&core->s_temp_run);
            enc_bit_est_inter(ctx, core, ctx->info.pic_header.slice_type);
            bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
            cost += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
            if (cost < cost_best)
            {
                cost_best = cost;
                cbf_best[Y_C] = cbf_y;
                cbf_best[U_C] = cbf_u;
                cbf_best[V_C] = cbf_v;
                SBAC_STORE(core->s_temp_best, core->s_temp_run);
            }
            SBAC_LOAD(core->s_temp_prev_comp_best, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);

            /* cbf test for each component */
            com_mcpy(nnz_store, num_nz_coef, sizeof(int) * MAX_NUM_TB * N_C);
            tb_part_store = mod_info_curr->tb_part;
            if (ctx->tree_status == TREE_LC) // do not need to test for luma only
            {
                for (i = 0; i < N_C; i++)
                {
                    if (is_cu_plane_nz(nnz_store, i) > 0)
                    {
                        cost_comp_best = MAX_COST;
                        SBAC_LOAD(core->s_temp_prev_comp_run, core->s_temp_prev_comp_best);
                        for (j = 0; j < 2; j++)
                        {
                            cost = dist[j][i] * (i == 0 ? 1 : ctx->dist_chroma_weight[i - 1]);
                            if (j)
                            {
                                cu_plane_nz_cpy(num_nz_coef, nnz_store, i);
                                if (i == 0)
                                    mod_info_curr->tb_part = tb_part_store;
                            }
                            else
                            {
                                cu_plane_nz_cln(num_nz_coef, i);
                                if (i == 0)
                                    mod_info_curr->tb_part = SIZE_2Nx2N;
                            }

                            SBAC_LOAD(core->s_temp_run, core->s_temp_prev_comp_run);
                            //enc_sbac_bit_reset(&core->s_temp_run);
                            bit_cnt = enc_get_bit_number(&core->s_temp_run);
                            enc_bit_est_inter_comp(ctx, core, coef[i], i);
                            bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
                            cost += RATE_TO_COST_LAMBDA(ctx->lambda[i], bit_cnt);
                            if (cost < cost_comp_best)
                            {
                                cost_comp_best = cost;
                                cbf_comps[i] = j;
                                SBAC_STORE(core->s_temp_prev_comp_best, core->s_temp_run);
                            }
                        }
                    }
                    else
                    {
                        cbf_comps[i] = 0;
                    }
                }

                // do not set if all zero, because the case of all zero has been tested
                if (cbf_comps[Y_C] != 0 || cbf_comps[U_C] != 0 || cbf_comps[V_C] != 0)
                {
                    for (i = 0; i < N_C; i++)
                    {
                        if (cbf_comps[i])
                        {
                            cu_plane_nz_cpy(num_nz_coef, nnz_store, i);
                            if (i == 0)
                                mod_info_curr->tb_part = tb_part_store;
                        }
                        else
                        {
                            cu_plane_nz_cln(num_nz_coef, i);
                            if (i == 0)
                                mod_info_curr->tb_part = SIZE_2Nx2N;
                        }
                    }

                    // if the best num_nz_coef is changed
                    if (!is_cu_nz_equ(num_nz_coef, nnz_store))
                    {
                        cbf_y = cbf_comps[Y_C];
                        cbf_u = cbf_comps[U_C];
                        cbf_v = cbf_comps[V_C];
                        cost = dist[cbf_y][Y_C] + ((dist[cbf_u][U_C] + dist[cbf_v][V_C]) * ctx->dist_chroma_weight[0]);

                        SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
                        //enc_sbac_bit_reset(&core->s_temp_run);
                        bit_cnt = enc_get_bit_number(&core->s_temp_run);
                        enc_bit_est_inter(ctx, core, ctx->info.pic_header.slice_type);
                        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
                        cost += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
                        if (cost < cost_best)
                        {
                            cost_best = cost;
                            cbf_best[Y_C] = cbf_y;
                            cbf_best[U_C] = cbf_u;
                            cbf_best[V_C] = cbf_v;
                            SBAC_STORE(core->s_temp_best, core->s_temp_run);
                        }
                    }
                }
            }

            for (i = 0; i < N_C; i++)
            {
                if (cbf_best[i])
                {
                    cu_plane_nz_cpy(num_nz_coef, nnz_store, i);
                    if (i == 0)
                        mod_info_curr->tb_part = tb_part_store;
                }
                else
                {
                    cu_plane_nz_cln(num_nz_coef, i);
                    if (i == 0)
                        mod_info_curr->tb_part = SIZE_2Nx2N;
                }

                if (is_cu_plane_nz(num_nz_coef, i) == 0 && is_cu_plane_nz(nnz_store, i) != 0)
                {
                    com_mset(coef[i], 0, sizeof(s16) * ((cu_width * cu_height) >> (i == 0 ? 0 : 2)));
                }
            }
        }
#if TR_SAVE_LOAD // inter save
        if (core->best_tb_part_hist == 255 && mod_info_curr->pb_part == SIZE_2Nx2N)
        {
            save_inter_tr_info(core, cu_ssd_u16, (u8)mod_info_curr->tb_part);
        }
#endif
    }

    if (!is_cu_plane_nz(num_nz_coef, Y_C))
        mod_info_curr->tb_part = SIZE_2Nx2N; // reset best tb_part if no residual Y

    return cost_best;
}

#if CHROMA_NOT_SPLIT
double pinter_residue_rdo_chroma(ENC_CTX *ctx, ENC_CORE *core)
{
    ENC_PINTER *pi = &ctx->pinter;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    s16(*coef)[MAX_CU_DIM] = mod_info_curr->coef;
    s16(*mv)[MV_D] = mod_info_curr->mv;
    s8 *refi = mod_info_curr->refi;
    pel(*pred)[MAX_CU_DIM] = mod_info_curr->pred;
    int bit_depth = ctx->info.bit_depth_internal;
    int(*num_nz_coef)[N_C], tnnz, width[N_C], height[N_C], log2_w[N_C], log2_h[N_C];;
    pel(*rec)[MAX_CU_DIM];
    s64    dist[2][N_C];
    double cost, cost_best = MAX_COST;
    int    cbf_best[N_C];
    int    bit_cnt;
    int    i, cbf_y, cbf_u, cbf_v;
    pel   *org[N_C];
    int scup = mod_info_curr->scup;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = mod_info_curr->cu_width;
    int cu_height = mod_info_curr->cu_height;
    int use_secTrans[MAX_NUM_TB] = { 0, 0, 0, 0 }; // secondary transform is disabled for inter coding
    int use_alt4x4Trans = 0;

    rec = mod_info_curr->rec;
    num_nz_coef = mod_info_curr->num_nz;
    width[Y_C] = 1 << cu_width_log2;
    height[Y_C] = 1 << cu_height_log2;
    width[U_C] = width[V_C] = 1 << (cu_width_log2 - 1);
    height[U_C] = height[V_C] = 1 << (cu_height_log2 - 1);
    log2_w[Y_C] = cu_width_log2;
    log2_h[Y_C] = cu_height_log2;
    log2_w[U_C] = log2_w[V_C] = cu_width_log2 - 1;
    log2_h[U_C] = log2_h[V_C] = cu_height_log2 - 1;
    org[Y_C] = pi->Yuv_org[Y_C] + (y * pi->stride_org[Y_C]) + x;
    org[U_C] = pi->Yuv_org[U_C] + ((y >> 1) * pi->stride_org[U_C]) + (x >> 1);
    org[V_C] = pi->Yuv_org[V_C] + ((y >> 1) * pi->stride_org[V_C]) + (x >> 1);

#if TB_SPLIT_EXT
    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.pb_part, &core->mod_info_curr.pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.tb_part, &core->mod_info_curr.tb_info);
#endif

    /* prepare MV */
    int luma_scup = mod_info_curr->scup + PEL2SCU(mod_info_curr->cu_width - 1) + PEL2SCU(mod_info_curr->cu_height - 1) * ctx->info.pic_width_in_scu;
    assert(MCU_GET_INTRA_FLAG(ctx->map.map_scu[luma_scup]) == 0);
    for (i = 0; i < REFP_NUM; i++)
    {
        refi[i] = ctx->map.map_refi[luma_scup][i];
        mv[i][MV_X] = ctx->map.map_mv[luma_scup][i][MV_X];
        mv[i][MV_Y] = ctx->map.map_mv[luma_scup][i][MV_Y];

        mod_info_curr->refi[i] = ctx->map.map_refi[luma_scup][i];
        mod_info_curr->mv[i][MV_X] = ctx->map.map_mv[luma_scup][i][MV_X];
        mod_info_curr->mv[i][MV_Y] = ctx->map.map_mv[luma_scup][i][MV_Y];
    }

    /* chroma MC */
    assert(mod_info_curr->pb_info.sub_scup[0] == mod_info_curr->scup);
    com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, ctx->tree_status, bit_depth);
    //com_mc(x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, refi, mv, pi->refp, pred, cu_width, ctx->tree_status, bit_depth);

    /* get residual */
    enc_diff_pred(x, y, cu_width_log2, cu_height_log2, pi->pic_org, pred, resi_t);
    memset(dist, 0, sizeof(s64) * 2 * N_C);
    for (i = 1; i < N_C; i++)
    {
        dist[0][i] = enc_ssd_16b(log2_w[i], log2_h[i], pred[i], org[i], width[i], pi->stride_org[i], bit_depth);
    }

    /* test all zero case */
    memset(cbf_best, 0, sizeof(int) * N_C);
    memset(num_nz_coef, 0, sizeof(int) * MAX_NUM_TB * N_C);
    assert(mod_info_curr->tb_part == SIZE_2Nx2N);
    cost_best = (double)((dist[0][U_C] + dist[0][V_C]) * ctx->dist_chroma_weight[0]);

    SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
    bit_cnt = enc_get_bit_number(&core->s_temp_run);
    encode_coef(&core->bs_temp, coef, mod_info_curr->cu_width_log2, mod_info_curr->cu_height_log2, mod_info_curr->cu_mode, mod_info_curr, ctx->tree_status, ctx); // only count coeff bits for chroma tree
    bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
    cost_best += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
    SBAC_STORE(core->s_temp_best, core->s_temp_run);

    /* transform and quantization */
    tnnz = enc_tq_yuv_nnz(ctx, core, mod_info_curr, coef, resi_t, pi->slice_type, 0, use_secTrans, use_alt4x4Trans, refi, mv, 0);

    if (tnnz)
    {
        com_itdq_yuv(mod_info_curr, coef, resi_t, ctx->wq, cu_width_log2, cu_height_log2, pi->qp_y, pi->qp_u, pi->qp_v, bit_depth, use_secTrans, use_alt4x4Trans);
        for (i = 1; i < N_C; i++)
        {
            com_recon(i == Y_C ? mod_info_curr->tb_part : SIZE_2Nx2N, resi_t[i], pred[i], num_nz_coef, i, width[i], height[i], width[i], rec[i], bit_depth);

            if (is_cu_plane_nz(num_nz_coef, i))
            {
                dist[1][i] = enc_ssd_16b(log2_w[i], log2_h[i], rec[i], org[i], width[i], pi->stride_org[i], bit_depth);
            }
            else
            {
                dist[1][i] = dist[0][i];
            }
        }

        cbf_y = is_cu_plane_nz(num_nz_coef, Y_C) > 0 ? 1 : 0;
        cbf_u = is_cu_plane_nz(num_nz_coef, U_C) > 0 ? 1 : 0;
        cbf_v = is_cu_plane_nz(num_nz_coef, V_C) > 0 ? 1 : 0;

        cost = (double)((dist[cbf_u][U_C] + dist[cbf_v][V_C]) * ctx->dist_chroma_weight[0]);

        SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
        bit_cnt = enc_get_bit_number(&core->s_temp_run);
        encode_coef(&core->bs_temp, coef, mod_info_curr->cu_width_log2, mod_info_curr->cu_height_log2, mod_info_curr->cu_mode, mod_info_curr, ctx->tree_status, ctx); // only count coeff bits for chroma tree
        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
        cost += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
        if (cost < cost_best)
        {
            cost_best = cost;
            cbf_best[Y_C] = cbf_y;
            cbf_best[U_C] = cbf_u;
            cbf_best[V_C] = cbf_v;
            SBAC_STORE(core->s_temp_best, core->s_temp_run);
        }

    }

    /* save */
    for (i = 0; i < N_C; i++)
    {
        int size_tmp = (cu_width * cu_height) >> (i == 0 ? 0 : 2);
        if (cbf_best[i] == 0)
        {
            cu_plane_nz_cln(core->mod_info_best.num_nz, i);
            com_mset(core->mod_info_best.coef[i], 0, sizeof(s16) * size_tmp);
            com_mcpy(core->mod_info_best.rec[i], pred[i], sizeof(s16) * size_tmp);
        }
        else
        {
            cu_plane_nz_cpy(core->mod_info_best.num_nz, num_nz_coef, i);
            com_mcpy(core->mod_info_best.coef[i], coef[i], sizeof(s16) * size_tmp);
            com_mcpy(core->mod_info_best.rec[i], rec[i], sizeof(s16) * size_tmp);
        }
    }

    return cost_best;
}
#endif

static void init_inter_data(ENC_PINTER *pi, ENC_CORE *core)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    mod_info_curr->skip_idx = 0;
#if SMVD
    mod_info_curr->smvd_flag = 0;
#endif
    get_part_info(pi->pic_width_in_scu, mod_info_curr->x_pos, mod_info_curr->y_pos, mod_info_curr->cu_width, mod_info_curr->cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    assert(mod_info_curr->pb_info.sub_scup[0] == mod_info_curr->scup);

    com_mset(mod_info_curr->mv, 0, sizeof(s16) * REFP_NUM * MV_D);
    com_mset(mod_info_curr->mvd, 0, sizeof(s16) * REFP_NUM * MV_D);
    com_mset(mod_info_curr->refi, 0, sizeof(s8)  * REFP_NUM);

    com_mset(mod_info_curr->num_nz, 0, sizeof(int)*N_C*MAX_NUM_TB);
    com_mset(mod_info_curr->coef, 0, sizeof(s16) * N_C * MAX_CU_DIM);

    com_mset(mod_info_curr->affine_mv, 0, sizeof(CPMV) * REFP_NUM * VER_NUM * MV_D);
    com_mset(mod_info_curr->affine_mvd, 0, sizeof(s16) * REFP_NUM * VER_NUM * MV_D);
}

static void derive_inter_cands(ENC_CTX *ctx, ENC_CORE *core, s16(*pmv_cands)[REFP_NUM][MV_D], s8(*refi_cands)[REFP_NUM], int *num_cands_all, int *num_cands_woUMVE)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    int num_cands = 0;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;
    int scup_co = get_colocal_scup(mod_info_curr->scup, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu);

    COM_MOTION motion_cands_curr[MAX_SKIP_NUM];
    s8 cnt_hmvp_cands_curr = 0;

    int umve_idx;
    s16 pmv_base_cands[UMVE_BASE_NUM][REFP_NUM][MV_D];
    s8 refi_base_cands[UMVE_BASE_NUM][REFP_NUM];

    mod_info_curr->affine_flag = 0;

    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->tb_part, &mod_info_curr->tb_info);
    init_inter_data(pi, core);

    // insert TMVP
    num_cands = 0;
    if (ctx->info.pic_header.slice_type == SLICE_P)
    {
        refi_cands[num_cands][REFP_0] = 0;
        refi_cands[num_cands][REFP_1] = -1;
        if (REFI_IS_VALID(ctx->refp[0][REFP_0].map_refi[scup_co][REFP_0]))
        {
            get_col_mv_from_list0(pi->refp[0], ctx->ptr, scup_co, pmv_cands[num_cands]);
        }
        else
        {
            pmv_cands[num_cands][REFP_0][MV_X] = 0;
            pmv_cands[num_cands][REFP_0][MV_Y] = 0;
        }
        pmv_cands[num_cands][REFP_1][MV_X] = 0;
        pmv_cands[num_cands][REFP_1][MV_Y] = 0;
    }
    else
    {
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
        if (!REFI_IS_VALID(pi->refp[0][REFP_1].map_refi[scup_co][REFP_0]) || pi->refp[0][REFP_1].list_is_library_pic[pi->refp[0][REFP_1].map_refi[scup_co][REFP_0]] || pi->refp[0][REFP_1].is_library_picture || pi->refp[0][REFP_0].is_library_picture)
#else
        if (!REFI_IS_VALID(pi->refp[0][REFP_1].map_refi[scup_co][REFP_0]))
#endif
        {
            com_get_mvp_default(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, REFP_0, 0, 0, pmv_cands[num_cands][REFP_0]);

            com_get_mvp_default(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, REFP_1, 0, 0, pmv_cands[num_cands][REFP_1]);
        }
        else
        {
            get_col_mv(pi->refp[0], ctx->ptr, scup_co, pmv_cands[num_cands]);
        }
        SET_REFI(refi_cands[num_cands], 0, 0);
    }
    num_cands++;

    // insert list_01, list_1, list_0
    derive_MHBskip_spatial_motions(&ctx->info, mod_info_curr, &ctx->map, &pmv_cands[num_cands], &refi_cands[num_cands]);
    num_cands += PRED_DIR_NUM;

    // insert HMVP with pruning
    if (ctx->info.sqh.num_of_hmvp_cand)
    {
        int skip_idx;
        for (skip_idx = 0; skip_idx < num_cands; skip_idx++)
        {
            fill_skip_candidates(motion_cands_curr, &cnt_hmvp_cands_curr, ctx->info.sqh.num_of_hmvp_cand, pmv_cands[skip_idx], refi_cands[skip_idx], 0);
        }
        for (skip_idx = core->cnt_hmvp_cands; skip_idx > 0; skip_idx--) // fill the HMVP skip candidates
        {
            COM_MOTION motion = core->motion_cands[skip_idx - 1];
            fill_skip_candidates(motion_cands_curr, &cnt_hmvp_cands_curr, ctx->info.sqh.num_of_hmvp_cand, motion.mv, motion.ref_idx, 1);
        }

        s8 cnt_hmvp_extend = cnt_hmvp_cands_curr;
        COM_MOTION motion = core->cnt_hmvp_cands ? core->motion_cands[core->cnt_hmvp_cands - 1] : motion_cands_curr[TRADITIONAL_SKIP_NUM - 1]; // use last HMVP candidate or last spatial candidate to fill the rest
        for (skip_idx = cnt_hmvp_cands_curr; skip_idx < (TRADITIONAL_SKIP_NUM + ctx->info.sqh.num_of_hmvp_cand); skip_idx++) // fill skip candidates when hmvp not enough
        {
            // cnt_hmvp_cands_curr not changed
            fill_skip_candidates(motion_cands_curr, &cnt_hmvp_extend, ctx->info.sqh.num_of_hmvp_cand, motion.mv, motion.ref_idx, 0);
        }
        assert(cnt_hmvp_extend == (TRADITIONAL_SKIP_NUM + ctx->info.sqh.num_of_hmvp_cand));

        get_hmvp_skip_cands(motion_cands_curr, cnt_hmvp_extend, pmv_cands, refi_cands);
        num_cands = cnt_hmvp_cands_curr;
    }

    // insert UMVE
    *num_cands_woUMVE = num_cands;
    if (ctx->info.sqh.umve_enable_flag)
    {
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
        derive_umve_base_motions(ctx->refp, &ctx->info, mod_info_curr, &ctx->map, pmv_cands[0], refi_cands[0], pmv_base_cands, refi_base_cands);
#else
        derive_umve_base_motions(&ctx->info, mod_info_curr, &ctx->map, pmv_cands[0], refi_cands[0], pmv_base_cands, refi_base_cands);
#endif
        for (umve_idx = 0; umve_idx < UMVE_MAX_REFINE_NUM * UMVE_BASE_NUM; umve_idx++)
        {
            derive_umve_final_motions(umve_idx, pi->refp, pi->ptr, pmv_base_cands, refi_base_cands, &pmv_cands[*num_cands_woUMVE], &refi_cands[*num_cands_woUMVE]);
        }
        num_cands += UMVE_MAX_REFINE_NUM * UMVE_BASE_NUM;
    }
    *num_cands_all = num_cands;
}

static void analyze_direct_skip(ENC_CTX *ctx, ENC_CORE *core, double *cost_best)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    double       cost;
    int          skip_idx;
    const int num_iter = 2;
    int bNoResidual;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;

    int num_cands_woUMVE = 0;
    s16 pmv_cands[MAX_SKIP_NUM + UMVE_MAX_REFINE_NUM * UMVE_BASE_NUM][REFP_NUM][MV_D];
    s8 refi_cands[MAX_SKIP_NUM + UMVE_MAX_REFINE_NUM * UMVE_BASE_NUM][REFP_NUM];
    int num_cands_all, num_rdo;
    double cost_list[MAX_INTER_SKIP_RDO];
    int mode_list[MAX_INTER_SKIP_RDO];

    derive_inter_cands(ctx, core, pmv_cands, refi_cands, &num_cands_all, &num_cands_woUMVE);
    num_rdo = num_cands_woUMVE;
    assert(num_rdo <= min(MAX_INTER_SKIP_RDO, TRADITIONAL_SKIP_NUM + ctx->info.sqh.num_of_hmvp_cand));

    make_cand_list(core, ctx, mode_list, cost_list, num_cands_woUMVE, num_cands_all, num_rdo, pmv_cands, refi_cands);

    for (skip_idx = 0; skip_idx < num_rdo; skip_idx++)
    {
        int skip_idx_true = mode_list[skip_idx];
        if (skip_idx_true < num_cands_woUMVE)
        {
            mod_info_curr->umve_flag = 0;
            mod_info_curr->skip_idx = skip_idx_true;
        }
        else
        {
            mod_info_curr->umve_flag = 1;
            mod_info_curr->umve_idx = skip_idx_true - num_cands_woUMVE;
        }

        mod_info_curr->mv[REFP_0][MV_X] = pmv_cands[skip_idx_true][REFP_0][MV_X];
        mod_info_curr->mv[REFP_0][MV_Y] = pmv_cands[skip_idx_true][REFP_0][MV_Y];
        mod_info_curr->mv[REFP_1][MV_X] = pmv_cands[skip_idx_true][REFP_1][MV_X];
        mod_info_curr->mv[REFP_1][MV_Y] = pmv_cands[skip_idx_true][REFP_1][MV_Y];
        mod_info_curr->refi[REFP_0] = refi_cands[skip_idx_true][REFP_0];
        mod_info_curr->refi[REFP_1] = refi_cands[skip_idx_true][REFP_1];

        if (!REFI_IS_VALID(mod_info_curr->refi[REFP_0]) && !REFI_IS_VALID(mod_info_curr->refi[REFP_1]))
        {
            continue;
        }

        // skip index 1 and 2 for P slice
        if ((ctx->info.pic_header.slice_type == SLICE_P) && (mod_info_curr->skip_idx == 1 || mod_info_curr->skip_idx == 2) && (mod_info_curr->umve_flag == 0))
        {
            continue;
        }

        for (bNoResidual = 0; bNoResidual < num_iter; bNoResidual++)
        {
            mod_info_curr->cu_mode = bNoResidual ? MODE_SKIP : MODE_DIR;
            cost = pinter_residue_rdo(ctx, core, 0);
            check_best_mode(core, pi, cost, cost_best);
        }
    }
}

static void analyze_affine_merge( ENC_CTX *ctx, ENC_CORE *core, double *cost_best )
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    pel          *y_org, *u_org, *v_org;

    s8           mrg_list_refi[AFF_MAX_NUM_MRG][REFP_NUM];
    int          mrg_list_cp_num[AFF_MAX_NUM_MRG];
    CPMV         mrg_list_cp_mv[AFF_MAX_NUM_MRG][REFP_NUM][VER_NUM][MV_D];

    double       cost = MAX_COST;
    int          mrg_idx, num_cands = 0;
    int          ver = 0, iter = 0, iter_start = 0, iter_end = 2;

    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int log2_cuw = mod_info_curr->cu_width_log2;
    int log2_cuh = mod_info_curr->cu_height_log2;
    int cu_width = 1 << log2_cuw;
    int cu_height = 1 << log2_cuh;
    y_org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];
    u_org = pi->Yuv_org[U_C] + (x >> 1) + ((y >> 1) * pi->stride_org[U_C]);
    v_org = pi->Yuv_org[V_C] + (x >> 1) + ((y >> 1) * pi->stride_org[V_C]);

    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->tb_part, &mod_info_curr->tb_info);
    init_inter_data(pi, core);

    pi->curr_mvr = 0;
    mod_info_curr->mvr_idx = 0;
    mod_info_curr->affine_flag = 1;
    num_cands = com_get_affine_merge_candidate(&ctx->info, mod_info_curr, pi->refp, &ctx->map, mrg_list_refi, mrg_list_cp_mv, mrg_list_cp_num, ctx->ptr);
  

    if ( num_cands == 0 )
        return;

    for ( mrg_idx = 0; mrg_idx < num_cands; mrg_idx++ )
    {
        // affine memory access constraints
        int memory_access[REFP_NUM];
        int allowed = 1;
        int i;
        for (i = 0; i < REFP_NUM; i++)
        {
            if (REFI_IS_VALID(mrg_list_refi[mrg_idx][i]))
            {
                if (mrg_list_cp_num[mrg_idx] == 3) // derive RB
                {
                    mrg_list_cp_mv[mrg_idx][i][3][MV_X] = mrg_list_cp_mv[mrg_idx][i][1][MV_X] + mrg_list_cp_mv[mrg_idx][i][2][MV_X] - mrg_list_cp_mv[mrg_idx][i][0][MV_X];
                    mrg_list_cp_mv[mrg_idx][i][3][MV_Y] = mrg_list_cp_mv[mrg_idx][i][1][MV_Y] + mrg_list_cp_mv[mrg_idx][i][2][MV_Y] - mrg_list_cp_mv[mrg_idx][i][0][MV_Y];
                }
                else // derive LB, RB
                {
                    mrg_list_cp_mv[mrg_idx][i][2][MV_X] = mrg_list_cp_mv[mrg_idx][i][0][MV_X] - (mrg_list_cp_mv[mrg_idx][i][1][MV_Y] - mrg_list_cp_mv[mrg_idx][i][0][MV_Y]) * (s16)cu_height / (s16)cu_width;
                    mrg_list_cp_mv[mrg_idx][i][2][MV_Y] = mrg_list_cp_mv[mrg_idx][i][0][MV_Y] + (mrg_list_cp_mv[mrg_idx][i][1][MV_X] - mrg_list_cp_mv[mrg_idx][i][0][MV_X]) * (s16)cu_height / (s16)cu_width;
                    mrg_list_cp_mv[mrg_idx][i][3][MV_X] = mrg_list_cp_mv[mrg_idx][i][1][MV_X] - (mrg_list_cp_mv[mrg_idx][i][1][MV_Y] - mrg_list_cp_mv[mrg_idx][i][0][MV_Y]) * (s16)cu_height / (s16)cu_width;
                    mrg_list_cp_mv[mrg_idx][i][3][MV_Y] = mrg_list_cp_mv[mrg_idx][i][1][MV_Y] + (mrg_list_cp_mv[mrg_idx][i][1][MV_X] - mrg_list_cp_mv[mrg_idx][i][0][MV_X]) * (s16)cu_height / (s16)cu_width;
                }
                memory_access[i] = com_get_affine_memory_access(mrg_list_cp_mv[mrg_idx][i], cu_width, cu_height);
            }
        }

        if ( REFI_IS_VALID( mrg_list_refi[mrg_idx][0] ) && REFI_IS_VALID( mrg_list_refi[mrg_idx][1] ) )
        {
            int mem = MAX_MEMORY_ACCESS_BI * cu_width * cu_height;
            if ( memory_access[0] > mem || memory_access[1] > mem )
            {
                allowed = 0;
            }
        }
        else
        {
            int valid_idx = REFI_IS_VALID( mrg_list_refi[mrg_idx][0] ) ? 0 : 1;
            int mem = MAX_MEMORY_ACCESS_UNI * cu_width * cu_height;
            if ( memory_access[valid_idx] > mem )
            {
                allowed = 0;
            }
        }
        if ( !allowed )
        {
            continue;
        }

        // set motion information
        mod_info_curr->umve_flag = 0;
        mod_info_curr->affine_flag = (u8)mrg_list_cp_num[mrg_idx] - 1;
        mod_info_curr->skip_idx = (u8)mrg_idx;
        for ( ver = 0; ver < mrg_list_cp_num[mrg_idx]; ver++ )
        {
            mod_info_curr->affine_mv[REFP_0][ver][MV_X] = mrg_list_cp_mv[mrg_idx][REFP_0][ver][MV_X];
            mod_info_curr->affine_mv[REFP_0][ver][MV_Y] = mrg_list_cp_mv[mrg_idx][REFP_0][ver][MV_Y];
            mod_info_curr->affine_mv[REFP_1][ver][MV_X] = mrg_list_cp_mv[mrg_idx][REFP_1][ver][MV_X];
            mod_info_curr->affine_mv[REFP_1][ver][MV_Y] = mrg_list_cp_mv[mrg_idx][REFP_1][ver][MV_Y];
        }
        mod_info_curr->refi[REFP_0] = mrg_list_refi[mrg_idx][REFP_0];
        mod_info_curr->refi[REFP_1] = mrg_list_refi[mrg_idx][REFP_1];

        for (iter = iter_start; iter < iter_end; iter++)
        {
            mod_info_curr->cu_mode = iter ? MODE_SKIP : MODE_DIR;
            cost = pinter_residue_rdo(ctx, core, 0);
            check_best_mode(core, pi, cost, cost_best);
        }
    }
}

static void analyze_uni_pred(ENC_CTX *ctx, ENC_CORE *core, double *cost_L0L1, s16 mv_L0L1[REFP_NUM][MV_D], s8 *refi_L0L1, double *cost_best)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    int lidx;
    s16 *mvp, *mv, *mvd;
    u32 mecost, best_mecost;
    s8 refi_cur = 0;
    s8 best_refi = 0;
    s8 t0, t1;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;

    mod_info_curr->cu_mode = MODE_INTER;
    for (lidx = 0; lidx <= ((pi->slice_type == SLICE_P) ? PRED_L0 : PRED_L1); lidx++) // uni-prediction (L0 or L1)
    {
        init_inter_data(pi, core);
        mv = mod_info_curr->mv[lidx];
        mvd = mod_info_curr->mvd[lidx];
        pi->num_refp = (u8)ctx->rpm.num_refp[lidx];
        best_mecost = COM_UINT32_MAX;
        for (refi_cur = 0; refi_cur < pi->num_refp; refi_cur++)
        {
            mvp = pi->mvp_scale[lidx][refi_cur];

            com_derive_mvp(ctx->info, mod_info_curr, ctx->ptr, lidx, refi_cur, core->cnt_hmvp_cands,
                core->motion_cands, ctx->map, ctx->refp, pi->curr_mvr, mvp);

            // motion search
            mecost = pi->fn_me(pi, x, y, cu_width, cu_height, mod_info_curr->x_pos, mod_info_curr->y_pos, cu_width, &refi_cur, lidx, mvp, mv, 0);
            pi->mv_scale[lidx][refi_cur][MV_X] = mv[MV_X];
            pi->mv_scale[lidx][refi_cur][MV_Y] = mv[MV_Y];
            if (mecost < best_mecost)
            {
                best_mecost = mecost;
                best_refi = refi_cur;
            }

#if BD_AFFINE_AMVR
            if (pi->curr_mvr < MAX_NUM_AFFINE_MVR)
#else
            if (pi->curr_mvr == 0)
#endif
            {
                pi->best_mv_uni[lidx][refi_cur][MV_X] = mv[MV_X];
                pi->best_mv_uni[lidx][refi_cur][MV_Y] = mv[MV_Y];
            }
        }
        mv[MV_X] = pi->mv_scale[lidx][best_refi][MV_X];
        mv[MV_Y] = pi->mv_scale[lidx][best_refi][MV_Y];
        mvp = pi->mvp_scale[lidx][best_refi];
        t0 = (lidx == 0) ? best_refi : REFI_INVALID;
        t1 = (lidx == 1) ? best_refi : REFI_INVALID;
        SET_REFI(mod_info_curr->refi, t0, t1);
        refi_L0L1[lidx] = best_refi;

        mv[MV_X] = (mv[MV_X] >> pi->curr_mvr) << pi->curr_mvr;
        mv[MV_Y] = (mv[MV_Y] >> pi->curr_mvr) << pi->curr_mvr;

        mvd[MV_X] = mv[MV_X] - mvp[MV_X];
        mvd[MV_Y] = mv[MV_Y] - mvp[MV_Y];

        /* important: reset mv/mvd */
        {
            // note: reset mvd, after clipping, mvd might not align with amvr index
            int amvr_shift = pi->curr_mvr;
            mvd[MV_X] = mvd[MV_X] >> amvr_shift << amvr_shift;
            mvd[MV_Y] = mvd[MV_Y] >> amvr_shift << amvr_shift;

            // note: reset mv, after clipping, mv might not equal to mvp + mvd
            int mv_x = (s32)mvd[MV_X] + mvp[MV_X];
            int mv_y = (s32)mvd[MV_Y] + mvp[MV_Y];
            mv[MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_x);
            mv[MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_y);
        }

        mv_L0L1[lidx][MV_X] = mv[MV_X];
        mv_L0L1[lidx][MV_Y] = mv[MV_Y];

        // get mot_bits for bi search: mvd + refi
        pi->mot_bits[lidx] = get_mv_bits_with_mvr(mvd[MV_X], mvd[MV_Y], pi->num_refp, best_refi, pi->curr_mvr);
        pi->mot_bits[lidx] -= (pi->curr_mvr == MAX_NUM_MVR - 1) ? pi->curr_mvr : (pi->curr_mvr + 1); // minus amvr index

        cost_L0L1[lidx] = pinter_residue_rdo(ctx, core, 0);
        check_best_mode(core, pi, cost_L0L1[lidx], cost_best);
    }
}

static void analyze_bi(ENC_CTX *ctx, ENC_CORE *core, s16 mv_L0L1[REFP_NUM][MV_D], const s8 *refi_L0L1, const double *cost_L0L1, double *cost_best)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    int bit_depth = ctx->info.bit_depth_internal;
    s8         refi[REFP_NUM] = { REFI_INVALID, REFI_INVALID };
    u32        best_mecost = COM_UINT32_MAX;
    s8        refi_best = 0, refi_cur;
    int        changed = 0;
    u32        mecost;
    pel        *org;
    pel(*pred)[MAX_CU_DIM];
    s8         t0, t1;
    double      cost;
    s8         lidx_ref, lidx_cnd;
    int         i;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;
    mod_info_curr->cu_mode = MODE_INTER;

    init_inter_data(pi, core);
    if (cost_L0L1[PRED_L0] <= cost_L0L1[PRED_L1])
    {
        lidx_ref = REFP_0;
        lidx_cnd = REFP_1;
    }
    else
    {
        lidx_ref = REFP_1;
        lidx_cnd = REFP_0;
    }
    mod_info_curr->refi[REFP_0] = refi_L0L1[REFP_0];
    mod_info_curr->refi[REFP_1] = refi_L0L1[REFP_1];
    t0 = (lidx_ref == REFP_0) ? refi_L0L1[lidx_ref] : REFI_INVALID;
    t1 = (lidx_ref == REFP_1) ? refi_L0L1[lidx_ref] : REFI_INVALID;
    SET_REFI(refi, t0, t1);
    mod_info_curr->mv[lidx_ref][MV_X] = mv_L0L1[lidx_ref][MV_X];
    mod_info_curr->mv[lidx_ref][MV_Y] = mv_L0L1[lidx_ref][MV_Y];
    mod_info_curr->mv[lidx_cnd][MV_X] = mv_L0L1[lidx_cnd][MV_X];
    mod_info_curr->mv[lidx_cnd][MV_Y] = mv_L0L1[lidx_cnd][MV_Y];
    /* get MVP lidx_cnd */
    org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];
    pred = mod_info_curr->pred;
    for (i = 0; i < BI_ITER; i++)
    {
        /* predict reference */
        s8 temp_refi[REFP_NUM];
        temp_refi[REFP_0] = mod_info_curr->refi[REFP_0];
        temp_refi[REFP_1] = mod_info_curr->refi[REFP_1];
        mod_info_curr->refi[REFP_0] = refi[REFP_0];
        mod_info_curr->refi[REFP_1] = refi[REFP_1];
        com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, CHANNEL_L, bit_depth);
        //com_mc(x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, refi, mod_info_curr->mv, pi->refp, pred, cu_width, CHANNEL_L, bit_depth);
        mod_info_curr->refi[REFP_0] = temp_refi[REFP_0];
        mod_info_curr->refi[REFP_1] = temp_refi[REFP_1];

        get_org_bi(org, pred[Y_C], pi->stride_org[Y_C], cu_width, cu_height, pi->org_bi, cu_width, 0, 0);
        SWAP(refi[lidx_ref], refi[lidx_cnd], t0);
        SWAP(lidx_ref, lidx_cnd, t0);
        changed = 0;
        for (refi_cur = 0; refi_cur < pi->num_refp; refi_cur++)
        {
            mecost = pi->fn_me(pi, x, y, cu_width, cu_height, mod_info_curr->x_pos, mod_info_curr->y_pos, cu_width, &refi_cur, lidx_ref, pi->mvp_scale[lidx_ref][refi_cur], pi->mv_scale[lidx_ref][refi_cur], 1);
            if (mecost < best_mecost)
            {
                refi_best = refi_cur;
                best_mecost = mecost;
                changed = 1;
                t0 = (lidx_ref == REFP_0) ? refi_best : mod_info_curr->refi[lidx_cnd];
                t1 = (lidx_ref == REFP_1) ? refi_best : mod_info_curr->refi[lidx_cnd];
                SET_REFI(mod_info_curr->refi, t0, t1);
                mod_info_curr->mv[lidx_ref][MV_X] = pi->mv_scale[lidx_ref][refi_cur][MV_X];
                mod_info_curr->mv[lidx_ref][MV_Y] = pi->mv_scale[lidx_ref][refi_cur][MV_Y];

                pi->mot_bits[lidx_ref] = get_mv_bits_with_mvr(mod_info_curr->mv[lidx_ref][MV_X] - pi->mvp_scale[lidx_ref][refi_cur][MV_X], mod_info_curr->mv[lidx_ref][MV_Y] - pi->mvp_scale[lidx_ref][refi_cur][MV_Y], pi->num_refp, refi_cur, pi->curr_mvr);
                pi->mot_bits[lidx_ref] -= (pi->curr_mvr == MAX_NUM_MVR - 1) ? pi->curr_mvr : (pi->curr_mvr + 1); // minus amvr index
            }
        }
        t0 = (lidx_ref == REFP_0) ? refi_best : REFI_INVALID;
        t1 = (lidx_ref == REFP_1) ? refi_best : REFI_INVALID;
        SET_REFI(refi, t0, t1);
        if (!changed)
        {
            break;
        }
    }

    mod_info_curr->mv[REFP_0][MV_X] = (mod_info_curr->mv[REFP_0][MV_X] >> pi->curr_mvr) << pi->curr_mvr;
    mod_info_curr->mv[REFP_0][MV_Y] = (mod_info_curr->mv[REFP_0][MV_Y] >> pi->curr_mvr) << pi->curr_mvr;
    mod_info_curr->mv[REFP_1][MV_X] = (mod_info_curr->mv[REFP_1][MV_X] >> pi->curr_mvr) << pi->curr_mvr;
    mod_info_curr->mv[REFP_1][MV_Y] = (mod_info_curr->mv[REFP_1][MV_Y] >> pi->curr_mvr) << pi->curr_mvr;

    mod_info_curr->mvd[REFP_0][MV_X] = mod_info_curr->mv[REFP_0][MV_X] - pi->mvp_scale[REFP_0][mod_info_curr->refi[REFP_0]][MV_X];
    mod_info_curr->mvd[REFP_0][MV_Y] = mod_info_curr->mv[REFP_0][MV_Y] - pi->mvp_scale[REFP_0][mod_info_curr->refi[REFP_0]][MV_Y];
    mod_info_curr->mvd[REFP_1][MV_X] = mod_info_curr->mv[REFP_1][MV_X] - pi->mvp_scale[REFP_1][mod_info_curr->refi[REFP_1]][MV_X];
    mod_info_curr->mvd[REFP_1][MV_Y] = mod_info_curr->mv[REFP_1][MV_Y] - pi->mvp_scale[REFP_1][mod_info_curr->refi[REFP_1]][MV_Y];

    /* important: reset mv/mvd */
    for (i=0; i<REFP_NUM; i++)
    {
        // note: reset mvd, after clipping, mvd might not align with amvr index
        int amvr_shift = pi->curr_mvr;
        mod_info_curr->mvd[i][MV_X] = mod_info_curr->mvd[i][MV_X] >> amvr_shift << amvr_shift;
        mod_info_curr->mvd[i][MV_Y] = mod_info_curr->mvd[i][MV_Y] >> amvr_shift << amvr_shift;

        // note: mvd = mv - mvp, but because of clipping, mv might not equal to mvp + mvd
        int mv_x = (s32)mod_info_curr->mvd[i][MV_X] + pi->mvp_scale[i][mod_info_curr->refi[i]][MV_X];
        int mv_y = (s32)mod_info_curr->mvd[i][MV_Y] + pi->mvp_scale[i][mod_info_curr->refi[i]][MV_Y];
        mod_info_curr->mv[i][MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_x);
        mod_info_curr->mv[i][MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_y);
    }

    cost = pinter_residue_rdo(ctx, core, 0);
    check_best_mode(core, pi, cost, cost_best);
}

#if SMVD
static u32 smvd_refine( ENC_CTX *ctx, ENC_CORE *core, int x, int y, int log2_cuw, int log2_cuh, s16 mv[REFP_NUM][MV_D], s16 mvp[REFP_NUM][MV_D], s8 refi[REFP_NUM], s32 lidx_cur, s32 lidx_tar, u32 mecost, s32 search_pattern, s32 search_round, s32 search_shift )
{
    ENC_PINTER *pi = &ctx->pinter;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int bit_depth = ctx->info.bit_depth_internal;

    int         cu_width, cu_height;
    u8          mv_bits = 0;
    pel        *org;

    s32 search_offset_cross[4][MV_D] = { { 0, 1 },{ 1, 0 },{ 0, -1 },{ -1, 0 } };
    s32 search_offset_square[8][MV_D] = { { -1, 1 },{ 0, 1 },{ 1, 1 },{ 1, 0 },{ 1, -1 },{ 0, -1 },{ -1, -1 },{ -1, 0 } };
    s32 search_offset_diamond[8][MV_D] = { { 0, 2 },{ 1, 1 },{ 2, 0 },{ 1, -1 },{ 0, -2 },{ -1, -1 },{ -2, 0 },{ -1, 1 } };
    s32 search_offset_hexagon[6][MV_D] = { { 2, 0 },{ 1, 2 },{ -1, 2 },{ -2, 0 },{ -1, -2 },{ 1, -2 } };

    s32 direct_start = 0;
    s32 direct_end = 0;
    s32 direct_rounding = 0;
    s32 direct_mask = 0;
    s32( *search_offset )[MV_D] = search_offset_diamond;

    s32 round;
    s32 best_direct = -1;
    s32 step = 1;

    org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];

    cu_width = (1 << log2_cuw);
    cu_height = (1 << log2_cuh);

    if ( search_pattern == 0 )
    {
        direct_end = 3;
        direct_rounding = 4;
        direct_mask = 0x03;
        search_offset = search_offset_cross;
    }
    else if ( search_pattern == 1 )
    {
        direct_end = 7;
        direct_rounding = 8;
        direct_mask = 0x07;
        search_offset = search_offset_square;
    }
    else if ( search_pattern == 2 )
    {
        direct_end = 7;
        direct_rounding = 8;
        direct_mask = 0x07;
        search_offset = search_offset_diamond;
    }
    else if ( search_pattern == 3 )
    {
        direct_end = 5;
        search_offset = search_offset_hexagon;
    }

    for ( round = 0; round < search_round; round++ )
    {
        s16 mv_cur_center[MV_D];
        s32 index;

        best_direct = -1;
        mv_cur_center[MV_X] = mv[lidx_cur][MV_X];
        mv_cur_center[MV_Y] = mv[lidx_cur][MV_Y];

        for ( index = direct_start; index <= direct_end; index++ )
        {
            s32 direct;
            u32 mecost_tmp;
            s16 mv_cand[REFP_NUM][MV_D], mvd_cand[REFP_NUM][MV_D];

            if ( search_pattern == 3 )
            {
                direct = index < 0 ? index + 6 : index >= 6 ? index - 6 : index;
            }
            else
            {
                direct = (index + direct_rounding) & direct_mask;
            }

            // update list cur
            mv_cand[lidx_cur][MV_X] = mv_cur_center[MV_X] + (s16)(search_offset[direct][MV_X] << search_shift);
            mv_cand[lidx_cur][MV_Y] = mv_cur_center[MV_Y] + (s16)(search_offset[direct][MV_Y] << search_shift);
            mvd_cand[lidx_cur][MV_X] = mv_cand[lidx_cur][MV_X] - mvp[lidx_cur][MV_X];
            mvd_cand[lidx_cur][MV_Y] = mv_cand[lidx_cur][MV_Y] - mvp[lidx_cur][MV_Y];

            // update list tar
            mv_cand[lidx_tar][MV_X] = mvp[lidx_tar][MV_X] - mvd_cand[lidx_cur][MV_X];
            mv_cand[lidx_tar][MV_Y] = mvp[lidx_tar][MV_Y] - mvd_cand[lidx_cur][MV_Y];

            mod_info_curr->mv[REFP_0][MV_X] = mv_cand[REFP_0][MV_X];
            mod_info_curr->mv[REFP_0][MV_Y] = mv_cand[REFP_0][MV_Y];
            mod_info_curr->mv[REFP_1][MV_X] = mv_cand[REFP_1][MV_X];
            mod_info_curr->mv[REFP_1][MV_Y] = mv_cand[REFP_1][MV_Y];
            
            // get cost
            com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, CHANNEL_L, bit_depth);
            //com_mc( x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, mod_info_curr->refi, mv_cand, pi->refp, mod_info_curr->pred, cu_width, CHANNEL_L, bit_depth);

            mv_bits = (u8)get_mv_bits_with_mvr( mvd_cand[lidx_cur][MV_X], mvd_cand[lidx_cur][MV_Y], 0, mod_info_curr->refi[lidx_cur], pi->curr_mvr );

            mecost_tmp = enc_satd_16b( log2_cuw, log2_cuh, org, mod_info_curr->pred[0], pi->stride_org[Y_C], cu_width, bit_depth);
            mecost_tmp += MV_COST( pi, mv_bits );

            // save best
            if ( mecost_tmp < mecost )
            {
                mecost = mecost_tmp;
                mv[lidx_cur][MV_X] = mv_cand[lidx_cur][MV_X];
                mv[lidx_cur][MV_Y] = mv_cand[lidx_cur][MV_Y];

                mv[lidx_tar][MV_X] = mv_cand[lidx_tar][MV_X];
                mv[lidx_tar][MV_Y] = mv_cand[lidx_tar][MV_Y];

                best_direct = direct;
            }
        }

        if ( best_direct == -1 )
        {
            break;
        }
        step = 1;
        if ( (search_pattern == 1) || (search_pattern == 2) )
        {
            step = 2 - (best_direct & 0x01);
        }
        direct_start = best_direct - step;
        direct_end = best_direct + step;
    }

    return mecost;
};

static void analyze_smvd( ENC_CTX *ctx, ENC_CORE *core, double *cost_best )
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    int bit_depth = ctx->info.bit_depth_internal;
    u32        mecost;
    pel        *org;
    double      cost = MAX_COST;
    int         lidx_ref, lidx_cnd;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int log2_cuw = mod_info_curr->cu_width_log2;
    int log2_cuh = mod_info_curr->cu_height_log2;
    int cu_width = 1 << log2_cuw;
    int cu_height = 1 << log2_cuh;
    mod_info_curr->cu_mode = MODE_INTER;
    init_inter_data(pi, core);

    u8          mv_bits = 0;
    s16         mv[REFP_NUM][MV_D], mvp[REFP_NUM][MV_D], mvd[REFP_NUM][MV_D];
    int         max_round;

    org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];

    {
        mod_info_curr->smvd_flag = 1;
        lidx_ref = 0;
        lidx_cnd = 1;

        mod_info_curr->refi[REFP_0] = 0;
        mod_info_curr->refi[REFP_1] = 0;

        mvp[REFP_0][MV_X] = pi->mvp_scale[REFP_0][mod_info_curr->refi[REFP_0]][MV_X];
        mvp[REFP_0][MV_Y] = pi->mvp_scale[REFP_0][mod_info_curr->refi[REFP_0]][MV_Y];
        mvp[REFP_1][MV_X] = pi->mvp_scale[REFP_1][mod_info_curr->refi[REFP_1]][MV_X];
        mvp[REFP_1][MV_Y] = pi->mvp_scale[REFP_1][mod_info_curr->refi[REFP_1]][MV_Y];

        mv[0][MV_X] = mvp[0][MV_X];
        mv[0][MV_Y] = mvp[0][MV_Y];
        mv[1][MV_X] = mvp[1][MV_X];
        mv[1][MV_Y] = mvp[1][MV_Y];

        mvd[REFP_0][MV_X] = mv[REFP_0][MV_X] - mvp[REFP_0][MV_X];
        mvd[REFP_0][MV_Y] = mv[REFP_0][MV_Y] - mvp[REFP_0][MV_Y];
        mvd[REFP_1][MV_X] = mv[REFP_1][MV_X] - mvp[REFP_1][MV_X];
        mvd[REFP_1][MV_Y] = mv[REFP_1][MV_Y] - mvp[REFP_1][MV_Y];

        mod_info_curr->mv[REFP_0][MV_X] = mv[REFP_0][MV_X];
        mod_info_curr->mv[REFP_0][MV_Y] = mv[REFP_0][MV_Y];
        mod_info_curr->mv[REFP_1][MV_X] = mv[REFP_1][MV_X];
        mod_info_curr->mv[REFP_1][MV_Y] = mv[REFP_1][MV_Y];

        com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, CHANNEL_L, bit_depth);
        //com_mc( x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, mod_info_curr->refi, mv, pi->refp, mod_info_curr->pred, cu_width, CHANNEL_L, bit_depth);

        mv_bits = (u8)get_mv_bits_with_mvr( mvd[lidx_ref][MV_X], mvd[lidx_ref][MV_Y], 0, mod_info_curr->refi[lidx_ref], pi->curr_mvr );
        mecost = enc_satd_16b( log2_cuw, log2_cuh, org, mod_info_curr->pred[0], pi->stride_org[Y_C], cu_width, bit_depth);
        mecost += MV_COST( pi, mv_bits );

        s16 mv_bi[REFP_NUM][MV_D];
        u32 mecost_bi;
        mv_bi[REFP_0][MV_X] = pi->mv_scale[REFP_0][0][MV_X];
        mv_bi[REFP_0][MV_Y] = pi->mv_scale[REFP_0][0][MV_Y];
        mvd[REFP_0][MV_X] = mv_bi[REFP_0][MV_X] - mvp[REFP_0][MV_X];
        mvd[REFP_0][MV_Y] = mv_bi[REFP_0][MV_Y] - mvp[REFP_0][MV_Y];
        mv_bi[REFP_1][MV_X] = mvp[REFP_1][MV_X] - mvd[REFP_0][MV_X];
        mv_bi[REFP_1][MV_Y] = mvp[REFP_1][MV_Y] - mvd[REFP_0][MV_Y];

        mod_info_curr->mv[REFP_0][MV_X] = mv_bi[REFP_0][MV_X];
        mod_info_curr->mv[REFP_0][MV_Y] = mv_bi[REFP_0][MV_Y];
        mod_info_curr->mv[REFP_1][MV_X] = mv_bi[REFP_1][MV_X];
        mod_info_curr->mv[REFP_1][MV_Y] = mv_bi[REFP_1][MV_Y];

        com_mc(&ctx->info, mod_info_curr, pi->refp, &ctx->map, CHANNEL_L, bit_depth);
        //com_mc(x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, mod_info_curr->refi, mv_bi, pi->refp, mod_info_curr->pred, cu_width, CHANNEL_L, bit_depth);

        mv_bits = (u8)get_mv_bits_with_mvr(mvd[0][MV_X], mvd[0][MV_Y], 0, mod_info_curr->refi[0], pi->curr_mvr);
        mecost_bi = enc_satd_16b(log2_cuw, log2_cuh, org, mod_info_curr->pred[0], pi->stride_org[Y_C], cu_width, bit_depth);
        mecost_bi += MV_COST(pi, mv_bits);

        if (mecost_bi < mecost)
        {
            mecost = mecost_bi;
            mv[0][MV_X] = mv_bi[0][MV_X];
            mv[0][MV_Y] = mv_bi[0][MV_Y];
            mv[1][MV_X] = mv_bi[1][MV_X];
            mv[1][MV_Y] = mv_bi[1][MV_Y];
        }

        // refine
        max_round = 8;
        mecost = smvd_refine( ctx, core, x, y, log2_cuw, log2_cuh, mv, mvp, mod_info_curr->refi, lidx_ref, lidx_cnd, mecost, 2, max_round, pi->curr_mvr );
        mecost = smvd_refine( ctx, core, x, y, log2_cuw, log2_cuh, mv, mvp, mod_info_curr->refi, lidx_ref, lidx_cnd, mecost, 0, 1, pi->curr_mvr );

        mod_info_curr->mv[REFP_0][MV_X] = (mv[REFP_0][MV_X] >> pi->curr_mvr) << pi->curr_mvr;
        mod_info_curr->mv[REFP_0][MV_Y] = (mv[REFP_0][MV_Y] >> pi->curr_mvr) << pi->curr_mvr;
        mod_info_curr->mv[REFP_1][MV_X] = (mv[REFP_1][MV_X] >> pi->curr_mvr) << pi->curr_mvr;
        mod_info_curr->mv[REFP_1][MV_Y] = (mv[REFP_1][MV_Y] >> pi->curr_mvr) << pi->curr_mvr;

        mod_info_curr->mvd[REFP_0][MV_X] = mod_info_curr->mv[REFP_0][MV_X] - mvp[REFP_0][MV_X];
        mod_info_curr->mvd[REFP_0][MV_Y] = mod_info_curr->mv[REFP_0][MV_Y] - mvp[REFP_0][MV_Y];
        mod_info_curr->mvd[REFP_1][MV_X] = mod_info_curr->mv[REFP_1][MV_X] - mvp[REFP_1][MV_X];
        mod_info_curr->mvd[REFP_1][MV_Y] = mod_info_curr->mv[REFP_1][MV_Y] - mvp[REFP_1][MV_Y];

        /* important: reset mv/mvd */
        // note: after clipping, mvd might not align with amvr index
        int amvr_shift = pi->curr_mvr;
        mod_info_curr->mvd[REFP_0][MV_X] = mod_info_curr->mvd[REFP_0][MV_X] >> amvr_shift << amvr_shift;
        mod_info_curr->mvd[REFP_0][MV_Y] = mod_info_curr->mvd[REFP_0][MV_Y] >> amvr_shift << amvr_shift;

        mod_info_curr->mvd[REFP_1][MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, -mod_info_curr->mvd[REFP_0][MV_X]);
        mod_info_curr->mvd[REFP_1][MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, -mod_info_curr->mvd[REFP_0][MV_Y]);

        // note: mvd = mv - mvp, but because of clipping, mv might not equal to mvp + mvd
        for (int i = 0; i < REFP_NUM; i++)
        {
            int mv_x = (s32)mod_info_curr->mvd[i][MV_X] + mvp[i][MV_X];
            int mv_y = (s32)mod_info_curr->mvd[i][MV_Y] + mvp[i][MV_Y];
            mod_info_curr->mv[i][MV_X] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_x);
            mod_info_curr->mv[i][MV_Y] = COM_CLIP3(COM_INT16_MIN, COM_INT16_MAX, mv_y);
        }

        cost = pinter_residue_rdo(ctx, core, 0);
        check_best_mode(core, pi, cost, cost_best);
    }
}
#endif

int pinter_init_frame(ENC_CTX *ctx)
{
    ENC_PINTER *pi;
    COM_PIC     *pic;
    int size;
    pi = &ctx->pinter;
    pic = pi->pic_org = PIC_ORG(ctx);
    pi->Yuv_org[Y_C] = pic->y;
    pi->Yuv_org[U_C] = pic->u;
    pi->Yuv_org[V_C] = pic->v;
    pi->stride_org[Y_C] = pic->stride_luma;
    pi->stride_org[U_C] = pic->stride_chroma;
    pi->stride_org[V_C] = pic->stride_chroma;

    pic = PIC_REC(ctx);
    pi->refp = ctx->refp;
    pi->slice_type = ctx->slice_type;
    pi->bit_depth = ctx->info.bit_depth_internal;
    pi->map_mv = ctx->map.map_mv;
    pi->pic_width_in_scu = ctx->info.pic_width_in_scu;
    size = sizeof(pel) * MAX_CU_DIM;
    com_mset(pi->pred_buf, 0, size);
    size = sizeof(pel) * N_C * MAX_CU_DIM;
    com_mset(pi->rec_buf, 0, size);
    size = sizeof(s16) * REFP_NUM * MAX_NUM_ACTIVE_REF_FRAME * MV_D;
    com_mset(pi->mvp_scale, 0, size);
    size = sizeof(s16) * REFP_NUM * MAX_NUM_ACTIVE_REF_FRAME * MV_D;
    com_mset(pi->mv_scale, 0, size);
    size = sizeof(int) * MV_D;
    com_mset(pi->max_imv, 0, size);

    size = sizeof(CPMV) * REFP_NUM * MAX_NUM_ACTIVE_REF_FRAME * VER_NUM * MV_D;
    com_mset(pi->affine_mvp_scale, 0, size);
    size = sizeof(CPMV) * REFP_NUM * MAX_NUM_ACTIVE_REF_FRAME * VER_NUM * MV_D;
    com_mset(pi->affine_mv_scale, 0, size);
    size = sizeof(pel) * MAX_CU_DIM;
    com_mset(pi->p_error, 0, size);
    size = sizeof(int) * 2 * MAX_CU_DIM;
    com_mset(pi->i_gradient, 0, size);

    size = sizeof(s16) * MAX_CU_DIM;
    com_mset(pi->org_bi, 0, size);
    size = sizeof(s32) * REFP_NUM;
    com_mset(pi->mot_bits, 0, size);

    return COM_OK;
}

int pinter_init_lcu(ENC_CTX *ctx, ENC_CORE *core)
{
    ENC_PINTER *pi;
    pi = &ctx->pinter;
    pi->lambda_mv = (u32)floor(65536.0 * ctx->sqrt_lambda[0]);
    pi->qp_y = core->qp_y;
    pi->qp_u = core->qp_u;
    pi->qp_v = core->qp_v;
    pi->ptr = ctx->ptr;
    pi->gop_size = ctx->param.gop_size;
    return COM_OK;
}


static void scaled_horizontal_sobel_filter(pel *pred, int pred_stride, int *derivate, int derivate_buf_stride, int width, int height)
{
#if SIMD_AFFINE
    int j, col, row;
    __m128i mm_pred[4];
    __m128i mm2x_pred[2];
    __m128i mm_intermediates[4];
    __m128i mm_derivate[2];
    assert(!(height % 2));
    assert(!(width % 4));
    /* Derivates of the rows and columns at the boundary are done at the end of this function */
    /* The value of col and row indicate the columns and rows for which the derivates have already been computed */
    for (col = 1; (col + 2) < width; col += 2)
    {
        mm_pred[0] = _mm_loadl_epi64((const __m128i *)(&pred[0 * pred_stride + col - 1]));
        mm_pred[1] = _mm_loadl_epi64((const __m128i *)(&pred[1 * pred_stride + col - 1]));
        mm_pred[0] = _mm_cvtepi16_epi32(mm_pred[0]);
        mm_pred[1] = _mm_cvtepi16_epi32(mm_pred[1]);
        for (row = 1; row < (height - 1); row += 2)
        {
            mm_pred[2] = _mm_loadl_epi64((const __m128i *)(&pred[(row + 1) * pred_stride + col - 1]));
            mm_pred[3] = _mm_loadl_epi64((const __m128i *)(&pred[(row + 2) * pred_stride + col - 1]));
            mm_pred[2] = _mm_cvtepi16_epi32(mm_pred[2]);
            mm_pred[3] = _mm_cvtepi16_epi32(mm_pred[3]);
            mm2x_pred[0] = _mm_slli_epi32(mm_pred[1], 1);
            mm2x_pred[1] = _mm_slli_epi32(mm_pred[2], 1);
            mm_intermediates[0] = _mm_add_epi32(mm2x_pred[0], mm_pred[0]);
            mm_intermediates[2] = _mm_add_epi32(mm2x_pred[1], mm_pred[1]);
            mm_intermediates[0] = _mm_add_epi32(mm_intermediates[0], mm_pred[2]);
            mm_intermediates[2] = _mm_add_epi32(mm_intermediates[2], mm_pred[3]);
            mm_pred[0] = mm_pred[2];
            mm_pred[1] = mm_pred[3];
            mm_intermediates[1] = _mm_srli_si128(mm_intermediates[0], 8);
            mm_intermediates[3] = _mm_srli_si128(mm_intermediates[2], 8);
            mm_derivate[0] = _mm_sub_epi32(mm_intermediates[1], mm_intermediates[0]);
            mm_derivate[1] = _mm_sub_epi32(mm_intermediates[3], mm_intermediates[2]);
            _mm_storel_epi64((__m128i *)(&derivate[col + (row + 0) * derivate_buf_stride]), mm_derivate[0]);
            _mm_storel_epi64((__m128i *)(&derivate[col + (row + 1) * derivate_buf_stride]), mm_derivate[1]);
        }
    }
    for (j = 1; j < (height - 1); j++)
    {
        derivate[j * derivate_buf_stride] = derivate[j * derivate_buf_stride + 1];
        derivate[j * derivate_buf_stride + (width - 1)] = derivate[j * derivate_buf_stride + (width - 2)];
    }
    com_mcpy
    (
        derivate,
        derivate + derivate_buf_stride,
        width * sizeof(derivate[0])
    );
    com_mcpy
    (
        derivate + (height - 1) * derivate_buf_stride,
        derivate + (height - 2) * derivate_buf_stride,
        width * sizeof(derivate[0])
    );
#else
    int j, k;
    for (j = 1; j < height - 1; j++)
    {
        for (k = 1; k < width - 1; k++)
        {
            int center = j * pred_stride + k;
            derivate[j * derivate_buf_stride + k] =
                pred[center + 1 - pred_stride] -
                pred[center - 1 - pred_stride] +
                (pred[center + 1] * 2) -
                (pred[center - 1] * 2) +
                pred[center + 1 + pred_stride] -
                pred[center - 1 + pred_stride];
        }
        derivate[j * derivate_buf_stride] = derivate[j * derivate_buf_stride + 1];
        derivate[j * derivate_buf_stride + width - 1] = derivate[j * derivate_buf_stride + width - 2];
    }
    derivate[0] = derivate[derivate_buf_stride + 1];
    derivate[width - 1] = derivate[derivate_buf_stride + width - 2];
    derivate[(height - 1) * derivate_buf_stride] = derivate[(height - 2) * derivate_buf_stride + 1];
    derivate[(height - 1) * derivate_buf_stride + width - 1] = derivate[(height - 2) * derivate_buf_stride + (width - 2)];
    for (j = 1; j < width - 1; j++)
    {
        derivate[j] = derivate[derivate_buf_stride + j];
        derivate[(height - 1) * derivate_buf_stride + j] = derivate[(height - 2) * derivate_buf_stride + j];
    }
#endif
}

static void scaled_vertical_sobel_filter(pel *pred, int pred_stride, int *derivate, int derivate_buf_stride, int width, int height)
{
#if SIMD_AFFINE
    int j, col, row;
    __m128i mm_pred[4];
    __m128i mm_intermediates[6];
    __m128i mm_derivate[2];
    assert(!(height % 2));
    assert(!(width % 4));
    /* Derivates of the rows and columns at the boundary are done at the end of this function */
    /* The value of col and row indicate the columns and rows for which the derivates have already been computed */
    for (col = 1; col < (width - 1); col += 2)
    {
        mm_pred[0] = _mm_loadl_epi64((const __m128i *)(&pred[0 * pred_stride + col - 1]));
        mm_pred[1] = _mm_loadl_epi64((const __m128i *)(&pred[1 * pred_stride + col - 1]));
        mm_pred[0] = _mm_cvtepi16_epi32(mm_pred[0]);
        mm_pred[1] = _mm_cvtepi16_epi32(mm_pred[1]);
        for (row = 1; row < (height - 1); row += 2)
        {
            mm_pred[2] = _mm_loadl_epi64((const __m128i *)(&pred[(row + 1) * pred_stride + col - 1]));
            mm_pred[3] = _mm_loadl_epi64((const __m128i *)(&pred[(row + 2) * pred_stride + col - 1]));
            mm_pred[2] = _mm_cvtepi16_epi32(mm_pred[2]);
            mm_pred[3] = _mm_cvtepi16_epi32(mm_pred[3]);
            mm_intermediates[0] = _mm_sub_epi32(mm_pred[2], mm_pred[0]);
            mm_intermediates[3] = _mm_sub_epi32(mm_pred[3], mm_pred[1]);
            mm_pred[0] = mm_pred[2];
            mm_pred[1] = mm_pred[3];
            mm_intermediates[1] = _mm_srli_si128(mm_intermediates[0], 4);
            mm_intermediates[4] = _mm_srli_si128(mm_intermediates[3], 4);
            mm_intermediates[2] = _mm_srli_si128(mm_intermediates[0], 8);
            mm_intermediates[5] = _mm_srli_si128(mm_intermediates[3], 8);
            mm_intermediates[1] = _mm_slli_epi32(mm_intermediates[1], 1);
            mm_intermediates[4] = _mm_slli_epi32(mm_intermediates[4], 1);
            mm_intermediates[0] = _mm_add_epi32(mm_intermediates[0], mm_intermediates[2]);
            mm_intermediates[3] = _mm_add_epi32(mm_intermediates[3], mm_intermediates[5]);
            mm_derivate[0] = _mm_add_epi32(mm_intermediates[0], mm_intermediates[1]);
            mm_derivate[1] = _mm_add_epi32(mm_intermediates[3], mm_intermediates[4]);
            _mm_storel_epi64((__m128i *) (&derivate[col + (row + 0) * derivate_buf_stride]), mm_derivate[0]);
            _mm_storel_epi64((__m128i *) (&derivate[col + (row + 1) * derivate_buf_stride]), mm_derivate[1]);
        }
    }
    for (j = 1; j < (height - 1); j++)
    {
        derivate[j * derivate_buf_stride] = derivate[j * derivate_buf_stride + 1];
        derivate[j * derivate_buf_stride + (width - 1)] = derivate[j * derivate_buf_stride + (width - 2)];
    }
    com_mcpy
    (
        derivate,
        derivate + derivate_buf_stride,
        width * sizeof(derivate[0])
    );
    com_mcpy
    (
        derivate + (height - 1) * derivate_buf_stride,
        derivate + (height - 2) * derivate_buf_stride,
        width * sizeof(derivate[0])
    );
#else
    int k, j;
    for (k = 1; k < width - 1; k++)
    {
        for (j = 1; j < height - 1; j++)
        {
            int center = j * pred_stride + k;
            derivate[j * derivate_buf_stride + k] =
                pred[center + pred_stride - 1] -
                pred[center - pred_stride - 1] +
                (pred[center + pred_stride] * 2) -
                (pred[center - pred_stride] * 2) +
                pred[center + pred_stride + 1] -
                pred[center - pred_stride + 1];
        }
        derivate[k] = derivate[derivate_buf_stride + k];
        derivate[(height - 1) * derivate_buf_stride + k] = derivate[(height - 2) * derivate_buf_stride + k];
    }
    derivate[0] = derivate[derivate_buf_stride + 1];
    derivate[width - 1] = derivate[derivate_buf_stride + width - 2];
    derivate[(height - 1) * derivate_buf_stride] = derivate[(height - 2) * derivate_buf_stride + 1];
    derivate[(height - 1) * derivate_buf_stride + width - 1] = derivate[(height - 2) * derivate_buf_stride + (width - 2)];
    for (j = 1; j < height - 1; j++)
    {
        derivate[j * derivate_buf_stride] = derivate[j * derivate_buf_stride + 1];
        derivate[j * derivate_buf_stride + width - 1] = derivate[j * derivate_buf_stride + width - 2];
    }
#endif
}

static void equal_coeff_computer(pel *residue, int residue_stride, int **derivate, int derivate_buf_stride, s64(*equal_coeff)[7], int width, int height, int vertex_num)
{
#if SIMD_AFFINE
    int j, k;
    int idx1 = 0, idx2 = 0;
    __m128i mm_two, mm_four;
    __m128i mm_tmp[4];
    __m128i mm_intermediate[4];
    __m128i mm_idx_k, mm_idx_j[2];
    __m128i mm_residue[2];
    // Add directly to indexes to get new index
    mm_two = _mm_set1_epi32(2);
    mm_four = _mm_set1_epi32(4);
    if (vertex_num == 3)
    {
        __m128i mm_c[12];
        //  mm_c - map
        //  C for 1st row of pixels
        //  mm_c[0] = iC[0][i] | iC[0][i+1] | iC[0][i+2] | iC[0][i+3]
        //  mm_c[1] = iC[1][i] | iC[1][i+1] | iC[1][i+2] | iC[1][i+3]
        //  mm_c[2] = iC[2][i] | iC[2][i+1] | iC[2][i+2] | iC[2][i+3]
        //  mm_c[3] = iC[3][i] | iC[3][i+1] | iC[3][i+2] | iC[3][i+3]
        //  mm_c[4] = iC[4][i] | iC[4][i+1] | iC[4][i+2] | iC[4][i+3]
        //  mm_c[5] = iC[5][i] | iC[5][i+1] | iC[5][i+2] | iC[5][i+3]
        //  C for 2nd row of pixels
        //  mm_c[6] = iC[6][i] | iC[6][i+1] | iC[6][i+2] | iC[6][i+3]
        //  mm_c[7] = iC[7][i] | iC[7][i+1] | iC[7][i+2] | iC[7][i+3]
        //  mm_c[8] = iC[8][i] | iC[8][i+1] | iC[8][i+2] | iC[8][i+3]
        //  mm_c[9] = iC[9][i] | iC[9][i+1] | iC[9][i+2] | iC[9][i+3]
        //  mm_c[10] = iC[10][i] | iC[10][i+1] | iC[10][i+2] | iC[10][i+3]
        //  mm_c[11] = iC[11][i] | iC[11][i+1] | iC[11][i+2] | iC[11][i+3]
        idx1 = -2 * derivate_buf_stride - 4;
        idx2 = -derivate_buf_stride - 4;
        mm_idx_j[0] = _mm_set1_epi32(-2);
        mm_idx_j[1] = _mm_set1_epi32(-1);
        for (j = 0; j < height; j += 2)
        {
            mm_idx_j[0] = _mm_add_epi32(mm_idx_j[0], mm_two);
            mm_idx_j[1] = _mm_add_epi32(mm_idx_j[1], mm_two);
            mm_idx_k = _mm_set_epi32(-1, -2, -3, -4);
            idx1 += (derivate_buf_stride << 1);
            idx2 += (derivate_buf_stride << 1);
            for (k = 0; k < width; k += 4)
            {
                idx1 += 4;
                idx2 += 4;
                mm_idx_k = _mm_add_epi32(mm_idx_k, mm_four);
                // 1st row
                mm_c[0] = _mm_loadu_si128((const __m128i*)&derivate[0][idx1]);
                mm_c[2] = _mm_loadu_si128((const __m128i*)&derivate[1][idx1]);
                // 2nd row
                mm_c[6] = _mm_loadu_si128((const __m128i*)&derivate[0][idx2]);
                mm_c[8] = _mm_loadu_si128((const __m128i*)&derivate[1][idx2]);
                // 1st row
                mm_c[1] = _mm_mullo_epi32(mm_idx_k, mm_c[0]);
                mm_c[3] = _mm_mullo_epi32(mm_idx_k, mm_c[2]);
                mm_c[4] = _mm_mullo_epi32(mm_idx_j[0], mm_c[0]);
                mm_c[5] = _mm_mullo_epi32(mm_idx_j[0], mm_c[2]);
                // 2nd row
                mm_c[7] = _mm_mullo_epi32(mm_idx_k, mm_c[6]);
                mm_c[9] = _mm_mullo_epi32(mm_idx_k, mm_c[8]);
                mm_c[10] = _mm_mullo_epi32(mm_idx_j[1], mm_c[6]);
                mm_c[11] = _mm_mullo_epi32(mm_idx_j[1], mm_c[8]);
                // Residue
                mm_residue[0] = _mm_loadl_epi64((const __m128i*)&residue[idx1]);
                mm_residue[1] = _mm_loadl_epi64((const __m128i*)&residue[idx2]);
                mm_residue[0] = _mm_cvtepi16_epi32(mm_residue[0]);
                mm_residue[1] = _mm_cvtepi16_epi32(mm_residue[1]);
                mm_residue[0] = _mm_slli_epi32(mm_residue[0], 3);
                mm_residue[1] = _mm_slli_epi32(mm_residue[1], 3);
                // Calculate residue coefficients first
                mm_tmp[2] = _mm_srli_si128(mm_residue[0], 4);
                mm_tmp[3] = _mm_srli_si128(mm_residue[1], 4);
                // 1st row
                mm_tmp[0] = _mm_srli_si128(mm_c[0], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[6], 4);
                // 7th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][6]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][6], mm_intermediate[3]);
                // 2nd row
                mm_tmp[0] = _mm_srli_si128(mm_c[1], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[7], 4);
                // 7th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[7], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][6]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][6], mm_intermediate[3]);
                // 3rd row
                mm_tmp[0] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[8], 4);
                // 7th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[8], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][6]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][6], mm_intermediate[3]);
                // 4th row
                mm_tmp[0] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[9], 4);
                // 7th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[3], mm_c[9], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[4][6]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][6], mm_intermediate[3]);
                // 5th row
                mm_tmp[0] = _mm_srli_si128(mm_c[4], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[10], 4);
                // 7th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[4], mm_c[10], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[5][6]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][6], mm_intermediate[3]);
                // 6th row
                mm_tmp[0] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[11], 4);
                // 7th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[5], mm_c[11], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[6][6]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][6], mm_intermediate[3]);
                //Start calculation of coefficient matrix
                // 1st row
                mm_tmp[0] = _mm_srli_si128(mm_c[0], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[6], 4);
                // 1st col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_c[0], mm_c[6], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][0]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][0], mm_intermediate[3]);
                // 2nd col of row and 1st col of 2nd row
                mm_tmp[2] = _mm_srli_si128(mm_c[1], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[7], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_c[1], mm_c[7], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][1]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][1], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][0], mm_intermediate[3]);
                // 3rd col of row and 1st col of 3rd row
                mm_tmp[2] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[8], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_c[2], mm_c[8], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][2]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][2], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][0], mm_intermediate[3]);
                // 4th col of row and 1st col of 4th row
                mm_tmp[2] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[9], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_c[3], mm_c[9], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][3], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][0], mm_intermediate[3]);
                // 5th col of row and 1st col of the 5th row
                mm_tmp[2] = _mm_srli_si128(mm_c[4], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[10], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_c[4], mm_c[10], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][4], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][0], mm_intermediate[3]);
                // 6th col of row and 1st col of the 6th row
                mm_tmp[2] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[11], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[6], mm_c[5], mm_c[11], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][5]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][5], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][0], mm_intermediate[3]);
                // 2nd row
                mm_tmp[0] = _mm_srli_si128(mm_c[1], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[7], 4);
                // 2nd col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[7], mm_c[1], mm_c[7], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][1]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][1], mm_intermediate[3]);
                // 3rd col of row and 2nd col of 3rd row
                mm_tmp[2] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[8], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[7], mm_c[2], mm_c[8], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][2]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][2], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][1], mm_intermediate[3]);
                // 4th col of row and 2nd col of 4th row
                mm_tmp[2] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[9], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[7], mm_c[3], mm_c[9], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][3], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][1], mm_intermediate[3]);
                // 5th col of row and 1st col of the 5th row
                mm_tmp[2] = _mm_srli_si128(mm_c[4], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[10], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[7], mm_c[4], mm_c[10], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][4], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][1], mm_intermediate[3]);
                // 6th col of row and 1st col of the 6th row
                mm_tmp[2] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[11], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[7], mm_c[5], mm_c[11], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][5]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][5], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][1], mm_intermediate[3]);
                // 3rd row
                mm_tmp[0] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[8], 4);
                //3rd Col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[8], mm_c[2], mm_c[8], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][2]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][2], mm_intermediate[3]);
                // 4th col of row and 3rd col of 4th row
                mm_tmp[2] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[9], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[8], mm_c[3], mm_c[9], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][3], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][2], mm_intermediate[3]);
                // 5th col of row and 1st col of the 5th row
                mm_tmp[2] = _mm_srli_si128(mm_c[4], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[10], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[8], mm_c[4], mm_c[10], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][4], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][2], mm_intermediate[3]);
                // 6th col of row and 1st col of the 6th row
                mm_tmp[2] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[11], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[8], mm_c[5], mm_c[11], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][5]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][5], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][2], mm_intermediate[3]);
                // 4th row
                mm_tmp[0] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[9], 4);
                // 4th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[3], mm_c[9], mm_c[3], mm_c[9], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[4][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][3], mm_intermediate[3]);
                // 5th col of row and 1st col of the 5th row
                mm_tmp[2] = _mm_srli_si128(mm_c[4], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[10], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[3], mm_c[9], mm_c[4], mm_c[10], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[4][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][4], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][3], mm_intermediate[3]);
                // 6th col of row and 1st col of the 6th row
                mm_tmp[2] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[11], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[3], mm_c[9], mm_c[5], mm_c[11], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[4][5]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][5], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][3], mm_intermediate[3]);
                // 5th row
                mm_tmp[0] = _mm_srli_si128(mm_c[4], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[10], 4);
                // 5th col of row and 1st col of the 5th row
                CALC_EQUAL_COEFF_8PXLS(mm_c[4], mm_c[10], mm_c[4], mm_c[10], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[5][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][4], mm_intermediate[3]);
                // 6th col of row and 1st col of the 6th row
                mm_tmp[2] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[11], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[4], mm_c[10], mm_c[5], mm_c[11], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[5][5]);
                _mm_storel_epi64((__m128i*)&equal_coeff[5][5], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][4], mm_intermediate[3]);
                // 6th row
                mm_tmp[0] = _mm_srli_si128(mm_c[5], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[11], 4);
                // 5th col of row and 1st col of the 5th row
                CALC_EQUAL_COEFF_8PXLS(mm_c[5], mm_c[11], mm_c[5], mm_c[11], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[6][5]);
                _mm_storel_epi64((__m128i*)&equal_coeff[6][5], mm_intermediate[3]);
            }
            idx1 -= (width);
            idx2 -= (width);
        }
    }
    else
    {
        __m128i mm_c[8];
        //  mm_c - map
        //  C for 1st row of pixels
        //  mm_c[0] = iC[0][i] | iC[0][i+1] | iC[0][i+2] | iC[0][i+3]
        //  mm_c[1] = iC[1][i] | iC[1][i+1] | iC[1][i+2] | iC[1][i+3]
        //  mm_c[2] = iC[2][i] | iC[2][i+1] | iC[2][i+2] | iC[2][i+3]
        //  mm_c[3] = iC[3][i] | iC[3][i+1] | iC[3][i+2] | iC[3][i+3]
        //  C for 2nd row of pixels
        //  mm_c[4] = iC[0][i] | iC[0][i+1] | iC[0][i+2] | iC[0][i+3]
        //  mm_c[5] = iC[1][i] | iC[1][i+1] | iC[1][i+2] | iC[1][i+3]
        //  mm_c[6] = iC[2][i] | iC[2][i+1] | iC[2][i+2] | iC[2][i+3]
        //  mm_c[7] = iC[3][i] | iC[3][i+1] | iC[3][i+2] | iC[3][i+3]
        idx1 = -2 * derivate_buf_stride - 4;
        idx2 = -derivate_buf_stride - 4;
        mm_idx_j[0] = _mm_set1_epi32(-2);
        mm_idx_j[1] = _mm_set1_epi32(-1);
        for (j = 0; j < height; j += 2)
        {
            mm_idx_j[0] = _mm_add_epi32(mm_idx_j[0], mm_two);
            mm_idx_j[1] = _mm_add_epi32(mm_idx_j[1], mm_two);
            mm_idx_k = _mm_set_epi32(-1, -2, -3, -4);
            idx1 += (derivate_buf_stride << 1);
            idx2 += (derivate_buf_stride << 1);
            for (k = 0; k < width; k += 4)
            {
                idx1 += 4;
                idx2 += 4;
                mm_idx_k = _mm_add_epi32(mm_idx_k, mm_four);
                mm_c[0] = _mm_loadu_si128((const __m128i*)&derivate[0][idx1]);
                mm_c[2] = _mm_loadu_si128((const __m128i*)&derivate[1][idx1]);
                mm_c[4] = _mm_loadu_si128((const __m128i*)&derivate[0][idx2]);
                mm_c[6] = _mm_loadu_si128((const __m128i*)&derivate[1][idx2]);
                mm_c[1] = _mm_mullo_epi32(mm_idx_k, mm_c[0]);
                mm_c[3] = _mm_mullo_epi32(mm_idx_j[0], mm_c[0]);
                mm_c[5] = _mm_mullo_epi32(mm_idx_k, mm_c[4]);
                mm_c[7] = _mm_mullo_epi32(mm_idx_j[1], mm_c[4]);
                mm_residue[0] = _mm_loadl_epi64((const __m128i*)&residue[idx1]);
                mm_residue[1] = _mm_loadl_epi64((const __m128i*)&residue[idx2]);
                mm_tmp[0] = _mm_mullo_epi32(mm_idx_j[0], mm_c[2]);
                mm_tmp[1] = _mm_mullo_epi32(mm_idx_k, mm_c[2]);
                mm_tmp[2] = _mm_mullo_epi32(mm_idx_j[1], mm_c[6]);
                mm_tmp[3] = _mm_mullo_epi32(mm_idx_k, mm_c[6]);
                mm_residue[0] = _mm_cvtepi16_epi32(mm_residue[0]);
                mm_residue[1] = _mm_cvtepi16_epi32(mm_residue[1]);
                mm_c[1] = _mm_add_epi32(mm_c[1], mm_tmp[0]);
                mm_c[3] = _mm_sub_epi32(mm_c[3], mm_tmp[1]);
                mm_c[5] = _mm_add_epi32(mm_c[5], mm_tmp[2]);
                mm_c[7] = _mm_sub_epi32(mm_c[7], mm_tmp[3]);
                mm_residue[0] = _mm_slli_epi32(mm_residue[0], 3);
                mm_residue[1] = _mm_slli_epi32(mm_residue[1], 3);
                //Start calculation of coefficient matrix
                // 1st row
                mm_tmp[0] = _mm_srli_si128(mm_c[0], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[4], 4);
                // 1st col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[4], mm_c[0], mm_c[4], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][0]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][0], mm_intermediate[3]);
                // 2nd col of row and 1st col of 2nd row
                mm_tmp[2] = _mm_srli_si128(mm_c[1], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[5], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[4], mm_c[1], mm_c[5], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][1]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][1], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][0], mm_intermediate[3]);
                // 3rd col of row and 1st col of 3rd row
                mm_tmp[2] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[6], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[4], mm_c[2], mm_c[6], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][2]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][2], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][0], mm_intermediate[3]);
                // 4th col of row and 1st col of 4th row
                mm_tmp[2] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[7], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[4], mm_c[3], mm_c[7], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][3], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][0], mm_intermediate[3]);
                // 5th col of row
                mm_tmp[2] = _mm_srli_si128(mm_residue[0], 4);
                mm_tmp[3] = _mm_srli_si128(mm_residue[1], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[0], mm_c[4], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[1][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[1][4], mm_intermediate[3]);
                // 2nd row
                mm_tmp[0] = _mm_srli_si128(mm_c[1], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[5], 4);
                // 2nd col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[5], mm_c[1], mm_c[5], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][1]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][1], mm_intermediate[3]);
                // 3rd col of row and 2nd col of 3rd row
                mm_tmp[2] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[6], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[5], mm_c[2], mm_c[6], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][2]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][2], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][1], mm_intermediate[3]);
                // 4th col of row and 2nd col of 4th row
                mm_tmp[2] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[7], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[5], mm_c[3], mm_c[7], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][3], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][1], mm_intermediate[3]);
                // 5th col of row
                mm_tmp[2] = _mm_srli_si128(mm_residue[0], 4);
                mm_tmp[3] = _mm_srli_si128(mm_residue[1], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[1], mm_c[5], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[2][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[2][4], mm_intermediate[3]);
                // 3rd row
                mm_tmp[0] = _mm_srli_si128(mm_c[2], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[6], 4);
                //3rd Col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[6], mm_c[2], mm_c[6], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][2]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][2], mm_intermediate[3]);
                // 4th col of row and 3rd col of 4th row
                mm_tmp[2] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[3] = _mm_srli_si128(mm_c[7], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[6], mm_c[3], mm_c[7], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][3], mm_intermediate[3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][2], mm_intermediate[3]);
                // 5th col of row
                mm_tmp[2] = _mm_srli_si128(mm_residue[0], 4);
                mm_tmp[3] = _mm_srli_si128(mm_residue[1], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[2], mm_c[6], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[3][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[3][4], mm_intermediate[3]);
                // 4th row
                mm_tmp[0] = _mm_srli_si128(mm_c[3], 4);
                mm_tmp[1] = _mm_srli_si128(mm_c[7], 4);
                // 4th col of row
                CALC_EQUAL_COEFF_8PXLS(mm_c[3], mm_c[7], mm_c[3], mm_c[7], mm_tmp[0], mm_tmp[1], mm_tmp[0], mm_tmp[1], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[4][3]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][3], mm_intermediate[3]);
                // 5th col of row
                mm_tmp[2] = _mm_srli_si128(mm_residue[0], 4);
                mm_tmp[3] = _mm_srli_si128(mm_residue[1], 4);
                CALC_EQUAL_COEFF_8PXLS(mm_c[3], mm_c[7], mm_residue[0], mm_residue[1], mm_tmp[0], mm_tmp[1], mm_tmp[2], mm_tmp[3], mm_intermediate[0], mm_intermediate[1], mm_intermediate[2], mm_intermediate[3], (const __m128i*)&equal_coeff[4][4]);
                _mm_storel_epi64((__m128i*)&equal_coeff[4][4], mm_intermediate[3]);
            }
            idx1 -= (width);
            idx2 -= (width);
        }
    }
#else
    int affine_param_num = (vertex_num << 1);
    int j, k, col, row;
    for (j = 0; j != height; j++)
    {
        for (k = 0; k != width; k++)
        {
            s64 intermediates[2];
            int iC[6];
            int iIdx = j * derivate_buf_stride + k;
            if (vertex_num == 2)
            {
                iC[0] = derivate[0][iIdx];
                iC[1] = k * derivate[0][iIdx];
                iC[1] += j * derivate[1][iIdx];
                iC[2] = derivate[1][iIdx];
                iC[3] = j * derivate[0][iIdx];
                iC[3] -= k * derivate[1][iIdx];
            }
            else
            {
                iC[0] = derivate[0][iIdx];
                iC[1] = k * derivate[0][iIdx];
                iC[2] = derivate[1][iIdx];
                iC[3] = k * derivate[1][iIdx];
                iC[4] = j * derivate[0][iIdx];
                iC[5] = j * derivate[1][iIdx];
            }
            for (col = 0; col < affine_param_num; col++)
            {
                intermediates[0] = iC[col];
                for (row = 0; row < affine_param_num; row++)
                {
                    intermediates[1] = intermediates[0] * iC[row];
                    equal_coeff[col + 1][row] += intermediates[1];
                }
                intermediates[1] = intermediates[0] * residue[iIdx];
                equal_coeff[col + 1][affine_param_num] += intermediates[1] * 8;
            }
        }
    }
#endif
}

void solve_equal(double(*equal_coeff)[7], int order, double* affine_para)
{
    int i, j, k;
    // row echelon
    for (i = 1; i < order; i++)
    {
        // find column max
        double temp = fabs(equal_coeff[i][i - 1]);
        int temp_idx = i;
        for (j = i + 1; j < order + 1; j++)
        {
            if (fabs(equal_coeff[j][i - 1]) > temp)
            {
                temp = fabs(equal_coeff[j][i - 1]);
                temp_idx = j;
            }
        }
        // swap line
        if (temp_idx != i)
        {
            for (j = 0; j < order + 1; j++)
            {
                equal_coeff[0][j] = equal_coeff[i][j];
                equal_coeff[i][j] = equal_coeff[temp_idx][j];
                equal_coeff[temp_idx][j] = equal_coeff[0][j];
            }
        }
        // elimination first column
        for (j = i + 1; j < order + 1; j++)
        {
            for (k = i; k < order + 1; k++)
            {
                equal_coeff[j][k] = equal_coeff[j][k] - equal_coeff[i][k] * equal_coeff[j][i - 1] / equal_coeff[i][i - 1];
            }
        }
    }
    affine_para[order - 1] = equal_coeff[order][order] / equal_coeff[order][order - 1];
    for (i = order - 2; i >= 0; i--)
    {
        double temp = 0;
        for (j = i + 1; j < order; j++)
        {
            temp += equal_coeff[i + 1][j] * affine_para[j];
        }
        affine_para[i] = (equal_coeff[i + 1][order] - temp) / equal_coeff[i + 1][i];
    }
}

static int get_affine_mv_bits(CPMV mv[VER_NUM][MV_D], CPMV mvp[VER_NUM][MV_D], int num_refp, int refi, int vertex_num
#if BD_AFFINE_AMVR
                              , u8 curr_mvr
#endif
                             )
{
    int bits = 0;
    int vertex;
    for (vertex = 0; vertex < vertex_num; vertex++)
    {
        int mvd_x = mv[vertex][MV_X] - mvp[vertex][MV_X];
        int mvd_y = mv[vertex][MV_Y] - mvp[vertex][MV_Y];
#if BD_AFFINE_AMVR
        u8 amvr_shift = Tab_Affine_AMVR(curr_mvr);
        // note: if clipped to MAX value, the mv/mvp might not align with amvr shift
        if (mv[vertex][MV_X] != COM_CPMV_MAX && mvp[vertex][MV_X] != COM_CPMV_MAX)
            assert(mvd_x == ((mvd_x >> amvr_shift) << amvr_shift));
        if (mv[vertex][MV_Y] != COM_CPMV_MAX && mvp[vertex][MV_Y] != COM_CPMV_MAX)
            assert(mvd_y == ((mvd_y >> amvr_shift) << amvr_shift));
        mvd_x >>= amvr_shift;
        mvd_y >>= amvr_shift;
#endif
        bits += (mvd_x > 2048 || mvd_x <= -2048) ? get_exp_golomb_bits(COM_ABS(mvd_x)) : enc_tbl_mv_bits[mvd_x];
        bits += (mvd_y > 2048 || mvd_y <= -2048) ? get_exp_golomb_bits(COM_ABS(mvd_y)) : enc_tbl_mv_bits[mvd_y];
    }
    bits += enc_tbl_refi_bits[num_refp][refi];
    return bits;
}

static u32 pinter_affine_me_gradient(ENC_PINTER * pi, int x, int y, int cu_width_log2, int cu_height_log2, s8 * refi, int lidx, CPMV mvp[VER_NUM][MV_D], CPMV mv[VER_NUM][MV_D], int bi, int vertex_num, int sub_w, int sub_h)
{
    int bit_depth = pi->bit_depth;
    CPMV mvt[VER_NUM][MV_D];
    s16 mvd[VER_NUM][MV_D];
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;
    u32 cost, cost_best = COM_UINT32_MAX;
    s8 ri = *refi;
    COM_PIC* refp = pi->refp[ri][lidx].pic;
    pel *pred = pi->pred_buf;
    pel *org = bi ? pi->org_bi : (pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C]);
    int s_org = bi ? cu_width : pi->stride_org[Y_C];
    int mv_bits, best_bits;
    int vertex, iter;
    int iter_num = bi ? AF_ITER_BI : AF_ITER_UNI;
    int para_num = (vertex_num << 1) + 1;
    int affine_param_num = para_num - 1;
    double affine_para[6];
    double delta_mv[6];
    s64    equal_coeff_t[7][7];
    double equal_coeff[7][7];
    pel    *error = pi->p_error;
    int    *derivate[2];
    derivate[0] = pi->i_gradient[0];
    derivate[1] = pi->i_gradient[1];
    cu_width = 1 << cu_width_log2;
    cu_height = 1 << cu_height_log2;

    /* set start mv */
    for (vertex = 0; vertex < vertex_num; vertex++)
    {
        mvt[vertex][MV_X] = mv[vertex][MV_X];
        mvt[vertex][MV_Y] = mv[vertex][MV_Y];
        mvd[vertex][MV_X] = 0;
        mvd[vertex][MV_Y] = 0;
    }
    /* do motion compensation with start mv */
    com_affine_mc_l(x, y, refp->width_luma, refp->height_luma, cu_width, cu_height, mvt, refp, pred, vertex_num, sub_w, sub_h, bit_depth);

    /* get mvd bits*/
    best_bits = get_affine_mv_bits(mvt, mvp, pi->num_refp, ri, vertex_num
#if BD_AFFINE_AMVR
                                   , pi->curr_mvr
#endif
                                  );
    if (bi)
    {
        best_bits += pi->mot_bits[1 - lidx];
    }
    cost_best = MV_COST(pi, best_bits);

    /* get satd */
    cost_best += calc_satd_16b(cu_width, cu_height, org, pred, s_org, cu_width, bit_depth) >> bi;
    if (vertex_num == 3)
    {
        iter_num = bi ? (AF_ITER_BI - 2) : (AF_ITER_UNI - 2);
    }
    for (iter = 0; iter < iter_num; iter++)
    {
        int row, col;
        int all_zero = 0;
        enc_diff_16b(cu_width_log2, cu_height_log2, org, pred, s_org, cu_width, cu_width, error);
        // sobel x direction
        // -1 0 1
        // -2 0 2
        // -1 0 1
        scaled_horizontal_sobel_filter(pred, cu_width, derivate[0], cu_width, cu_width, cu_height);
        // sobel y direction
        // -1 -2 -1
        //  0  0  0
        //  1  2  1
        scaled_vertical_sobel_filter(pred, cu_width, derivate[1], cu_width, cu_width, cu_height);
        // solve delta x and y
        for (row = 0; row < para_num; row++)
        {
            com_mset(&equal_coeff_t[row][0], 0, para_num * sizeof(s64));
        }
        equal_coeff_computer(error, cu_width, derivate, cu_width, equal_coeff_t, cu_width, cu_height, vertex_num);
        for (row = 0; row < para_num; row++)
        {
            for (col = 0; col < para_num; col++)
            {
                equal_coeff[row][col] = (double)equal_coeff_t[row][col];
            }
        }
        solve_equal(equal_coeff, affine_param_num, affine_para);
        // convert to delta mv
        if (vertex_num == 3)
        {
            // for MV0
            delta_mv[0] = affine_para[0];
            delta_mv[2] = affine_para[2];
            // for MV1
            delta_mv[1] = affine_para[1] * cu_width + affine_para[0];
            delta_mv[3] = affine_para[3] * cu_width + affine_para[2];
            // for MV2
            delta_mv[4] = affine_para[4] * cu_height + affine_para[0];
            delta_mv[5] = affine_para[5] * cu_height + affine_para[2];
#if BD_AFFINE_AMVR
            u8 amvr_shift = Tab_Affine_AMVR(pi->curr_mvr);
            if (amvr_shift == 0)
            {
                mvd[0][MV_X] = (s16)(delta_mv[0] * 16 + (delta_mv[0] >= 0 ? 0.5 : -0.5)); //  1/16-pixel
                mvd[0][MV_Y] = (s16)(delta_mv[2] * 16 + (delta_mv[2] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_X] = (s16)(delta_mv[1] * 16 + (delta_mv[1] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_Y] = (s16)(delta_mv[3] * 16 + (delta_mv[3] >= 0 ? 0.5 : -0.5));
                mvd[2][MV_X] = (s16)(delta_mv[4] * 16 + (delta_mv[4] >= 0 ? 0.5 : -0.5));
                mvd[2][MV_Y] = (s16)(delta_mv[5] * 16 + (delta_mv[5] >= 0 ? 0.5 : -0.5));
            }
            else
            {
                mvd[0][MV_X] = (s16)(delta_mv[0] * 4 + (delta_mv[0] >= 0 ? 0.5 : -0.5));
                mvd[0][MV_Y] = (s16)(delta_mv[2] * 4 + (delta_mv[2] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_X] = (s16)(delta_mv[1] * 4 + (delta_mv[1] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_Y] = (s16)(delta_mv[3] * 4 + (delta_mv[3] >= 0 ? 0.5 : -0.5));
                mvd[2][MV_X] = (s16)(delta_mv[1] * 4 + (delta_mv[4] >= 0 ? 0.5 : -0.5));
                mvd[2][MV_Y] = (s16)(delta_mv[3] * 4 + (delta_mv[5] >= 0 ? 0.5 : -0.5));
                mvd[0][MV_X] <<= 2; // 1/16-pixel
                mvd[0][MV_Y] <<= 2;
                mvd[1][MV_X] <<= 2;
                mvd[1][MV_Y] <<= 2;
                mvd[2][MV_X] <<= 2;
                mvd[2][MV_Y] <<= 2;
                if (amvr_shift > 0)
                {
                    com_mv_rounding_s16(mvd[0][MV_X], mvd[0][MV_Y], &mvd[0][MV_X], &mvd[0][MV_Y], amvr_shift, amvr_shift);
                    com_mv_rounding_s16(mvd[1][MV_X], mvd[1][MV_Y], &mvd[1][MV_X], &mvd[1][MV_Y], amvr_shift, amvr_shift);
                    com_mv_rounding_s16(mvd[2][MV_X], mvd[2][MV_Y], &mvd[2][MV_X], &mvd[2][MV_Y], amvr_shift, amvr_shift);
                }
            }
#else
            mvd[0][MV_X] = (s16)(delta_mv[0] * 4 + (delta_mv[0] >= 0 ? 0.5 : -0.5));
            mvd[0][MV_Y] = (s16)(delta_mv[2] * 4 + (delta_mv[2] >= 0 ? 0.5 : -0.5));
            mvd[1][MV_X] = (s16)(delta_mv[1] * 4 + (delta_mv[1] >= 0 ? 0.5 : -0.5));
            mvd[1][MV_Y] = (s16)(delta_mv[3] * 4 + (delta_mv[3] >= 0 ? 0.5 : -0.5));
            mvd[2][MV_X] = (s16)(delta_mv[4] * 4 + (delta_mv[4] >= 0 ? 0.5 : -0.5));
            mvd[2][MV_Y] = (s16)(delta_mv[5] * 4 + (delta_mv[5] >= 0 ? 0.5 : -0.5));
#endif
        }
        else
        {
            // for MV0
            delta_mv[0] = affine_para[0];
            delta_mv[2] = affine_para[2];
            // for MV1
            delta_mv[1] = affine_para[1] * cu_width + affine_para[0];
            delta_mv[3] = -affine_para[3] * cu_width + affine_para[2];
#if BD_AFFINE_AMVR
            u8 amvr_shift = Tab_Affine_AMVR(pi->curr_mvr);
            if (amvr_shift == 0)
            {
                mvd[0][MV_X] = (s16)(delta_mv[0] * 16 + (delta_mv[0] >= 0 ? 0.5 : -0.5));
                mvd[0][MV_Y] = (s16)(delta_mv[2] * 16 + (delta_mv[2] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_X] = (s16)(delta_mv[1] * 16 + (delta_mv[1] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_Y] = (s16)(delta_mv[3] * 16 + (delta_mv[3] >= 0 ? 0.5 : -0.5));
            }
            else
            {
                mvd[0][MV_X] = (s16)(delta_mv[0] * 4 + (delta_mv[0] >= 0 ? 0.5 : -0.5));
                mvd[0][MV_Y] = (s16)(delta_mv[2] * 4 + (delta_mv[2] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_X] = (s16)(delta_mv[1] * 4 + (delta_mv[1] >= 0 ? 0.5 : -0.5));
                mvd[1][MV_Y] = (s16)(delta_mv[3] * 4 + (delta_mv[3] >= 0 ? 0.5 : -0.5));
                mvd[0][MV_X] <<= 2;//  1/16-pixel
                mvd[0][MV_Y] <<= 2;
                mvd[1][MV_X] <<= 2;
                mvd[1][MV_Y] <<= 2;
                if (amvr_shift > 0)
                {
                    com_mv_rounding_s16(mvd[0][MV_X], mvd[0][MV_Y], &mvd[0][MV_X], &mvd[0][MV_Y], amvr_shift, amvr_shift);
                    com_mv_rounding_s16(mvd[1][MV_X], mvd[1][MV_Y], &mvd[1][MV_X], &mvd[1][MV_Y], amvr_shift, amvr_shift);
                }
            }
#else
            mvd[0][MV_X] = (s16)(delta_mv[0] * 4 + (delta_mv[0] >= 0 ? 0.5 : -0.5));
            mvd[0][MV_Y] = (s16)(delta_mv[2] * 4 + (delta_mv[2] >= 0 ? 0.5 : -0.5));
            mvd[1][MV_X] = (s16)(delta_mv[1] * 4 + (delta_mv[1] >= 0 ? 0.5 : -0.5));
            mvd[1][MV_Y] = (s16)(delta_mv[3] * 4 + (delta_mv[3] >= 0 ? 0.5 : -0.5));
#endif
        }

        // check early terminate
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            if (mvd[vertex][MV_X] != 0 || mvd[vertex][MV_Y] != 0)
            {
                all_zero = 0;
                break;
            }
            all_zero = 1;
        }
        if (all_zero)
        {
            break;
        }

        /* update mv */
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            s32 mvx = (s32)mvt[vertex][MV_X] + (s32)mvd[vertex][MV_X];
            s32 mvy = (s32)mvt[vertex][MV_Y] + (s32)mvd[vertex][MV_Y];
            mvt[vertex][MV_X] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mvx);
            mvt[vertex][MV_Y] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mvy);
#if BD_AFFINE_AMVR
            // after clipping, last 2/4 bits of mv may not be zero when amvr_shift is 2/4, perform rounding without offset
            u8 amvr_shift = Tab_Affine_AMVR(pi->curr_mvr);
            if (mvt[vertex][MV_X] == COM_CPMV_MAX)
            {
                mvt[vertex][MV_X] = mvt[vertex][MV_X] >> amvr_shift << amvr_shift;
            }
            if (mvt[vertex][MV_Y] == COM_CPMV_MAX)
            {
                mvt[vertex][MV_Y] = mvt[vertex][MV_Y] >> amvr_shift << amvr_shift;
            }
#endif
        }
        /* do motion compensation with updated mv */
        com_affine_mc_l(x, y, refp->width_luma, refp->height_luma, cu_width, cu_height, mvt, refp, pred, vertex_num, sub_w, sub_h, bit_depth);
        /* get mvd bits*/
        mv_bits = get_affine_mv_bits(mvt, mvp, pi->num_refp, ri, vertex_num
#if BD_AFFINE_AMVR
                                     , pi->curr_mvr
#endif
                                    );
        if (bi)
        {
            mv_bits += pi->mot_bits[1 - lidx];
        }
        cost = MV_COST(pi, mv_bits);
        /* get satd */
        cost += calc_satd_16b(cu_width, cu_height, org, pred, s_org, cu_width, bit_depth) >> bi;
        /* save best mv */
        if (cost < cost_best)
        {
            cost_best = cost;
            best_bits = mv_bits;
            for (vertex = 0; vertex < vertex_num; vertex++)
            {
                mv[vertex][MV_X] = mvt[vertex][MV_X];
                mv[vertex][MV_Y] = mvt[vertex][MV_Y];
            }
        }
    }
    return (cost_best - MV_COST(pi, best_bits));
}

static void analyze_affine_uni(ENC_CTX * ctx, ENC_CORE * core, CPMV aff_mv_L0L1[REFP_NUM][VER_NUM][MV_D], s8 *refi_L0L1, double * cost_L0L1, double *cost_best)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    int bit_depth = ctx->info.bit_depth_internal;
    int lidx;
    s8* refi;
    s8 refi_cur;
    u32 mecost, best_mecost;
    s8 t0, t1;
    s8 refi_temp = 0;
    CPMV(*affine_mvp)[MV_D], (*affine_mv)[MV_D];
    s16 (*affine_mvd)[MV_D];
    int vertex = 0;
    int vertex_num = 2;
    int mebits, best_bits = 0;
    u32 cost_trans[REFP_NUM][MAX_NUM_ACTIVE_REF_FRAME];
    CPMV tmp_mv_array[VER_NUM][MV_D];
    int memory_access;
    int allowed = 1;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;
    int mem = MAX_MEMORY_ACCESS_UNI * (1 << cu_width_log2) * (1 << cu_height_log2);

    int sub_w = 4;
    int sub_h = 4;
    if (ctx->info.pic_header.affine_subblock_size_idx > 0)
    {
        sub_w = 8;
        sub_h = 8;
    }

    /* AFFINE 4 parameters Motion Search */
    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->tb_part, &mod_info_curr->tb_info);
    mod_info_curr->cu_mode = MODE_INTER;
    mod_info_curr->affine_flag = 1;
    for (lidx = 0; lidx <= ((pi->slice_type == SLICE_P) ? PRED_L0 : PRED_L1); lidx++)
    {
        init_inter_data(pi, core);
        refi = mod_info_curr->refi;
        affine_mv = mod_info_curr->affine_mv[lidx];
        affine_mvd = mod_info_curr->affine_mvd[lidx];
        pi->num_refp = (u8)ctx->rpm.num_refp[lidx];
        best_mecost = COM_UINT32_MAX;
        for (refi_cur = 0; refi_cur < pi->num_refp; refi_cur++)
        {
            affine_mvp = pi->affine_mvp_scale[lidx][refi_cur];
            mod_info_curr->refi[lidx] = refi_cur;
            com_get_affine_mvp_scaling(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, lidx, affine_mvp, vertex_num
#if BD_AFFINE_AMVR
                                       , pi->curr_mvr
#endif
                                      );
            {
                u32 mvp_best = COM_UINT32_MAX;
                u32 mvp_temp = COM_UINT32_MAX;
                s8  refi_t[REFP_NUM];
                COM_PIC* refp = pi->refp[refi_cur][lidx].pic;
                pel *pred = pi->pred_buf;
                pel *org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];
                int s_org = pi->stride_org[Y_C];

                // get cost of mvp
                com_affine_mc_l(x, y, refp->width_luma, refp->height_luma, cu_width, cu_height, affine_mvp, refp, pred, vertex_num, sub_w, sub_h, bit_depth);
                mvp_best = calc_satd_16b(cu_width, cu_height, org, pred, s_org, cu_width, bit_depth);
                mebits = get_affine_mv_bits(affine_mvp, affine_mvp, pi->num_refp, refi_cur, vertex_num
#if BD_AFFINE_AMVR
                                            , pi->curr_mvr
#endif
                                           );
                mvp_best += MV_COST(pi, mebits);

                // get cost of translational mv
                s16 mv_cliped[REFP_NUM][MV_D];
                mv_cliped[lidx][MV_X] = (s16)(pi->best_mv_uni[lidx][refi_cur][MV_X]);
                mv_cliped[lidx][MV_Y] = (s16)(pi->best_mv_uni[lidx][refi_cur][MV_Y]);

                refi_t[lidx] = refi_cur;
                refi_t[1 - lidx] = -1;
                mv_clip(x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, refi_t, mv_cliped, mv_cliped);
                for (vertex = 0; vertex < vertex_num; vertex++)
                {
                    tmp_mv_array[vertex][MV_X] = mv_cliped[lidx][MV_X];
                    tmp_mv_array[vertex][MV_Y] = mv_cliped[lidx][MV_Y];
#if BD_AFFINE_AMVR
                    s32 tmp_mvx, tmp_mvy;
                    tmp_mvx = tmp_mv_array[vertex][MV_X] << 2;
                    tmp_mvy = tmp_mv_array[vertex][MV_Y] << 2;
                    if (pi->curr_mvr == 1)
                    {
                        com_mv_rounding_s32(tmp_mvx, tmp_mvy, &tmp_mvx, &tmp_mvy, 4, 4);
                    }
                    tmp_mv_array[vertex][MV_X] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, tmp_mvx);
                    tmp_mv_array[vertex][MV_Y] = (CPMV)COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, tmp_mvy);
#endif
                }
                com_affine_mc_l(x, y, refp->width_luma, refp->height_luma, cu_width, cu_height, tmp_mv_array, refp, pred, vertex_num, sub_w, sub_h, bit_depth);
                cost_trans[lidx][refi_cur] = calc_satd_16b(cu_width, cu_height, org, pred, s_org, cu_width, bit_depth);
                mebits = get_affine_mv_bits(tmp_mv_array, affine_mvp, pi->num_refp, refi_cur, vertex_num
#if BD_AFFINE_AMVR
                                            , pi->curr_mvr
#endif
                                           );
                mvp_temp = cost_trans[lidx][refi_cur] + MV_COST(pi, mebits);
                if (mvp_temp < mvp_best)
                {
                    for (vertex = 0; vertex < vertex_num; vertex++)
                    {
                        affine_mv[vertex][MV_X] = tmp_mv_array[vertex][MV_X];
                        affine_mv[vertex][MV_Y] = tmp_mv_array[vertex][MV_Y];
                    }
                }
                else
                {
                    for (vertex = 0; vertex < vertex_num; vertex++)
                    {
                        affine_mv[vertex][MV_X] = affine_mvp[vertex][MV_X];
                        affine_mv[vertex][MV_Y] = affine_mvp[vertex][MV_Y];
                    }
                }
            }

            /* affine motion search */
            mecost = pi->fn_affine_me(pi, x, y, cu_width_log2, cu_height_log2, &refi_cur, lidx, affine_mvp, affine_mv, 0, vertex_num, sub_w, sub_h);

            // update MVP bits
            t0 = (lidx == 0) ? refi_cur : REFI_INVALID;
            t1 = (lidx == 1) ? refi_cur : REFI_INVALID;
            SET_REFI(refi, t0, t1);
            mebits = get_affine_mv_bits(affine_mv, affine_mvp, pi->num_refp, refi_cur, vertex_num
#if BD_AFFINE_AMVR
                                        , pi->curr_mvr
#endif
                                       );
            mecost += MV_COST(pi, mebits);

            /* save affine per ref me results */
            for (vertex = 0; vertex < vertex_num; vertex++)
            {
                pi->affine_mv_scale[lidx][refi_cur][vertex][MV_X] = affine_mv[vertex][MV_X];
                pi->affine_mv_scale[lidx][refi_cur][vertex][MV_Y] = affine_mv[vertex][MV_Y];
            }
            if (mecost < best_mecost)
            {
                best_mecost = mecost;
                best_bits = mebits;
                refi_temp = refi_cur;
            }
        }
        /* save affine per list me results */
        refi_cur = refi_temp;
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            affine_mv[vertex][MV_X] = pi->affine_mv_scale[lidx][refi_cur][vertex][MV_X];
            affine_mv[vertex][MV_Y] = pi->affine_mv_scale[lidx][refi_cur][vertex][MV_Y];
        }
        affine_mvp = pi->affine_mvp_scale[lidx][refi_cur];
        t0 = (lidx == 0) ? refi_cur : REFI_INVALID;
        t1 = (lidx == 1) ? refi_cur : REFI_INVALID;
        SET_REFI(refi, t0, t1);
        refi_L0L1[lidx] = refi_cur;

        /* get affine mvd */
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            affine_mvd[vertex][MV_X] = affine_mv[vertex][MV_X] - affine_mvp[vertex][MV_X];
            affine_mvd[vertex][MV_Y] = affine_mv[vertex][MV_Y] - affine_mvp[vertex][MV_Y];
        }

        /* important: reset affine mv/mvd */
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
#if BD_AFFINE_AMVR
            // note: after clipping, mvd might not align with amvr index
            int amvr_shift = Tab_Affine_AMVR(pi->curr_mvr);
            affine_mvd[vertex][MV_X] = affine_mvd[vertex][MV_X] >> amvr_shift << amvr_shift;
            affine_mvd[vertex][MV_Y] = affine_mvd[vertex][MV_Y] >> amvr_shift << amvr_shift;
#endif
            // note: mvd = mv - mvp, but because of clipping, mv might not equal to mvp + mvd
            int mv_x = (s32)affine_mvd[vertex][MV_X] + affine_mvp[vertex][MV_X];
            int mv_y = (s32)affine_mvd[vertex][MV_Y] + affine_mvp[vertex][MV_Y];
            affine_mv[vertex][MV_X] = COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_x);
            affine_mv[vertex][MV_Y] = COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_y);
        }

        pi->mot_bits[lidx] = best_bits;
        affine_mv[2][MV_X] = affine_mv[0][MV_X] - (affine_mv[1][MV_Y] - affine_mv[0][MV_Y]) * (s16)cu_height / (s16)cu_width;
        affine_mv[2][MV_Y] = affine_mv[0][MV_Y] + (affine_mv[1][MV_X] - affine_mv[0][MV_X]) * (s16)cu_height / (s16)cu_width;
        affine_mv[3][MV_X] = affine_mv[1][MV_X] - (affine_mv[1][MV_Y] - affine_mv[0][MV_Y]) * (s16)cu_height / (s16)cu_width;
        affine_mv[3][MV_Y] = affine_mv[1][MV_Y] + (affine_mv[1][MV_X] - affine_mv[0][MV_X]) * (s16)cu_height / (s16)cu_width;

        for (vertex = 0; vertex < vertex_num; vertex++)
        {
            aff_mv_L0L1[lidx][vertex][MV_X] = affine_mv[vertex][MV_X];
            aff_mv_L0L1[lidx][vertex][MV_Y] = affine_mv[vertex][MV_Y];
        }
        allowed = 1;
        memory_access = com_get_affine_memory_access(affine_mv, cu_width, cu_height);
        if (memory_access > mem)
        {
            allowed = 0;
        }
        if (allowed)
        {
            assert(mod_info_curr->mv[REFP_0][MV_X] == 0);
            assert(mod_info_curr->mv[REFP_0][MV_Y] == 0);
            cost_L0L1[lidx] = pinter_residue_rdo(ctx, core, 0);
            check_best_mode(core, pi, cost_L0L1[lidx], cost_best);
        }
    }
}

static void analyze_affine_bi(ENC_CTX * ctx, ENC_CORE * core, CPMV aff_mv_L0L1[REFP_NUM][VER_NUM][MV_D], const s8 *refi_L0L1, const double * cost_L0L1, double *cost_best)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTER *pi = &ctx->pinter;
    int bit_depth = ctx->info.bit_depth_internal;
    s8         refi[REFP_NUM] = { REFI_INVALID, REFI_INVALID };
    u32        best_mecost = COM_UINT32_MAX;
    s8        refi_best = 0, refi_cur;
    int        changed = 0;
    u32        mecost;
    pel        *org;
    pel(*pred)[MAX_CU_DIM];
    int        vertex_num = 2;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;
    s8         t0, t1;
    double      cost;
    s8         lidx_ref, lidx_cnd;
    u8          mvp_idx = 0;
    int         i;
    int         vertex;
    int         mebits;
    int         memory_access[REFP_NUM];
    int         mem = MAX_MEMORY_ACCESS_BI * (1 << cu_width_log2) * (1 << cu_height_log2);
    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->pb_part, &mod_info_curr->pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, mod_info_curr->tb_part, &mod_info_curr->tb_info);
    mod_info_curr->cu_mode = MODE_INTER;
    init_inter_data(pi, core);
    if (cost_L0L1[REFP_0] <= cost_L0L1[REFP_1])
    {
        lidx_ref = REFP_0;
        lidx_cnd = REFP_1;
    }
    else
    {
        lidx_ref = REFP_1;
        lidx_cnd = REFP_0;
    }
    mod_info_curr->refi[REFP_0] = refi_L0L1[REFP_0];
    mod_info_curr->refi[REFP_1] = refi_L0L1[REFP_1];
    for (vertex = 0; vertex < vertex_num; vertex++)
    {
        mod_info_curr->affine_mv[lidx_ref][vertex][MV_X] = aff_mv_L0L1[lidx_ref][vertex][MV_X];
        mod_info_curr->affine_mv[lidx_ref][vertex][MV_Y] = aff_mv_L0L1[lidx_ref][vertex][MV_Y];
        mod_info_curr->affine_mv[lidx_cnd][vertex][MV_X] = aff_mv_L0L1[lidx_cnd][vertex][MV_X];
        mod_info_curr->affine_mv[lidx_cnd][vertex][MV_Y] = aff_mv_L0L1[lidx_cnd][vertex][MV_Y];
    }
    /* get MVP lidx_cnd */
    org = pi->Yuv_org[Y_C] + x + y * pi->stride_org[Y_C];
    pred = mod_info_curr->pred;
    t0 = (lidx_ref == REFP_0) ? mod_info_curr->refi[lidx_ref] : REFI_INVALID;
    t1 = (lidx_ref == REFP_1) ? mod_info_curr->refi[lidx_ref] : REFI_INVALID;
    SET_REFI(refi, t0, t1);
    for (i = 0; i < AFFINE_BI_ITER; i++)
    {
        /* predict reference */
        com_affine_mc_l(x, y, ctx->info.pic_width, ctx->info.pic_height, cu_width, cu_height, mod_info_curr->affine_mv[lidx_ref], pi->refp[refi[lidx_ref]][lidx_ref].pic, pred[0], vertex_num, 8, 8, bit_depth);

        get_org_bi(org, pred[Y_C], pi->stride_org[Y_C], cu_width, cu_height, pi->org_bi, cu_width, 0, 0);
        SWAP(refi[lidx_ref], refi[lidx_cnd], t0);
        SWAP(lidx_ref, lidx_cnd, t0);
        changed = 0;
        for (refi_cur = 0; refi_cur < pi->num_refp; refi_cur++)
        {
            refi[lidx_ref] = refi_cur;
            mecost = pi->fn_affine_me(pi, x, y, cu_width_log2, cu_height_log2, &refi[lidx_ref], lidx_ref, \
                                      pi->affine_mvp_scale[lidx_ref][refi_cur], pi->affine_mv_scale[lidx_ref][refi_cur], 1, vertex_num, 8, 8);

            // update MVP bits
            mebits = get_affine_mv_bits(pi->affine_mv_scale[lidx_ref][refi_cur], pi->affine_mvp_scale[lidx_ref][refi_cur], pi->num_refp, refi_cur, vertex_num
#if BD_AFFINE_AMVR
                                        , pi->curr_mvr
#endif
                                       );
            mebits += pi->mot_bits[1 - lidx_ref]; // add opposite bits
            mecost += MV_COST(pi, mebits);

            if (mecost < best_mecost)
            {
                pi->mot_bits[lidx_ref] = mebits - pi->mot_bits[1 - lidx_ref];
                refi_best = refi_cur;
                best_mecost = mecost;
                changed = 1;
                t0 = (lidx_ref == REFP_0) ? refi_best : mod_info_curr->refi[lidx_cnd];
                t1 = (lidx_ref == REFP_1) ? refi_best : mod_info_curr->refi[lidx_cnd];
                SET_REFI(mod_info_curr->refi, t0, t1);
                for (vertex = 0; vertex < vertex_num; vertex++)
                {
                    mod_info_curr->affine_mv[lidx_ref][vertex][MV_X] = pi->affine_mv_scale[lidx_ref][refi_cur][vertex][MV_X];
                    mod_info_curr->affine_mv[lidx_ref][vertex][MV_Y] = pi->affine_mv_scale[lidx_ref][refi_cur][vertex][MV_Y];
                }
            }
        }
        t0 = (lidx_ref == REFP_0) ? refi_best : REFI_INVALID;
        t1 = (lidx_ref == REFP_1) ? refi_best : REFI_INVALID;
        SET_REFI(refi, t0, t1);
        if (!changed) break;
    }

    // get affine mvd
    for (vertex = 0; vertex < vertex_num; vertex++)
    {
        mod_info_curr->affine_mvd[REFP_0][vertex][MV_X] = mod_info_curr->affine_mv[REFP_0][vertex][MV_X] - pi->affine_mvp_scale[REFP_0][mod_info_curr->refi[REFP_0]][vertex][MV_X];
        mod_info_curr->affine_mvd[REFP_0][vertex][MV_Y] = mod_info_curr->affine_mv[REFP_0][vertex][MV_Y] - pi->affine_mvp_scale[REFP_0][mod_info_curr->refi[REFP_0]][vertex][MV_Y];
        mod_info_curr->affine_mvd[REFP_1][vertex][MV_X] = mod_info_curr->affine_mv[REFP_1][vertex][MV_X] - pi->affine_mvp_scale[REFP_1][mod_info_curr->refi[REFP_1]][vertex][MV_X];
        mod_info_curr->affine_mvd[REFP_1][vertex][MV_Y] = mod_info_curr->affine_mv[REFP_1][vertex][MV_Y] - pi->affine_mvp_scale[REFP_1][mod_info_curr->refi[REFP_1]][vertex][MV_Y];
    }

    /* important: reset affine mv/mvd */
    for (i = 0; i < REFP_NUM; i++)
    {
        for (vertex = 0; vertex < vertex_num; vertex++)
        {
#if BD_AFFINE_AMVR
            // note: after clipping, mvd might not align with amvr index
            int amvr_shift = Tab_Affine_AMVR(pi->curr_mvr);
            mod_info_curr->affine_mvd[i][vertex][MV_X] = mod_info_curr->affine_mvd[i][vertex][MV_X] >> amvr_shift << amvr_shift;
            mod_info_curr->affine_mvd[i][vertex][MV_Y] = mod_info_curr->affine_mvd[i][vertex][MV_Y] >> amvr_shift << amvr_shift;
#endif
            // note: mvd = mv - mvp, but because of clipping, mv might not equal to mvp + mvd
            int mv_x = (s32)mod_info_curr->affine_mvd[i][vertex][MV_X] + pi->affine_mvp_scale[i][mod_info_curr->refi[i]][vertex][MV_X];
            int mv_y = (s32)mod_info_curr->affine_mvd[i][vertex][MV_Y] + pi->affine_mvp_scale[i][mod_info_curr->refi[i]][vertex][MV_Y];
            mod_info_curr->affine_mv[i][vertex][MV_X] = COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_x);
            mod_info_curr->affine_mv[i][vertex][MV_Y] = COM_CLIP3(COM_CPMV_MIN, COM_CPMV_MAX, mv_y);
        }
    }

    for (i = 0; i < REFP_NUM; i++)
    {
        if (vertex_num == 3)
        {
            mod_info_curr->affine_mv[i][3][MV_X] = mod_info_curr->affine_mv[i][1][MV_X] + mod_info_curr->affine_mv[i][2][MV_X] - mod_info_curr->affine_mv[i][0][MV_X];
            mod_info_curr->affine_mv[i][3][MV_Y] = mod_info_curr->affine_mv[i][1][MV_Y] + mod_info_curr->affine_mv[i][2][MV_Y] - mod_info_curr->affine_mv[i][0][MV_Y];
        }
        else
        {
            mod_info_curr->affine_mv[i][2][MV_X] = mod_info_curr->affine_mv[i][0][MV_X] - (mod_info_curr->affine_mv[i][1][MV_Y] - mod_info_curr->affine_mv[i][0][MV_Y]) * (s16)cu_height / (s16)cu_width;
            mod_info_curr->affine_mv[i][2][MV_Y] = mod_info_curr->affine_mv[i][0][MV_Y] + (mod_info_curr->affine_mv[i][1][MV_X] - mod_info_curr->affine_mv[i][0][MV_X]) * (s16)cu_height / (s16)cu_width;
            mod_info_curr->affine_mv[i][3][MV_X] = mod_info_curr->affine_mv[i][1][MV_X] - (mod_info_curr->affine_mv[i][1][MV_Y] - mod_info_curr->affine_mv[i][0][MV_Y]) * (s16)cu_height / (s16)cu_width;
            mod_info_curr->affine_mv[i][3][MV_Y] = mod_info_curr->affine_mv[i][1][MV_Y] + (mod_info_curr->affine_mv[i][1][MV_X] - mod_info_curr->affine_mv[i][0][MV_X]) * (s16)cu_height / (s16)cu_width;
        }
        memory_access[i] = com_get_affine_memory_access(mod_info_curr->affine_mv[i], cu_width, cu_height);
    }
    if (memory_access[0] > mem || memory_access[1] > mem)
    {
        return;
    }
    else
    {
        cost = pinter_residue_rdo(ctx, core, 0);
        check_best_mode(core, pi, cost, cost_best);
    }
}

// the best motion vector recorded will be used to determine the search range in the next MVR loop
static void update_best_int_mv(COM_MODE *bst_info, ENC_PINTER *pi)
{
    if (abs(bst_info->mv[REFP_0][MV_X]) > abs(bst_info->mv[REFP_1][MV_X]))
    {
        pi->max_imv[MV_X] = (abs(bst_info->mv[REFP_0][MV_X]) + 1) >> 2;
        if (bst_info->mv[REFP_0][MV_X] < 0)
        {
            pi->max_imv[MV_X] = -1 * pi->max_imv[MV_X];
        }
    }
    else
    {
        pi->max_imv[MV_X] = (abs(bst_info->mv[REFP_1][MV_X]) + 1) >> 2;
        if (bst_info->mv[REFP_1][MV_X] < 0)
        {
            pi->max_imv[MV_X] = -1 * pi->max_imv[MV_X];
        }
    }

    if (abs(bst_info->mv[REFP_0][MV_Y]) > abs(bst_info->mv[REFP_1][MV_Y]))
    {
        pi->max_imv[MV_Y] = (abs(bst_info->mv[REFP_0][MV_Y]) + 1) >> 2;
        if (bst_info->mv[REFP_0][MV_Y] < 0)
        {
            pi->max_imv[MV_Y] = -1 * pi->max_imv[MV_Y];
        }
    }
    else
    {
        pi->max_imv[MV_Y] = (abs(bst_info->mv[REFP_1][MV_Y]) + 1) >> 2;
        if (bst_info->mv[REFP_1][MV_Y] < 0)
        {
            pi->max_imv[MV_Y] = -1 * pi->max_imv[MV_Y];
        }
    }
}

#if EXT_AMVR_HMVP
static int same_mvp(ENC_CORE *core, ENC_CTX *ctx, COM_MOTION hmvp_motion)
{
    COM_MOTION c_motion;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int x_scu = mod_info_curr->x_scu;
    int y_scu = mod_info_curr->y_scu;
    int cu_width_in_scu = mod_info_curr->cu_width >> MIN_CU_LOG2;
    int pic_width_in_scu = ctx->info.pic_width_in_scu;
    u32 *map_scu = ctx->map.map_scu;
    int cnt_hmvp = core->cnt_hmvp_cands;

    int neb_addr = mod_info_curr->scup - ctx->info.pic_width_in_scu + cu_width_in_scu;
    int neb_avaliable = y_scu > 0 && x_scu + cu_width_in_scu < pic_width_in_scu && MCU_GET_CODED_FLAG(map_scu[neb_addr]) && !MCU_GET_INTRA_FLAG(map_scu[neb_addr]);
    if (neb_avaliable && cnt_hmvp)
    {
        c_motion.mv[0][0] = ctx->map.map_mv[neb_addr][0][0];
        c_motion.mv[0][1] = ctx->map.map_mv[neb_addr][0][1];
        c_motion.mv[1][0] = ctx->map.map_mv[neb_addr][1][0];
        c_motion.mv[1][1] = ctx->map.map_mv[neb_addr][1][1];

        c_motion.ref_idx[0] = ctx->map.map_refi[neb_addr][0];
        c_motion.ref_idx[1] = ctx->map.map_refi[neb_addr][1];
        return same_motion(hmvp_motion, c_motion);
    }
    return 0;
}
#endif

double analyze_inter_cu(ENC_CTX *ctx, ENC_CORE *core)
{
    ENC_PINTER *pi = &ctx->pinter;
    COM_MODE *cur_info = &core->mod_info_curr;
    COM_MODE *bst_info = &core->mod_info_best;
    int bit_depth = ctx->info.bit_depth_internal;
    int i, j;
    s16  coef_blk[N_C][MAX_CU_DIM];
    s16  resi[N_C][MAX_CU_DIM];
    double cost_best = MAX_COST;
    int cu_width_log2 = cur_info->cu_width_log2;
    int cu_height_log2 = cur_info->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;

#if TB_SPLIT_EXT
    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, cur_info->x_scu << 2, cur_info->y_scu << 2, cu_width, cu_height, core->mod_info_curr.pb_part, &core->mod_info_curr.pb_info);
    get_part_info(ctx->info.pic_width_in_scu, cur_info->x_scu << 2, cur_info->y_scu << 2, cu_width, cu_height, core->mod_info_curr.tb_part, &core->mod_info_curr.tb_info);
#endif

#if EXT_AMVR_HMVP
    int num_iter_mvp = 2;
    int num_hmvp_inter = MAX_NUM_MVR;
    if (ctx->info.sqh.emvr_enable_flag) // also imply (ctx->info.sqh.num_of_hmvp_cand && ctx->info.sqh.amvr_enable_flag) is true
    {
        if (core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].visit)
        {
            num_hmvp_inter = core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].mvr_hmvp_idx_history + 1;
            if (num_hmvp_inter > MAX_NUM_MVR)
            {
                num_hmvp_inter = MAX_NUM_MVR;
            }
        }
    }
#endif

    int num_amvr = MAX_NUM_MVR;
    if (ctx->info.sqh.amvr_enable_flag)
    {
        if (core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].visit)
        {
            num_amvr = core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].mvr_idx_history + 1;
            if (num_amvr > MAX_NUM_MVR)
            {
                num_amvr = MAX_NUM_MVR;
            }
        }
    }
    else
    {
        num_amvr = 1; /* only allow 1/4 pel of resolution */
    }
    pi->curr_mvr = 0;

    int allow_affine = ctx->info.sqh.affine_enable_flag;
    if (core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].visit)
    {
        if (core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].affine_flag_history == 0)
        {
            allow_affine = 0;
        }
    }
    // init translation mv for affine
    for (i = 0; i < REFP_NUM; i++)
    {
        for (j = 0; j < MAX_NUM_ACTIVE_REF_FRAME; j++)
        {
            pi->best_mv_uni[i][j][MV_X] = 0;
            pi->best_mv_uni[i][j][MV_Y] = 0;
        }
    }

#if INTER_CU_CONSTRAINT
    if (cu_width * cu_height >= 64)
    {
#endif
        analyze_direct_skip(ctx, core, &cost_best);
#if INTER_CU_CONSTRAINT
    }
#endif


#if EXT_AMVR_HMVP
    for (cur_info->mvp_from_hmvp_flag = 0; cur_info->mvp_from_hmvp_flag < num_iter_mvp; cur_info->mvp_from_hmvp_flag++)
    {
        if (cur_info->mvp_from_hmvp_flag)
        {
            num_amvr = 0;
            if (ctx->info.sqh.emvr_enable_flag) // also imply (ctx->info.sqh.num_of_hmvp_cand && ctx->info.sqh.amvr_enable_flag) is true
            {
                if ((bst_info->cu_mode == MODE_SKIP && core->skip_mvps_check == 0) || (bst_info->cu_mode != MODE_SKIP))
                {
                    num_amvr = min(num_hmvp_inter, core->cnt_hmvp_cands);
                    pi->max_imv[0] = 0;
                    pi->max_imv[1] = 0;
                }
            }
        }
#endif

        for (pi->curr_mvr = 0; pi->curr_mvr < num_amvr; pi->curr_mvr++)
        {
            double cost_L0L1[2] = { MAX_COST, MAX_COST };
            s8 refi_L0L1[2] = { REFI_INVALID, REFI_INVALID };
            s16 mv_L0L1[REFP_NUM][MV_D];
#if EXT_AMVR_HMVP
            if (cur_info->mvp_from_hmvp_flag)
            {
                if (same_mvp(core, ctx, core->motion_cands[core->cnt_hmvp_cands - 1 - pi->curr_mvr]))
                {
                    continue;
                }
                if (bst_info->cu_mode == MODE_SKIP && bst_info->skip_idx >= TRADITIONAL_SKIP_NUM && bst_info->skip_idx - TRADITIONAL_SKIP_NUM == pi->curr_mvr)
                {
                    continue;
                }
            }
#endif
#if TB_SPLIT_EXT
            init_pb_part(&core->mod_info_curr);
            init_tb_part(&core->mod_info_curr);
            get_part_info(ctx->info.pic_width_in_scu, cur_info->x_scu << 2, cur_info->y_scu << 2, cu_width, cu_height, core->mod_info_curr.pb_part, &core->mod_info_curr.pb_info);
            get_part_info(ctx->info.pic_width_in_scu, cur_info->x_scu << 2, cur_info->y_scu << 2, cu_width, cu_height, core->mod_info_curr.tb_part, &core->mod_info_curr.tb_info);
#endif
            analyze_uni_pred(ctx, core, cost_L0L1, mv_L0L1, refi_L0L1, &cost_best);
#if INTER_CU_CONSTRAINT
            if (pi->slice_type == SLICE_B && cu_width * cu_height >= 64)
#else
            if (pi->slice_type == SLICE_B) // bi-prediction
#endif
            {
                analyze_bi(ctx, core, mv_L0L1, refi_L0L1, cost_L0L1, &cost_best);
#if SMVD
                if (ctx->info.sqh.smvd_enable_flag && ctx->ptr - ctx->refp[0][REFP_0].ptr == ctx->refp[0][REFP_1].ptr - ctx->ptr
#if SKIP_SMVD
                        && !cur_info->mvp_from_hmvp_flag
#endif
                   )
                {
                    analyze_smvd(ctx, core, &cost_best);
                }
#endif
            }

#if EXT_AMVR_HMVP
            if (cur_info->mvp_from_hmvp_flag)
            {
                if (pi->curr_mvr >= SKIP_MVR_IDX + 1 && (core->mod_info_best.cu_mode == MODE_SKIP))
                {
                    break;
                }
            }
            else
#endif
                if (pi->curr_mvr >= SKIP_MVR_IDX && ((core->mod_info_best.cu_mode == MODE_SKIP || core->mod_info_best.cu_mode == MODE_DIR)))
                {
                    break;
                }

            if (pi->curr_mvr >= FAST_MVR_IDX)
            {
                if (abs(bst_info->mvd[REFP_0][MV_X]) <= 0 &&
                        abs(bst_info->mvd[REFP_0][MV_Y]) <= 0 &&
                        abs(bst_info->mvd[REFP_1][MV_X]) <= 0 &&
                        abs(bst_info->mvd[REFP_1][MV_Y]) <= 0)
                {
                    break;
                }
            }
            update_best_int_mv(bst_info, pi);
        }
#if EXT_AMVR_HMVP
    }
#endif

    double cost_L0L1[2] = { MAX_COST, MAX_COST };
    s8 refi_L0L1[2] = { REFI_INVALID, REFI_INVALID };
    CPMV aff_mv_L0L1[REFP_NUM][VER_NUM][MV_D];

    if (allow_affine && cu_width >= AFF_SIZE && cu_height >= AFF_SIZE)
    {
        analyze_affine_merge(ctx, core, &cost_best);
    }

    if (allow_affine && cu_width >= AFF_SIZE && cu_height >= AFF_SIZE)
    {
        if (!(core->mod_info_best.cu_mode == MODE_SKIP && !core->mod_info_best.affine_flag)) //fast skip affine
        {
#if BD_AFFINE_AMVR
            int num_affine_amvr = ctx->info.sqh.amvr_enable_flag ? MAX_NUM_AFFINE_MVR : 1;
            for (pi->curr_mvr = 0; pi->curr_mvr < num_affine_amvr; pi->curr_mvr++)
            {
#endif
                analyze_affine_uni(ctx, core, aff_mv_L0L1, refi_L0L1, cost_L0L1, &cost_best);
                if (pi->slice_type == SLICE_B)
                {
                    analyze_affine_bi(ctx, core, aff_mv_L0L1, refi_L0L1, cost_L0L1, &cost_best);
                }
#if BD_AFFINE_AMVR
            }
#endif
        }
    }

    /* reconstruct */
    int start_comp = (ctx->tree_status == TREE_L || ctx->tree_status == TREE_LC) ? Y_C : U_C;
    int num_comp = ctx->tree_status == TREE_LC ? 3 : (ctx->tree_status == TREE_L ? 1 : 2);
    for (j = start_comp; j < start_comp + num_comp; j++)
    {
        int size_tmp = (cu_width * cu_height) >> (j == 0 ? 0 : 2);
        com_mcpy(coef_blk[j], bst_info->coef[j], sizeof(s16) * size_tmp);
    }
    int use_secTrans[MAX_NUM_TB] = { 0, 0, 0, 0 };
    com_itdq_yuv(&core->mod_info_best, coef_blk, resi, ctx->wq, cu_width_log2, cu_height_log2, pi->qp_y, pi->qp_u, pi->qp_v, bit_depth, use_secTrans, 0);
    for (i = start_comp; i < start_comp + num_comp; i++)
    {
        int stride = (i == 0 ? cu_width : cu_width >> 1);
        com_recon(i == Y_C ? core->mod_info_best.tb_part : SIZE_2Nx2N, resi[i], bst_info->pred[i], core->mod_info_best.num_nz, i, stride, (i == 0 ? cu_height : cu_height >> 1), stride, bst_info->rec[i], bit_depth);
    }

    if (!core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].visit)
    {
        core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].mvr_idx_history = bst_info->mvr_idx;
    }

#if EXT_AMVR_HMVP
    if (!core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].visit && bst_info->mvp_from_hmvp_flag)
    {
        core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].mvr_hmvp_idx_history = bst_info->mvr_idx;
    }
#endif

    if (!core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].visit)
    {
        core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup].affine_flag_history = bst_info->affine_flag;
    }

    return cost_best;
}

int pinter_set_complexity(ENC_CTX *ctx, int complexity)
{
    ENC_PINTER *pi;
    pi = &ctx->pinter;
    /* default values *************************************************/
    pi->max_search_range = ctx->param.max_b_frames == 0 ? SEARCH_RANGE_IPEL_LD : SEARCH_RANGE_IPEL_RA;
    pi->search_range_ipel[MV_X] = (s16)pi->max_search_range;
    pi->search_range_ipel[MV_Y] = (s16)pi->max_search_range;
    pi->search_range_spel[MV_X] = SEARCH_RANGE_SPEL;
    pi->search_range_spel[MV_Y] = SEARCH_RANGE_SPEL;
    pi->search_pattern_hpel = tbl_search_pattern_hpel_partial;
    pi->search_pattern_hpel_cnt = 8;
    pi->search_pattern_qpel = tbl_search_pattern_qpel_8point;
    pi->search_pattern_qpel_cnt = 8;
    pi->me_level = ME_LEV_QPEL;
    pi->fn_me = pinter_me_epzs;
    pi->fn_affine_me = pinter_affine_me_gradient;
    pi->complexity = complexity;
    return COM_OK;
}
