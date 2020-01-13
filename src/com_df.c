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

#include "com_def.h"
#include "com_df.h"
#include "com_tbl.h"

const u8 sm_tc_table[54] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,5,5,6,6,7,8,9,10,11,13,14,16,18,20,22,24
};

const u8 sm_beta_table[52] =
{
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6,7,8,9,10,11,12,13,14,15,16,17,18,20,22,24,26,28,30,32,34,36,38,40,42,44,46,48,50,52,54,56,58,60,62,64
};


typedef unsigned short byte;
#if DEBLOCK_M4647
byte ALPHA_TABLE[64] =
{
    0,  0,  0,  0,  0,  0,  0,  0,
    1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  2,  2,  2,
    2,  2,  2,  3,  3,  3,  3,  4,
    4,  4,  5,  5,  6,  6,  7,  7,
    8,  9,  10, 10, 11, 12, 13, 15,
    16, 17, 19, 21, 23, 25, 27, 29,
    32, 35, 38, 41, 45, 49, 54, 59
};
byte  BETA_TABLE[64] =
{
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  2,  2,
    2,  2,  2,  2,  3,  3,  3,  3,
    4,  4,  5,  5,  5,  6,  6,  7,
    8,  8,  9,  10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 23, 24, 24, 25, 25, 26, 27
};
#else
byte ALPHA_TABLE[64] =
{
    0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 2, 2, 2, 3, 3,
    4, 4, 5, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 15, 16, 18, 20,
    22, 24, 26, 28, 30, 33, 33, 35,
    35, 36, 37, 37, 39, 39, 42, 44,
    46, 48, 50, 52, 53, 54, 55, 56,
    57, 58, 59, 60, 61, 62, 63, 64
};
byte  BETA_TABLE[64] =
{
    0, 0, 0, 0, 0, 0, 1, 1,
    1, 1, 1, 1, 1, 2, 2, 2,
    2, 2, 3, 3, 3, 3, 4, 4,
    4, 4, 5, 5, 5, 5, 6, 6,
    6, 7, 7, 7, 8, 8, 8, 9,
    9, 10, 10, 11, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22,
    23, 23, 24, 24, 25, 25, 26, 27
};

#endif
byte CLIP_TAB_AVS[64] =
{
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 2,
    2, 2, 2, 2, 2, 2, 3, 3,
    3, 3, 3, 3, 3, 4, 4, 4,
    5, 5, 5, 6, 6, 6, 7, 7,
    7, 7, 8, 8, 8, 9, 9, 9
};

void CreateEdgeFilter_avs2(int w, int h, int ***ppbEdgeFilter)
{
    //allocate memory
    for (int i = 0; i < LOOPFILTER_DIR_TYPE; i++)
    {
        ppbEdgeFilter[i] = (int **)calloc((h >> LOOPFILTER_SIZE_IN_BIT), sizeof(int *));
        for (int j = 0; j < (h >> LOOPFILTER_SIZE_IN_BIT); j++)
        {
            ppbEdgeFilter[i][j] = (int *)calloc((w >> LOOPFILTER_SIZE_IN_BIT), sizeof(int));
        }
    }
    //reset value
    for (int i = 0; i < LOOPFILTER_DIR_TYPE; i++)
    {
        for (int b4_y = 0; b4_y < (h >> LOOPFILTER_SIZE_IN_BIT); b4_y++)
        {
            for (int b4_x = 0; b4_x < (w >> LOOPFILTER_SIZE_IN_BIT); b4_x++)
            {
                ppbEdgeFilter[i][b4_y][b4_x] = 0;
            }
        }
    }
}

void ClearEdgeFilter_avs2(int x, int y, int w, int h, int ***ppbEdgeFilter)
{
    for (int i = 0; i < LOOPFILTER_DIR_TYPE; i++)
    {
        for (int b4_y = (y >> LOOPFILTER_SIZE_IN_BIT); b4_y < ((y + h) >> LOOPFILTER_SIZE_IN_BIT); b4_y++)
        {
            for (int b4_x = (x >> LOOPFILTER_SIZE_IN_BIT); b4_x < ((x + w) >> LOOPFILTER_SIZE_IN_BIT); b4_x++)
            {
                ppbEdgeFilter[i][b4_y][b4_x] = 0;
            }
        }
    }
}

