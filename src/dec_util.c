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

#include "dec_def.h"

void dec_picbuf_expand(DEC_CTX * ctx, COM_PIC * pic)
{
    com_picbuf_expand(pic, pic->padsize_luma, pic->padsize_chroma);
}

int dec_picbuf_check_signature(COM_PIC * pic, u8 signature[16])
{
    u8 pic_sign[16];
    int ret;
    /* execute MD5 digest here */
    ret = com_picbuf_signature(pic, pic_sign);
    com_assert_rv(COM_SUCCEEDED(ret), ret);
    com_assert_rv(com_mcmp(signature, pic_sign, 16)==0, COM_ERR_BAD_CRC);
    return COM_OK;
}


void dec_set_dec_info(DEC_CTX * ctx, DEC_CORE * core)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    s8  (*map_refi)[REFP_NUM];
    s16 (*map_mv)[REFP_NUM][MV_D];
    u32  *map_scu;
    s8   *map_ipm;
    u32  *map_cu_mode;
#if TB_SPLIT_EXT
    u32  *map_pb_tb_part;
#endif
    s8   *map_depth;
    int   w_cu;
    int   h_cu;
    int   scup;
    int   pic_width_in_scu;
    int   i, j;
    int   flag;
    int   cu_cbf_flag;

    scup = mod_info_curr->scup;
    w_cu = (1 << mod_info_curr->cu_width_log2) >> MIN_CU_LOG2;
    h_cu = (1 << mod_info_curr->cu_height_log2) >> MIN_CU_LOG2;
    pic_width_in_scu = ctx->info.pic_width_in_scu;
    map_refi = ctx->map.map_refi + scup;
    map_scu = ctx->map.map_scu + scup;
    map_mv = ctx->map.map_mv + scup;
    map_ipm = ctx->map.map_ipm + scup;
    map_cu_mode = ctx->map.map_cu_mode + scup;
#if TB_SPLIT_EXT
    map_pb_tb_part = ctx->map.map_pb_tb_part + scup;
#endif
    map_depth = ctx->map.map_depth + scup;

    flag = (mod_info_curr->cu_mode == MODE_INTRA) ? 1 : 0;

#if CHROMA_NOT_SPLIT // decoder cu_cbf derivation for deblocking
    if (ctx->tree_status == TREE_C)
    {
        return;
    }
    if (ctx->tree_status == TREE_L)
    {
        assert( core->mod_info_curr.num_nz[TBUV0][U_C] == 0 && core->mod_info_curr.num_nz[TBUV0][V_C] == 0 );
    }
#endif

    cu_cbf_flag = is_cu_nz(core->mod_info_curr.num_nz);

    for (i = 0; i < h_cu; i++)
    {
        for (j = 0; j < w_cu; j++)
        {
            int pb_idx_y = get_part_idx(mod_info_curr->pb_part, j << 2, i << 2, w_cu << 2, h_cu << 2);
            if (mod_info_curr->cu_mode == MODE_SKIP)
            {
                MCU_SET_SF(map_scu[j]);
            }
            else
            {
                MCU_CLR_SF(map_scu[j]);
            }
            if (cu_cbf_flag)
            {
                MCU_SET_CBF(map_scu[j]);
            }
            else
            {
                MCU_CLR_CBF(map_scu[j]);
            }

            if (mod_info_curr->affine_flag)
            {
                MCU_SET_AFF(map_scu[j], mod_info_curr->affine_flag);
            }
            else
            {
                MCU_CLR_AFF(map_scu[j]);
            }

            MCU_SET_X_SCU_OFF(map_cu_mode[j], j);
            MCU_SET_Y_SCU_OFF(map_cu_mode[j], i);
            MCU_SET_LOGW(map_cu_mode[j], mod_info_curr->cu_width_log2);
            MCU_SET_LOGH(map_cu_mode[j], mod_info_curr->cu_height_log2);
            MCU_SET_IF_COD_SN_QP(map_scu[j], flag, ctx->slice_num, core->qp_y);
#if TB_SPLIT_EXT
            MCU_SET_TB_PART_LUMA(map_pb_tb_part[j], mod_info_curr->tb_part);
#endif
            map_refi[j][REFP_0] = mod_info_curr->refi[REFP_0];
            map_refi[j][REFP_1] = mod_info_curr->refi[REFP_1];
            map_mv[j][REFP_0][MV_X] = mod_info_curr->mv[REFP_0][MV_X];
            map_mv[j][REFP_0][MV_Y] = mod_info_curr->mv[REFP_0][MV_Y];
            map_mv[j][REFP_1][MV_X] = mod_info_curr->mv[REFP_1][MV_X];
            map_mv[j][REFP_1][MV_Y] = mod_info_curr->mv[REFP_1][MV_Y];
            map_ipm[j] = mod_info_curr->ipm[pb_idx_y][0];
            map_depth[j] = (s8)mod_info_curr->cud;
        }
        map_refi += pic_width_in_scu;
        map_mv += pic_width_in_scu;
        map_scu += pic_width_in_scu;
        map_ipm += pic_width_in_scu;
        map_depth += pic_width_in_scu;
        map_cu_mode += pic_width_in_scu;
#if TB_SPLIT_EXT
        map_pb_tb_part += pic_width_in_scu;
#endif
    }

    if (mod_info_curr->affine_flag)
    {
        com_set_affine_mvf(&ctx->info, mod_info_curr, ctx->refp, &ctx->map);
    }

#if MVF_TRACE
    // Trace MVF in decoder
    {
        map_refi = ctx->map.map_refi + scup;
        map_scu = ctx->map.map_scu + scup;
        map_mv = ctx->map.map_mv + scup;
        map_cu_mode = ctx->map.map_cu_mode + scup;
        for (i = 0; i < h_cu; i++)
        {
            for (j = 0; j < w_cu; j++)
            {
                COM_TRACE_COUNTER;
                COM_TRACE_STR(" x: ");
                COM_TRACE_INT(j);
                COM_TRACE_STR(" y: ");
                COM_TRACE_INT(i);
                COM_TRACE_STR(" ref0: ");
                COM_TRACE_INT(map_refi[j][REFP_0]);
                COM_TRACE_STR(" mv: ");
                COM_TRACE_MV(map_mv[j][REFP_0][MV_X], map_mv[j][REFP_0][MV_Y]);
                COM_TRACE_STR(" ref1: ");
                COM_TRACE_INT(map_refi[j][REFP_1]);
                COM_TRACE_STR(" mv: ");
                COM_TRACE_MV(map_mv[j][REFP_1][MV_X], map_mv[j][REFP_1][MV_Y]);

                COM_TRACE_STR(" affine: ");
                COM_TRACE_INT(MCU_GET_AFF(map_scu[j]));
                if (MCU_GET_AFF(map_scu[j]))
                {
                    COM_TRACE_STR(" logw: ");
                    COM_TRACE_INT(MCU_GET_LOGW(map_cu_mode[j]));
                    COM_TRACE_STR(" logh: ");
                    COM_TRACE_INT(MCU_GET_LOGH(map_cu_mode[j]));
                    COM_TRACE_STR(" xoff: ");
                    COM_TRACE_INT(MCU_GET_X_SCU_OFF(map_cu_mode[j]));
                    COM_TRACE_STR(" yoff: ");
                    COM_TRACE_INT(MCU_GET_Y_SCU_OFF(map_cu_mode[j]));
                }

                COM_TRACE_STR("\n");
            }
            map_refi += pic_width_in_scu;
            map_mv += pic_width_in_scu;
            map_scu += pic_width_in_scu;
            map_cu_mode += pic_width_in_scu;
        }
    }
#endif
}