void DeleteEdgeFilter_avs2(int ***ppbEdgeFilter, int h)
{
    for (int i = 0; i < LOOPFILTER_DIR_TYPE; i++)
    {
        if( ppbEdgeFilter[i] ) {
        for (int j = 0; j < (h >> LOOPFILTER_SIZE_IN_BIT); j++)
        {
            if( ppbEdgeFilter[i][j] )
            {
                com_mfree_fast( ppbEdgeFilter[i][j] );
                ppbEdgeFilter[i][j] = NULL;
            }
        }

        com_mfree_fast(ppbEdgeFilter[i]);
        ppbEdgeFilter[i] = NULL;
        }
    }
}

void xSetEdgeFilterParam_hor_avs2(COM_PIC * pic, int ***ppbEdgeFilter, int x_pel, int y_pel, int cuw, int cuh, int edgecondition)
{
    int w = cuw >> LOOPFILTER_SIZE_IN_BIT;
    int b4_x_start = x_pel >> LOOPFILTER_SIZE_IN_BIT;
    int b4_y_start = y_pel >> LOOPFILTER_SIZE_IN_BIT;
    int idir = 1;
    if (y_pel > 0 && (y_pel % LOOPFILTER_GRID == 0))
    {
        if (b4_y_start == 0)
        {
            return;
        }
        for (int i = 0; i < w; i++)
        {
            if ((int)(b4_x_start + i) < ((pic->width_luma) >> LOOPFILTER_SIZE_IN_BIT)&& (int)(b4_y_start) < ((pic->height_luma) >> LOOPFILTER_SIZE_IN_BIT))
            {
                if (ppbEdgeFilter[idir][b4_y_start][b4_x_start + i])
                {
                    break;
                }
                ppbEdgeFilter[idir][b4_y_start][b4_x_start + i] = edgecondition;
            }
            else
            {
                break;
            }
        }
    }
}

void xSetEdgeFilterParam_ver_avs2(COM_PIC * pic, int ***ppbEdgeFilter, int x_pel, int y_pel, int cuw, int cuh, int edgecondition)
{
    int h = cuh >> LOOPFILTER_SIZE_IN_BIT;
    int b4_x_start = x_pel >> LOOPFILTER_SIZE_IN_BIT;
    int b4_y_start = y_pel >> LOOPFILTER_SIZE_IN_BIT;
    int idir = 0;
    /* vertical filtering */
    if (x_pel > 0 && (x_pel % LOOPFILTER_GRID == 0))
    {
        if (b4_x_start == 0)
        {
            return;
        }
        for (int i = 0; i < h; i++)
        {
            if ((int)(b4_y_start + i) < (pic->height_luma >> LOOPFILTER_SIZE_IN_BIT))
            {
                if (ppbEdgeFilter[idir][b4_y_start + i][b4_x_start])
                {
                    break;
                }
                ppbEdgeFilter[idir][b4_y_start + i][b4_x_start] = edgecondition;
            }
            else
            {
                break;
            }
        }
    }
}

void xSetEdgeFilter_avs2(COM_INFO *info, COM_MAP *map, COM_PIC * pic, int*** ppbEdgeFilter)
{
    int i, j;
    BOOL b_recurse = 1;
    int lcu_num = 0;
    /* all lcu */
    for (j = 0; j < info->pic_height_in_lcu; j++)
    {
        for (i = 0; i < info->pic_width_in_lcu; i++)
        {
            xSetEdgeFilter_One_SCU_avs2(info, map, pic, ppbEdgeFilter, (i << info->log2_max_cuwh), (j << info->log2_max_cuwh), info->max_cuwh, info->max_cuwh, 0, 0, b_recurse);
            lcu_num++;
        }
    }
}

void xSetEdgeFilter_One_SCU_avs2(COM_INFO *info, COM_MAP *map, COM_PIC * pic, int ***ppbEdgeFilter, int x, int y, int cuw, int cuh, int cud, int cup, BOOL b_recurse)
{
    s8 split_mode = NO_SPLIT;
    int lcu_idx;
    int edge_type = 0;

    lcu_idx = (x >> info->log2_max_cuwh) + (y >> info->log2_max_cuwh) * info->pic_width_in_lcu;
    if (b_recurse)
    {
        com_get_split_mode(&split_mode, cud, cup, cuw, cuh, info->max_cuwh, (map->map_split[lcu_idx]));
    }
    if (b_recurse && split_mode != NO_SPLIT)
    {
        COM_SPLIT_STRUCT split_struct;
        com_split_get_part_structure(split_mode, x, y, cuw, cuh, cup, cud, info->log2_max_cuwh - MIN_CU_LOG2, &split_struct);
        for (int part_num = 0; part_num < split_struct.part_count; ++part_num)
        {
            int cur_part_num = part_num;
            int sub_cuw = split_struct.width[cur_part_num];
            int sub_cuh = split_struct.height[cur_part_num];
            int x_pos = split_struct.x_pos[cur_part_num];
            int y_pos = split_struct.y_pos[cur_part_num];
            if (x_pos < info->pic_width && y_pos < info->pic_height)
            {
                xSetEdgeFilter_One_SCU_avs2(info, map, pic, ppbEdgeFilter, x_pos, y_pos, sub_cuw, sub_cuh, split_struct.cud, split_struct.cup[cur_part_num], b_recurse);
            }
        }
    }
    else
    {
        // luma tb filtering due to intra_DT and PBT
        int scu_idx = PEL2SCU(y) * info->pic_width_in_scu + PEL2SCU(x);
        int pb_tb_part = map->map_pb_tb_part[scu_idx];
        int tb_part = MCU_GET_TB_PART_LUMA(pb_tb_part);

        switch (tb_part)
        {
        case SIZE_2NxhN:
            xSetEdgeFilterParam_hor_avs2(pic, ppbEdgeFilter, x, y + cuh / 4 * 1, cuw, cuh, EDGE_TYPE_LUMA);
            xSetEdgeFilterParam_hor_avs2(pic, ppbEdgeFilter, x, y + cuh / 4 * 2, cuw, cuh, EDGE_TYPE_LUMA);
            xSetEdgeFilterParam_hor_avs2(pic, ppbEdgeFilter, x, y + cuh / 4 * 3, cuw, cuh, EDGE_TYPE_LUMA);
            break;
        case SIZE_hNx2N:
            xSetEdgeFilterParam_ver_avs2(pic, ppbEdgeFilter, x + cuw / 4 * 1, y, cuw, cuh, EDGE_TYPE_LUMA);
            xSetEdgeFilterParam_ver_avs2(pic, ppbEdgeFilter, x + cuw / 4 * 2, y, cuw, cuh, EDGE_TYPE_LUMA);
            xSetEdgeFilterParam_ver_avs2(pic, ppbEdgeFilter, x + cuw / 4 * 3, y, cuw, cuh, EDGE_TYPE_LUMA);
            break;
        case SIZE_NxN:
            xSetEdgeFilterParam_hor_avs2(pic, ppbEdgeFilter, x, y + cuh / 4 * 2, cuw, cuh, EDGE_TYPE_LUMA);
            xSetEdgeFilterParam_ver_avs2(pic, ppbEdgeFilter, x + cuw / 4 * 2, y, cuw, cuh, EDGE_TYPE_LUMA);
            break;
        default:
            break;
        }

        xSetEdgeFilterParam_hor_avs2(pic, ppbEdgeFilter, x, y, cuw, cuh, EDGE_TYPE_ALL);  ///UP
        xSetEdgeFilterParam_ver_avs2(pic, ppbEdgeFilter, x, y, cuw, cuh, EDGE_TYPE_ALL);  /// LEFT
    }
}