void dec_derive_skip_direct_info(DEC_CTX * ctx, DEC_CORE * core)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int cu_width = (1 << mod_info_curr->cu_width_log2);
    int cu_height = (1 << mod_info_curr->cu_height_log2);

    if (mod_info_curr->affine_flag) // affine merge motion vector
    {
        s8  mrg_list_refi[AFF_MAX_NUM_MRG][REFP_NUM];
        int mrg_list_cp_num[AFF_MAX_NUM_MRG];
        CPMV mrg_list_cp_mv[AFF_MAX_NUM_MRG][REFP_NUM][VER_NUM][MV_D];
        int cp_idx, lidx;
        int mrg_idx = mod_info_curr->skip_idx;

        com_get_affine_merge_candidate(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, mrg_list_refi, mrg_list_cp_mv, mrg_list_cp_num, ctx->ptr);
        mod_info_curr->affine_flag = (u8)mrg_list_cp_num[mrg_idx] - 1;

        COM_TRACE_COUNTER;
        COM_TRACE_STR("merge affine flag after constructed candidate ");
        COM_TRACE_INT(mod_info_curr->affine_flag);
        COM_TRACE_STR("\n");

        for (lidx = 0; lidx < REFP_NUM; lidx++)
        {
            mod_info_curr->mv[lidx][MV_X] = 0;
            mod_info_curr->mv[lidx][MV_Y] = 0;
            if (REFI_IS_VALID(mrg_list_refi[mrg_idx][lidx]))
            {
                mod_info_curr->refi[lidx] = mrg_list_refi[mrg_idx][lidx];
                for (cp_idx = 0; cp_idx < mrg_list_cp_num[mrg_idx]; cp_idx++)
                {
                    mod_info_curr->affine_mv[lidx][cp_idx][MV_X] = mrg_list_cp_mv[mrg_idx][lidx][cp_idx][MV_X];
                    mod_info_curr->affine_mv[lidx][cp_idx][MV_Y] = mrg_list_cp_mv[mrg_idx][lidx][cp_idx][MV_Y];
                }
            }
            else
            {
                mod_info_curr->refi[lidx] = REFI_INVALID;
                for (cp_idx = 0; cp_idx < mrg_list_cp_num[mrg_idx]; cp_idx++)
                {
                    mod_info_curr->affine_mv[lidx][cp_idx][MV_X] = 0;
                    mod_info_curr->affine_mv[lidx][cp_idx][MV_Y] = 0;
                }
            }
        }
    }
    else
    {
        s16 pmv_temp[REFP_NUM][MV_D];
        s8 refi_temp[REFP_NUM];
        refi_temp[REFP_0] = 0;
        refi_temp[REFP_1] = 0;
        int scup_co = get_colocal_scup(mod_info_curr->scup, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu);
        if (ctx->info.pic_header.slice_type == SLICE_P)
        {
            refi_temp[REFP_0] = 0;
            refi_temp[REFP_1] = -1;
            if (REFI_IS_VALID(ctx->refp[0][REFP_0].map_refi[scup_co][REFP_0]))
            {
                get_col_mv_from_list0(ctx->refp[0], ctx->ptr, scup_co, pmv_temp);
            }
            else
            {
                pmv_temp[REFP_0][MV_X] = 0;
                pmv_temp[REFP_0][MV_Y] = 0;
            }
            pmv_temp[REFP_1][MV_X] = 0;
            pmv_temp[REFP_1][MV_Y] = 0;
        }
        else
        {
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
            if (!REFI_IS_VALID(ctx->refp[0][REFP_1].map_refi[scup_co][REFP_0]) || ctx->refp[0][REFP_1].list_is_library_pic[ctx->refp[0][REFP_1].map_refi[scup_co][REFP_0]] || ctx->refp[0][REFP_1].is_library_picture || ctx->refp[0][REFP_0].is_library_picture)
#else
            if (!REFI_IS_VALID(ctx->refp[0][REFP_1].map_refi[scup_co][REFP_0]))
#endif
            {
                com_get_mvp_default(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, REFP_0, 0, 0, pmv_temp[REFP_0]);

                com_get_mvp_default(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->ptr, REFP_1, 0, 0, pmv_temp[REFP_1]);
            }
            else
            {
                get_col_mv(ctx->refp[0], ctx->ptr, scup_co, pmv_temp);
            }
        }

        if (!mod_info_curr->umve_flag)
        {
            int num_cands = 0;
            u8 spatial_skip_idx = mod_info_curr->skip_idx;
            s16 skip_pmv_cands[MAX_SKIP_NUM][REFP_NUM][MV_D];
            s8 skip_refi[MAX_SKIP_NUM][REFP_NUM];
            copy_mv(skip_pmv_cands[0][REFP_0], pmv_temp[REFP_0]);
            copy_mv(skip_pmv_cands[0][REFP_1], pmv_temp[REFP_1]);
            skip_refi[0][REFP_0] = refi_temp[REFP_0];
            skip_refi[0][REFP_1] = refi_temp[REFP_1];
            num_cands++;

            if (mod_info_curr->skip_idx != 0)
            {
                int skip_idx;
                COM_MOTION motion_cands_curr[MAX_SKIP_NUM];
                s8 cnt_hmvp_cands_curr = 0;
                derive_MHBskip_spatial_motions(&ctx->info, mod_info_curr, &ctx->map, &skip_pmv_cands[num_cands], &skip_refi[num_cands]);
                num_cands += PRED_DIR_NUM;
                if (ctx->info.sqh.num_of_hmvp_cand && mod_info_curr->skip_idx >= TRADITIONAL_SKIP_NUM)
                {
                    for (skip_idx = 0; skip_idx < num_cands; skip_idx++) // fill the traditional skip candidates
                    {
                        fill_skip_candidates(motion_cands_curr, &cnt_hmvp_cands_curr, ctx->info.sqh.num_of_hmvp_cand, skip_pmv_cands[skip_idx], skip_refi[skip_idx], 0);
                    }
                    assert(cnt_hmvp_cands_curr == TRADITIONAL_SKIP_NUM);
                    for (skip_idx = core->cnt_hmvp_cands; skip_idx > 0; skip_idx--) // fill the HMVP skip candidates
                    {
                        COM_MOTION motion = core->motion_cands[skip_idx - 1];
                        fill_skip_candidates(motion_cands_curr, &cnt_hmvp_cands_curr, ctx->info.sqh.num_of_hmvp_cand, motion.mv, motion.ref_idx, 1);
                    }

                    COM_MOTION motion = core->cnt_hmvp_cands ? core->motion_cands[core->cnt_hmvp_cands - 1] : motion_cands_curr[TRADITIONAL_SKIP_NUM - 1]; // use last HMVP candidate or last spatial candidate to fill the rest
                    for (skip_idx = cnt_hmvp_cands_curr; skip_idx < (TRADITIONAL_SKIP_NUM + ctx->info.sqh.num_of_hmvp_cand); skip_idx++) // fill skip candidates when hmvp not enough
                    {
                        fill_skip_candidates(motion_cands_curr, &cnt_hmvp_cands_curr, ctx->info.sqh.num_of_hmvp_cand, motion.mv, motion.ref_idx, 0);
                    }
                    assert(cnt_hmvp_cands_curr == (TRADITIONAL_SKIP_NUM + ctx->info.sqh.num_of_hmvp_cand));

                    get_hmvp_skip_cands(motion_cands_curr, cnt_hmvp_cands_curr, skip_pmv_cands, skip_refi);
                }
                assert(cnt_hmvp_cands_curr <= (TRADITIONAL_SKIP_NUM + ctx->info.sqh.num_of_hmvp_cand));
            }
            mod_info_curr->mv[REFP_0][MV_X] = skip_pmv_cands[spatial_skip_idx][REFP_0][MV_X];
            mod_info_curr->mv[REFP_0][MV_Y] = skip_pmv_cands[spatial_skip_idx][REFP_0][MV_Y];
            mod_info_curr->mv[REFP_1][MV_X] = skip_pmv_cands[spatial_skip_idx][REFP_1][MV_X];
            mod_info_curr->mv[REFP_1][MV_Y] = skip_pmv_cands[spatial_skip_idx][REFP_1][MV_Y];
            mod_info_curr->refi[REFP_0] = skip_refi[spatial_skip_idx][REFP_0];
            mod_info_curr->refi[REFP_1] = skip_refi[spatial_skip_idx][REFP_1];
        }
        else
        {
            int umve_idx = mod_info_curr->umve_idx;
            s16 pmv_base_cands[UMVE_BASE_NUM][REFP_NUM][MV_D];
            s8 refi_base_cands[UMVE_BASE_NUM][REFP_NUM];
            s16 pmv_umve_cands[UMVE_MAX_REFINE_NUM*UMVE_BASE_NUM][REFP_NUM][MV_D];
            s8 refi_umve_cands[UMVE_MAX_REFINE_NUM*UMVE_BASE_NUM][REFP_NUM];

#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
            derive_umve_base_motions(ctx->refp, &ctx->info, mod_info_curr, &ctx->map, pmv_temp, refi_temp, pmv_base_cands, refi_base_cands);
#else
            derive_umve_base_motions(&ctx->info, mod_info_curr, &ctx->map, pmv_temp, refi_temp, pmv_base_cands, refi_base_cands);
#endif
            derive_umve_final_motions(umve_idx, ctx->refp, ctx->ptr, pmv_base_cands, refi_base_cands, pmv_umve_cands, refi_umve_cands);
            mod_info_curr->mv[REFP_0][MV_X] = pmv_umve_cands[umve_idx][REFP_0][MV_X];
            mod_info_curr->mv[REFP_0][MV_Y] = pmv_umve_cands[umve_idx][REFP_0][MV_Y];
            mod_info_curr->mv[REFP_1][MV_X] = pmv_umve_cands[umve_idx][REFP_1][MV_X];
            mod_info_curr->mv[REFP_1][MV_Y] = pmv_umve_cands[umve_idx][REFP_1][MV_Y];
            mod_info_curr->refi[REFP_0] = refi_umve_cands[umve_idx][REFP_0];
            mod_info_curr->refi[REFP_1] = refi_umve_cands[umve_idx][REFP_1];
        }
    }
}