int SkipFiltering(COM_INFO *info, COM_MAP *map, COM_REFP refp[MAX_NUM_REF_PICS][REFP_NUM], u32 *MbP, u32 *MbQ, int dir, int block_y, int block_x)
{
#if DEBLOCK_M4647
    int t = (block_x)+(block_y)* info->pic_width_in_scu;
    int reflist;
    s8 *refiP,*refiQ;
    s16 (*mvP)[MV_D],(*mvQ)[MV_D];

    refiP = *(map->map_refi + t);
    mvP   = *(map->map_mv   + t);
    refiQ = *(map->map_refi + t + (dir ? (-info->pic_width_in_scu) : (-1)));
    mvQ   = *(map->map_mv   + t + (dir ? (-info->pic_width_in_scu) : (-1)));

    COM_PIC *p_pic0 = REFI_IS_VALID(refiP[REFP_0]) ? refp[refiP[REFP_0]][REFP_0].pic : NULL;
    COM_PIC *p_pic1 = REFI_IS_VALID(refiP[REFP_1]) ? refp[refiP[REFP_1]][REFP_1].pic : NULL;
    COM_PIC *q_pic0 = REFI_IS_VALID(refiQ[REFP_0]) ? refp[refiQ[REFP_0]][REFP_0].pic : NULL;
    COM_PIC *q_pic1 = REFI_IS_VALID(refiQ[REFP_1]) ? refp[refiQ[REFP_1]][REFP_1].pic : NULL;

    if ( (p_pic0 == NULL && p_pic1 == NULL) || (q_pic0 == NULL && q_pic1 == NULL) )
    {
        return 0;
    }
    if (MCU_GET_CBF(*MbP) || MCU_GET_CBF(*MbQ))
    {
        return 0;
    }

    if (p_pic0 == q_pic0 && p_pic1 == q_pic1)
    {
        for (reflist = REFP_0; reflist < REFP_NUM; reflist++)
        {
            if (REFI_IS_VALID(refiP[reflist]))
            {
                if ((abs(mvP[reflist][MV_X] - mvQ[reflist][MV_X]) >= 4) ||
                        (abs(mvP[reflist][MV_Y] - mvQ[reflist][MV_Y]) >= 4))
                {
                    return 0;
                }
            }
        }
        return 1;
    }
    if (p_pic0 == q_pic1 && p_pic1 == q_pic0)
    {
        for (reflist = REFP_0; reflist < REFP_NUM; reflist++)
        {
            if (REFI_IS_VALID(refiP[reflist]))
            {
                if ((abs(mvP[reflist][MV_X] - mvQ[!reflist][MV_X]) >= 4) ||
                        (abs(mvP[reflist][MV_Y] - mvQ[!reflist][MV_Y]) >= 4))
                {
                    return 0;
                }
            }
        }
        return 1;
    }
    return 0;
#else
    int    iFlagSkipFiltering;
    int t = (block_x)+(block_y)* info->pic_width_in_scu;
    int reflist;
    s8 *curr_refi;
    s8 *neighbor_refi;
    s16(*curr_mv)[MV_D];
    s16(*neighbor_mv)[MV_D];
    curr_refi = *(map->map_refi + t);
    curr_mv = *(map->map_mv + t);
    neighbor_refi = *(map->map_refi + t + (dir ? (-info->pic_width_in_scu) : (-1)));
    neighbor_mv = *(map->map_mv + t + (dir ? (-info->pic_width_in_scu) : (-1)));
    s16 mv0[2] = { 0, 0 };
    s16 mv1[2] = { 0, 0 };
    s8 ref0 = -1;
    s8 ref1 = -1;
    BOOL b_forward = (REFI_IS_VALID(curr_refi[REFP_0]) & REFI_IS_VALID(neighbor_refi[REFP_0]));
    BOOL b_backward = (REFI_IS_VALID(curr_refi[REFP_1]) & REFI_IS_VALID(neighbor_refi[REFP_1]));

    if ((b_forward || b_backward))
    {
        if (b_forward) //use forward
        {
            reflist = REFP_0;
        }
        else //if (b_backward) // use backward
        {
            reflist = REFP_1;
        }
        mv0[MV_X] = curr_mv[reflist][MV_X];
        mv0[MV_Y] = curr_mv[reflist][MV_Y];
        ref0 = curr_refi[reflist];
        mv1[MV_X] = neighbor_mv[reflist][MV_X];
        mv1[MV_Y] = neighbor_mv[reflist][MV_Y];
        ref1 = neighbor_refi[reflist];
        switch (info->pic_header.slice_type)
        {
        case COM_ST_B:
            if ((MCU_GET_CBF(*MbP) == 0) && (MCU_GET_CBF(*MbQ) == 0) &&
                    (abs(mv0[MV_X] - mv1[MV_X]) < 4) &&
                    (abs(mv0[MV_Y] - mv1[MV_Y]) < 4) &&
                    (ref0 == ref1) && REFI_IS_VALID(ref0))
            {
                iFlagSkipFiltering = 1;
            }
            else
            {
                iFlagSkipFiltering = 0;
            }
            break;
        default:
            iFlagSkipFiltering = 0;
        }
    }
    else
    {
        iFlagSkipFiltering = 0;
    }
    return iFlagSkipFiltering;
#endif
}

void EdgeLoopX(COM_INFO *info, COM_MAP *map, COM_REFP refp[MAX_NUM_REF_PICS][REFP_NUM], pel *SrcPtr, int QP, int dir, int width, int Chro, u32 *MbP, u32 *MbQ, int block_y, int block_x)
{
    int     temp_pel, PtrInc;
    int     inc, inc2, inc3, inc4;
    int     AbsDelta;
    int     L3, L2, L1, L0, R0, R1, R2, R3;
    int     fs; //fs stands for filtering strength.  The larger fs is, the stronger filter is applied.
    int     Alpha, Beta, Beta1;
    int     FlatnessL, FlatnessR;
    int     shift1 = info->bit_depth_internal - 8;
    int     skipFilteringFlag = 0;
    // FlatnessL and FlatnessR describe how flat the curve is of one codingUnit.
    int alpha_c_offset = info->pic_header.alpha_c_offset;
    int beta_offset = info->pic_header.beta_offset;
    PtrInc = dir ? 1 : width;
    inc    = dir ? width : 1;
    inc2 = inc << 1;
    inc3 = inc + inc2;
    inc4 = inc + inc3;

    Alpha = ALPHA_TABLE[COM_CLIP3(MIN_QUANT, MAX_QUANT_BASE, QP + alpha_c_offset)] << shift1;
    Beta  = BETA_TABLE [COM_CLIP3(MIN_QUANT, MAX_QUANT_BASE, QP + beta_offset   )] << shift1;
    Beta1 = 1 << shift1;

    for (temp_pel = 0; temp_pel < LOOPFILTER_SIZE; temp_pel++)
    {
        if (temp_pel % 4 == 0)
        {
            skipFilteringFlag = SkipFiltering(info, map, refp, MbP, MbQ, dir, block_y + !dir * (temp_pel >> 2), block_x + dir * (temp_pel >> 2));
        }

        L3 = SrcPtr[-inc4];
        L2 = SrcPtr[-inc3];
        L1 = SrcPtr[-inc2];
        L0 = SrcPtr[-inc];
        R0 = SrcPtr[0];
        R1 = SrcPtr[inc];
        R2 = SrcPtr[inc2];
        R3 = SrcPtr[inc3];

        AbsDelta = COM_ABS(R0 - L0);
#if DEBLOCK_M4647
        if (!skipFilteringFlag)
        {
#else
        if (!skipFilteringFlag && (AbsDelta < Alpha) && (AbsDelta > 1))
        {
#endif
            FlatnessL = (COM_ABS(L1 - L0) < Beta) ? 2 : 0;
            if (COM_ABS(L2 - L0) < Beta)
            {
                FlatnessL++;
            }
            FlatnessR = (COM_ABS(R0 - R1) < Beta) ? 2 : 0;
            if (COM_ABS(R0 - R2) < Beta)
            {
                FlatnessR++;
            }

            switch (FlatnessL + FlatnessR)
            {
            case 6:
#if DEBLOCK_M4647
                fs = (COM_ABS(R0 - R1) <= Beta / 4 && COM_ABS(L0 - L1) <= Beta / 4 && AbsDelta < Alpha) ? 4 : 3;
#else
                fs = (R1 == R0 && L0 == L1) ? 4 : 3;
#endif
                break;
            case 5:
                fs = (R1 == R0 && L0 == L1) ? 3 : 2;
                break;
            case 4:
                fs = (FlatnessL == 2) ? 2 : 1;
                break;
            case 3:
                fs = (COM_ABS(L1 - R1) < Beta) ? 1 : 0;
                break;
            default:
                fs = 0;
                break;
            }

            if (Chro)
            {
                if (fs > 0)
                {
                    fs--;
                }
#if DEBLOCK_M4647
                switch (fs)
                {
                case 3:
                    SrcPtr[-inc2] = (pel)((L2 * 3 + L1 * 8 + L0 * 3 + R0 * 2 + 8) >> 4);                           //L2
                    SrcPtr[ inc ] = (pel)((R2 * 3 + R1 * 8 + R0 * 3 + L0 * 2 + 8) >> 4);                           //R2
                case 2:
                case 1:
                    SrcPtr[-inc] = (pel)((L1 * 3 + L0 * 10 + R0 * 3 + 8) >> 4);
                    SrcPtr[   0] = (pel)((R1 * 3 + R0 * 10 + L0 * 3 + 8) >> 4);
                    break;

                default:
                    break;
                }
#else
                switch (fs)
                {
                case 3:
                    SrcPtr[-inc] = (pel)((L2 + (L1 << 2) + (L0 << 2) + (L0 << 1) + (R0 << 2) + R1 + 8) >> 4);           //L0
                    SrcPtr[0] = (pel)((L1 + (L0 << 2) + (R0 << 2) + (R0 << 1) + (R1 << 2) + R2 + 8) >> 4);           //R0
                    SrcPtr[-inc2] = (pel)((L2 * 3 + L1 * 8 + L0 * 4 + R0 + 8) >> 4);
                    SrcPtr[inc] = (pel)((R2 * 3 + R1 * 8 + R0 * 4 + L0 + 8) >> 4);
                    break;
                case 2:
                    SrcPtr[-inc] = (pel)(((L1 << 1) + L1 + (L0 << 3) + (L0 << 1) + (R0 << 1) + R0 + 8) >> 4);
                    SrcPtr[0] = (pel)(((L0 << 1) + L0 + (R0 << 3) + (R0 << 1) + (R1 << 1) + R1 + 8) >> 4);
                    break;
                case 1:
                    SrcPtr[-inc] = (pel)((L0 * 3 + R0 + 2) >> 2);
                    SrcPtr[0] = (pel)((R0 * 3 + L0 + 2) >> 2);
                    break;
                default:
                    break;
                }
#endif
            }
            else
            {
                switch (fs)
                {
                case 4:
#if DEBLOCK_M4647
                    SrcPtr[-inc] = (pel)((L2 * 3 + L1 * 8 + L0 * 10 + R0 * 8 + R1 * 3 + 16) >> 5);             //L0
                    SrcPtr[0] = (pel)((R2 * 3 + R1 * 8 + R0 * 10 + L0 * 8 + L1 * 3 + 16) >> 5);             //R0
                    SrcPtr[-inc2] = (pel)((L2 * 4 + L1 * 5 + L0 * 4 + R0 * 3 + 8) >> 4);                       //L1
                    SrcPtr[inc] = (pel)((R2 * 4 + R1 * 5 + R0 * 4 + L0 * 3 + 8) >> 4);                       //R1
                    SrcPtr[-inc3] = (pel)((L3 * 2 + L2 * 2 + L1 * 2 + L0 * 1 + R0 + 4) >> 3);                          //L2
                    SrcPtr[inc2] = (pel)((R3 * 2 + R2 * 2 + R1 * 2 + R0 * 1 + L0 + 4) >> 3);                          //R2
#else
                    SrcPtr[-inc] = (pel)((L0 + ((L0 + L2) << 3) + L2 + (R0 << 3) + (R2 << 2) + (R2 << 1) + 16) >> 5);             //L0
                    SrcPtr[-inc2] = (pel)(((L0 << 3) - L0 + (L2 << 2) + (L2 << 1) + R0 + (R0 << 1) + 8) >> 4);           //L1
                    SrcPtr[-inc3] = (pel)(((L0 << 2) + L2 + (L2 << 1) + R0 + 4) >> 3);       //L2
                    SrcPtr[0] = (pel)((R0 + ((R0 + R2) << 3) + R2 + (L0 << 3) + (L2 << 2) + (L2 << 1) + 16) >> 5);             //R0
                    SrcPtr[inc] = (pel)(((R0 << 3) - R0 + (R2 << 2) + (R2 << 1) + L0 + (L0 << 1) + 8) >> 4);           //R1
                    SrcPtr[inc2] = (pel)(((R0 << 2) + R2 + (R2 << 1) + L0 + 4) >> 3);       //R2

#endif
                    break;
                case 3:
                    SrcPtr[-inc] = (pel)((L2 + (L1 << 2) + (L0 << 2) + (L0 << 1) + (R0 << 2) + R1 + 8) >> 4); //L0
                    SrcPtr[0] = (pel)((L1 + (L0 << 2) + (R0 << 2) + (R0 << 1) + (R1 << 2) + R2 + 8) >> 4); //R0
                    SrcPtr[-inc2] = (pel)((L2 * 3 + L1 * 8 + L0 * 4 + R0 + 8) >> 4);                           //L2
                    SrcPtr[inc] = (pel)((R2 * 3 + R1 * 8 + R0 * 4 + L0 + 8) >> 4);                           //R2
                    break;
                case 2:
                    SrcPtr[-inc] = (pel)(((L1 << 1) + L1 + (L0 << 3) + (L0 << 1) + (R0 << 1) + R0 + 8) >> 4);
                    SrcPtr[0] = (pel)(((L0 << 1) + L0 + (R0 << 3) + (R0 << 1) + (R1 << 1) + R1 + 8) >> 4);
                    break;
                case 1:
                    SrcPtr[-inc] = (pel)((L0 * 3 + R0 + 2) >> 2);
                    SrcPtr[0] = (pel)((R0 * 3 + L0 + 2) >> 2);
                    break;
                default:
                    break;
                }
            }
            SrcPtr += PtrInc;    // Next row or column
            temp_pel += Chro;
        }
        else
        {
            SrcPtr += PtrInc;
            temp_pel += Chro;
        }
    }
}

void DeblockMb_avs2(COM_INFO *info, COM_MAP *map, COM_PIC *pic, COM_REFP refp[MAX_NUM_REF_PICS][REFP_NUM], int*** ppbEdgeFilter, int mb_y, int mb_x, int EdgeDir)
{
    pel         * SrcY, *SrcU, *SrcV;
    int           EdgeCondition;
    int           dir, QP;
    unsigned int  b4_x_start;
    unsigned int  b4_y_start;
    int           iFlagSkipFiltering;
    int x_pel = mb_x << LOOPFILTER_SIZE_IN_BIT;
    int y_pel = mb_y << LOOPFILTER_SIZE_IN_BIT;
    int s_l = pic->stride_luma;
    int s_c = pic->stride_chroma;
    int t;
    t = x_pel + y_pel * s_l;
    SrcY = pic->y + t;
    t = (x_pel >> 1) + (y_pel >> 1) * s_c;
    SrcU = pic->u + t;
    SrcV = pic->v + t;
    u32    *MbP, *MbQ;
    t = mb_x + mb_y * info->pic_width_in_scu;
    MbQ = map->map_scu + t; // current Mb
    dir = EdgeDir;
    {
        EdgeCondition = (dir && mb_y) || (!dir && mb_x);     // can not filter beyond frame boundaries
        if (!dir && mb_x && !info->sqh.cross_patch_loop_filter)
        {
            EdgeCondition = (map->map_patch_idx[t] == map->map_patch_idx[t - 1]) ? EdgeCondition : 0;
            //  can not filter beyond slice boundaries
        }
        if (dir && mb_y && !info->sqh.cross_patch_loop_filter)
        {
            EdgeCondition = (map->map_patch_idx[t] == map->map_patch_idx[t - info->pic_width_in_scu]) ? EdgeCondition : 0;
            //  can not filter beyond slice boundaries
        }
        b4_x_start = mb_x;
        b4_y_start = mb_y;
        EdgeCondition = (ppbEdgeFilter[dir][b4_y_start][b4_x_start] &&
                         EdgeCondition) ? ppbEdgeFilter[dir][b4_y_start][b4_x_start] : 0;
        // then  4 horicontal
        if (EdgeCondition)
        {
            MbP = (dir) ? (MbQ - info->pic_width_in_scu) : (MbQ - 1);         // MbP = Mb of the remote 4x4 block
            int MbQ_qp = MCU_GET_QP(*MbQ);
            int MbP_qp = MCU_GET_QP(*MbP);
            QP = (MbP_qp + MbQ_qp + 1) >> 1;
            // Average QP of the two blocks
            iFlagSkipFiltering = 0;
            if (!iFlagSkipFiltering)
            {
                EdgeLoopX(info, map, refp, SrcY, QP - info->qp_offset_bit_depth, dir, s_l, 0, MbP, MbQ, mb_y << (LOOPFILTER_SIZE_IN_BIT - 2), mb_x << (LOOPFILTER_SIZE_IN_BIT - 2));
                if ((EdgeCondition == EDGE_TYPE_ALL))
                {
                    if (((mb_y << MIN_CU_LOG2) % (LOOPFILTER_GRID << 1) == 0 && dir) || (((mb_x << MIN_CU_LOG2) % (LOOPFILTER_GRID << 1) == 0) && (!dir)))
                    {
                        int cQPuv;
                        int c_p_QPuv = MbP_qp + info->pic_header.chroma_quant_param_delta_cb - info->qp_offset_bit_depth;
                        int c_q_QPuv = MbQ_qp + info->pic_header.chroma_quant_param_delta_cb - info->qp_offset_bit_depth;
                        c_p_QPuv = COM_CLIP( c_p_QPuv, MIN_QUANT - 16, MAX_QUANT_BASE );
                        c_q_QPuv = COM_CLIP( c_q_QPuv, MIN_QUANT - 16, MAX_QUANT_BASE );
                        if (c_p_QPuv >= 0)
                        {
                            c_p_QPuv = com_tbl_qp_chroma_adjust[COM_MIN(MAX_QUANT_BASE, c_p_QPuv)];
                        }
                        if (c_q_QPuv >= 0)
                        {
                            c_q_QPuv = com_tbl_qp_chroma_adjust[COM_MIN(MAX_QUANT_BASE, c_q_QPuv)];
                        }
                        c_p_QPuv = COM_CLIP( c_p_QPuv + info->qp_offset_bit_depth, MIN_QUANT, MAX_QUANT_BASE + info->qp_offset_bit_depth );
                        c_q_QPuv = COM_CLIP( c_q_QPuv + info->qp_offset_bit_depth, MIN_QUANT, MAX_QUANT_BASE + info->qp_offset_bit_depth );

                        cQPuv = (c_p_QPuv + c_q_QPuv + 1) >> 1;
                        EdgeLoopX(info, map, refp, SrcU, cQPuv - info->qp_offset_bit_depth, dir, s_c, 1, MbP, MbQ, mb_y << (LOOPFILTER_SIZE_IN_BIT - 2), mb_x << (LOOPFILTER_SIZE_IN_BIT - 2));

                        c_p_QPuv = MbP_qp + info->pic_header.chroma_quant_param_delta_cr - info->qp_offset_bit_depth;
                        c_q_QPuv = MbQ_qp + info->pic_header.chroma_quant_param_delta_cr - info->qp_offset_bit_depth;
                        c_p_QPuv = COM_CLIP( c_p_QPuv, MIN_QUANT - 16, MAX_QUANT_BASE );
                        c_q_QPuv = COM_CLIP( c_q_QPuv, MIN_QUANT - 16, MAX_QUANT_BASE );

                        if (c_p_QPuv >= 0)
                        {
                            c_p_QPuv = com_tbl_qp_chroma_adjust[COM_MIN(MAX_QUANT_BASE, c_p_QPuv)];
                        }
                        if (c_q_QPuv >= 0)
                        {
                            c_q_QPuv = com_tbl_qp_chroma_adjust[COM_MIN(MAX_QUANT_BASE, c_q_QPuv)];
                        }
                        c_p_QPuv = COM_CLIP( c_p_QPuv + info->qp_offset_bit_depth, MIN_QUANT, MAX_QUANT_BASE + info->qp_offset_bit_depth );
                        c_q_QPuv = COM_CLIP( c_q_QPuv + info->qp_offset_bit_depth, MIN_QUANT, MAX_QUANT_BASE + info->qp_offset_bit_depth );
                        cQPuv = (c_p_QPuv + c_q_QPuv + 1) >> 1;
                        EdgeLoopX(info, map, refp, SrcV, cQPuv - info->qp_offset_bit_depth, dir, s_c, 1, MbP, MbQ, mb_y << (LOOPFILTER_SIZE_IN_BIT - 2), mb_x << (LOOPFILTER_SIZE_IN_BIT - 2));
                    }
                }
            }
        }
        //}
    }//end loop dir
}

//deblock one CU
void DeblockBlk_avs2(COM_INFO *info, COM_MAP *map, COM_PIC * pic, COM_REFP refp[MAX_NUM_REF_PICS][REFP_NUM], int*** ppbEdgeFilter, int pel_x, int pel_y, int cuw, int cuh)
{
    for (int mb_y = pel_y >> LOOPFILTER_SIZE_IN_BIT; mb_y < ((pel_y + cuh) >> LOOPFILTER_SIZE_IN_BIT); mb_y++)
    {
        for (int mb_x = pel_x >> LOOPFILTER_SIZE_IN_BIT; mb_x < ((pel_x + cuw) >> LOOPFILTER_SIZE_IN_BIT); mb_x++)
        {
            //vertical
            DeblockMb_avs2(info, map, pic, refp, ppbEdgeFilter, mb_y, mb_x, 0);
        }
    }
    for (int mb_y = pel_y >> LOOPFILTER_SIZE_IN_BIT; mb_y < ((pel_y + cuh) >> LOOPFILTER_SIZE_IN_BIT); mb_y++)
    {
        for (int mb_x = pel_x >> LOOPFILTER_SIZE_IN_BIT; mb_x < ((pel_x + cuw) >> LOOPFILTER_SIZE_IN_BIT); mb_x++)
        {
            //horizontal
            DeblockMb_avs2(info, map, pic, refp, ppbEdgeFilter, mb_y, mb_x, 1);
        }
    }
}

int printf_flag = 0;

//deblock one frame
void DeblockFrame_avs2(COM_INFO *info, COM_MAP *map, COM_PIC * pic, COM_REFP refp[MAX_NUM_REF_PICS][REFP_NUM], int*** ppbEdgeFilter)
{
    int mb_x, mb_y;
    int blk_nr = -1;

    printf_flag = 1;

    for (mb_y = 0; mb_y < (pic->height_luma >> LOOPFILTER_SIZE_IN_BIT); mb_y++)
    {
        for (mb_x = 0; mb_x < (pic->width_luma >> LOOPFILTER_SIZE_IN_BIT); mb_x++)
        {
            blk_nr++;
            //vertical
            DeblockMb_avs2(info, map, pic, refp, ppbEdgeFilter, mb_y, mb_x, 0);
        }
    }
    blk_nr = -1;
    for (mb_y = 0; mb_y < (pic->height_luma >> LOOPFILTER_SIZE_IN_BIT); mb_y++)
    {
        for (mb_x = 0; mb_x < (pic->width_luma >> LOOPFILTER_SIZE_IN_BIT); mb_x++)
        {
            blk_nr++;
            //horizontal
            DeblockMb_avs2(info, map, pic, refp, ppbEdgeFilter, mb_y, mb_x, 1);
        }
    }
    printf_flag = 0;
}