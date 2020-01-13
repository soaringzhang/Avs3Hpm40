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
#include "dec_eco.h"
#include "com_df.h"
#include "com_sao.h"
#include "dec_DecAdaptiveLoopFilter.h"

/* convert DEC into DEC_CTX */
#define DEC_ID_TO_CTX_R(id, ctx) \
    com_assert_r((id)); \
    (ctx) = (DEC_CTX *)id; \
    com_assert_r((ctx)->magic == DEC_MAGIC_CODE);

/* convert DEC into DEC_CTX with return value if assert on */
#define DEC_ID_TO_CTX_RV(id, ctx, ret) \
    com_assert_rv((id), (ret)); \
    (ctx) = (DEC_CTX *)id; \
    com_assert_rv((ctx)->magic == DEC_MAGIC_CODE, (ret));

static DEC_CTX * ctx_alloc(void)
{
    DEC_CTX * ctx;
    ctx = (DEC_CTX*)com_malloc_fast(sizeof(DEC_CTX));
    com_assert_rv(ctx != NULL, NULL);
    com_mset_x64a(ctx, 0, sizeof(DEC_CTX));
    /* set default value */
    ctx->dtr = 0;
    ctx->pic_cnt = 0;
    return ctx;
}

static void ctx_free(DEC_CTX * ctx)
{
    com_mfree_fast(ctx);
}

static DEC_CORE * core_alloc(void)
{
    DEC_CORE * core;
    core = (DEC_CORE*)com_malloc_fast(sizeof(DEC_CORE));
    com_assert_rv(core, NULL);
    com_mset_x64a(core, 0, sizeof(DEC_CORE));
    return core;
}

static void core_free(DEC_CORE * core)
{
    com_mfree_fast(core);
}

static void sequence_deinit(DEC_CTX * ctx)
{
    if( ctx->map.map_scu )
    {
        com_mfree( ctx->map.map_scu );
        ctx->map.map_scu = NULL;
    }
    if( ctx->map.map_split )
    {
        com_mfree( ctx->map.map_split );
        ctx->map.map_split = NULL;
    }
    if( ctx->map.map_ipm )
    {
        com_mfree( ctx->map.map_ipm );
        ctx->map.map_ipm = NULL;
    }
    if( ctx->map.map_cu_mode )
    {
        com_mfree( ctx->map.map_cu_mode );
        ctx->map.map_cu_mode = NULL;
    }
#if TB_SPLIT_EXT
    if( ctx->map.map_pb_tb_part )
    {
        com_mfree( ctx->map.map_pb_tb_part );
        ctx->map.map_pb_tb_part = NULL;
    }
#endif
    if( ctx->map.map_depth )
    {
        com_mfree( ctx->map.map_depth );
        ctx->map.map_depth = NULL;
    }
    if( ctx->map.map_patch_idx )
    {
        com_mfree( ctx->map.map_patch_idx );
        ctx->map.map_patch_idx = NULL;
    }

    DeleteEdgeFilter_avs2(ctx->ppbEdgeFilter, ctx->info.pic_height);

    if( ctx->pic_sao )
    {
        com_picbuf_free( ctx->pic_sao );
        ctx->pic_sao = NULL;
    }

    com_free_3d_SAOstatdate(ctx->saostatData,
                            ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
                                        ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C);
    com_free_2d_SAOParameter(ctx->saoBlkParams,
                             ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
                                         ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)));
    com_free_2d_SAOParameter(ctx->rec_saoBlkParams,
                             ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
                                         ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)));

    if (ctx->pic_alf_Rec)
    {
        com_picbuf_free(ctx->pic_alf_Rec);
        ctx->pic_alf_Rec = NULL;
    }
    if (ctx->pic_alf_Dec)
    {
        com_picbuf_free(ctx->pic_alf_Dec);
        ctx->pic_alf_Dec = NULL;
    }
    ReleaseAlfGlobalBuffer(ctx);
    ctx->info.pic_header.pic_alf_on = NULL;
    ctx->info.pic_header.m_alfPictureParam = NULL;
    com_picman_deinit(&ctx->dpm);
}

static int sequence_init(DEC_CTX * ctx, COM_SQH * sqh)
{
    int size;
    int ret;

    ctx->info.bit_depth_internal = (sqh->encoding_precision == 2) ? 10 : 8;
    assert(sqh->sample_precision == 1 || sqh->sample_precision == 2);
    ctx->info.bit_depth_input = (sqh->sample_precision == 1) ? 8 : 10;
    ctx->info.qp_offset_bit_depth = (8 * (ctx->info.bit_depth_internal - 8));

    sequence_deinit(ctx);
    ctx->info.pic_width  = ((sqh->horizontal_size + MINI_SIZE - 1) / MINI_SIZE) * MINI_SIZE;
    ctx->info.pic_height = ((sqh->vertical_size   + MINI_SIZE - 1) / MINI_SIZE) * MINI_SIZE;
    ctx->info.max_cuwh = 1 << sqh->log2_max_cu_width_height;
    ctx->info.log2_max_cuwh = CONV_LOG2(ctx->info.max_cuwh);

    size = ctx->info.max_cuwh;
    ctx->info.pic_width_in_lcu = (ctx->info.pic_width + (size - 1)) / size;
    ctx->info.pic_height_in_lcu = (ctx->info.pic_height + (size - 1)) / size;
    ctx->info.f_lcu = ctx->info.pic_width_in_lcu * ctx->info.pic_height_in_lcu;
    ctx->info.pic_width_in_scu = (ctx->info.pic_width + ((1 << MIN_CU_LOG2) - 1)) >> MIN_CU_LOG2;
    ctx->info.pic_height_in_scu = (ctx->info.pic_height + ((1 << MIN_CU_LOG2) - 1)) >> MIN_CU_LOG2;
    ctx->info.f_scu = ctx->info.pic_width_in_scu * ctx->info.pic_height_in_scu;
    /* alloc SCU map */
    if (ctx->map.map_scu == NULL)
    {
        size = sizeof(u32) * ctx->info.f_scu;
        ctx->map.map_scu = (u32 *)com_malloc(size);
        com_assert_gv(ctx->map.map_scu, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_scu, 0, size);
    }
    /* alloc cu mode SCU map */
    if (ctx->map.map_cu_mode == NULL)
    {
        size = sizeof(u32) * ctx->info.f_scu;
        ctx->map.map_cu_mode = (u32 *)com_malloc(size);
        com_assert_gv(ctx->map.map_cu_mode, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_cu_mode, 0, size);
    }
    /* alloc map for CU split flag */
    if (ctx->map.map_split == NULL)
    {
        size = sizeof(s8) * ctx->info.f_lcu * MAX_CU_DEPTH * NUM_BLOCK_SHAPE * MAX_CU_CNT_IN_LCU;
        ctx->map.map_split = com_malloc(size);
        com_assert_gv(ctx->map.map_split, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_split, 0, size);
    }
    /* alloc map for intra prediction mode */
    if (ctx->map.map_ipm == NULL)
    {
        size = sizeof(s8) * ctx->info.f_scu;
        ctx->map.map_ipm = (s8 *)com_malloc(size);
        com_assert_gv(ctx->map.map_ipm, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_ipm, -1, size);
    }
    if (ctx->map.map_patch_idx == NULL)
    {
        size = sizeof(s8)* ctx->info.f_scu;
        ctx->map.map_patch_idx = (s8 *)com_malloc(size);
        com_assert_gv(ctx->map.map_patch_idx, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_patch_idx, -1, size);
    }
#if TB_SPLIT_EXT
    /* alloc map for pb and tb partition size (for luma and chroma) */
    if (ctx->map.map_pb_tb_part == NULL)
    {
        size = sizeof(u32) * ctx->info.f_scu;
        ctx->map.map_pb_tb_part = (u32 *)com_malloc(size);
        com_assert_gv(ctx->map.map_pb_tb_part, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        com_mset_x64a(ctx->map.map_pb_tb_part, 0xffffffff, size);
    }
#endif
    /* alloc map for intra depth */
    if( ctx->map.map_depth == NULL )
    {
        size = sizeof( s8 ) * ctx->info.f_scu;
        ctx->map.map_depth = com_malloc_fast( size );
        com_assert_gv( ctx->map.map_depth, ret, COM_ERR_OUT_OF_MEMORY, ERR );
        com_mset( ctx->map.map_depth, -1, size );
    }

    ctx->pa.width = ctx->info.pic_width;
    ctx->pa.height = ctx->info.pic_height;
    ctx->pa.pad_l = PIC_PAD_SIZE_L;
    ctx->pa.pad_c = PIC_PAD_SIZE_C;
    ret = com_picman_init(&ctx->dpm, MAX_PB_SIZE, MAX_NUM_REF_PICS, &ctx->pa);
    com_assert_g(COM_SUCCEEDED(ret), ERR);

    CreateEdgeFilter_avs2(ctx->info.pic_width, ctx->info.pic_height, ctx->ppbEdgeFilter);

    if (ctx->pic_sao == NULL)
    {
        ctx->pic_sao = com_pic_alloc(&ctx->pa, &ret);
        com_assert_rv(ctx->pic_sao != NULL, ret);
    }

    com_malloc_3d_SAOstatdate(&(ctx->saostatData),
                              ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
                                          ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C, NUM_SAO_NEW_TYPES);
    com_malloc_2d_SAOParameter(&(ctx->saoBlkParams),
                               ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
                                           ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C);
    com_malloc_2d_SAOParameter(&ctx->rec_saoBlkParams,
                               ((ctx->info.pic_width >> ctx->info.log2_max_cuwh) + (ctx->info.pic_width % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)) * ((
                                           ctx->info.pic_height >> ctx->info.log2_max_cuwh) + (ctx->info.pic_height % (1 << ctx->info.log2_max_cuwh) ? 1 : 0)), N_C);

    ctx->info.pic_header.tool_alf_on = ctx->info.sqh.adaptive_leveling_filter_enable_flag;
    if (ctx->pic_alf_Rec == NULL)
    {
        ctx->pic_alf_Rec = com_pic_alloc(&ctx->pa, &ret);
        com_assert_rv(ctx->pic_alf_Rec != NULL, ret);
    }
    if (ctx->pic_alf_Dec == NULL)
    {
        ctx->pic_alf_Dec = com_pic_alloc(&ctx->pa, &ret);
        com_assert_rv(ctx->pic_alf_Dec != NULL, ret);
    }
    CreateAlfGlobalBuffer(ctx);
    ctx->info.pic_header.pic_alf_on = ctx->pic_alf_on;
    ctx->info.pic_header.m_alfPictureParam = ctx->Dec_ALF->m_alfPictureParam;
    /*initialize patch info */
#if PATCH
    PATCH_INFO * patch = NULL;
    patch = (PATCH_INFO *)com_malloc_fast(sizeof(PATCH_INFO));
    ctx->patch = patch;
    ctx->patch->stable = sqh->patch_stable;
    ctx->patch->cross_patch_loop_filter = sqh->cross_patch_loop_filter;
    ctx->patch->ref_colocated = sqh->patch_ref_colocated;
    if (ctx->patch->stable)
    {
        ctx->patch->uniform = sqh->patch_uniform;
        if (ctx->patch->uniform)
        {
            ctx->patch->width = sqh->patch_width_minus1 + 1;
            ctx->patch->height = sqh->patch_height_minus1 + 1;
            ctx->patch->columns = ctx->info.pic_width_in_lcu / ctx->patch->width;
            ctx->patch->rows = ctx->info.pic_height_in_lcu / ctx->patch->height;
            /*set column_width and row_height*/
            for (int i = 0; i < ctx->patch->columns; i++)
            {
                ctx->patch_column_width[i] = ctx->patch->width;
            }
            if (ctx->info.pic_width_in_lcu%ctx->patch->width != 0)
            {
                if (ctx->patch->columns == 0)
                {
                    ctx->patch_column_width[ctx->patch->columns] = ctx->info.pic_width_in_lcu - ctx->patch->width*ctx->patch->columns;
                    ctx->patch->columns++;
                }
                else
                {
                    ctx->patch_column_width[ctx->patch->columns - 1] = ctx->patch_column_width[ctx->patch->columns - 1] + ctx->info.pic_width_in_lcu - ctx->patch->width*ctx->patch->columns;
                }
            }
            for (int i = 0; i < ctx->patch->rows; i++)
            {
                ctx->patch_row_height[i] = ctx->patch->height;
            }
            if (ctx->info.pic_height_in_lcu%ctx->patch->height != 0)
            {
                if (ctx->patch->rows == 0)
                {
                    ctx->patch_row_height[ctx->patch->rows] = ctx->info.pic_height_in_lcu - ctx->patch->height*ctx->patch->rows;
                    ctx->patch->rows++;
                }
                else
                {
#if PATCH_M4839
                    ctx->patch_row_height[ctx->patch->rows] = ctx->info.pic_height_in_lcu - ctx->patch->height*ctx->patch->rows;
                    ctx->patch->rows++;
#else
                    ctx->patch_row_height[ctx->patch->rows - 1] = ctx->patch_row_height[ctx->patch->rows - 1] + ctx->info.pic_height_in_lcu - ctx->patch->height*ctx->patch->rows;
#endif
                }
            }
            /*pointer to the data*/
            ctx->patch->width_in_lcu = ctx->patch_column_width;
            ctx->patch->height_in_lcu = ctx->patch_row_height;
        }
    }
#endif

    return COM_OK;
ERR:
    sequence_deinit(ctx);
    ctx->init_flag = 0;
    return ret;
}

static int slice_init(DEC_CTX * ctx, DEC_CORE * core, COM_PIC_HEADER * pic_header)
{
    core->lcu_num = 0;
    core->x_lcu = 0;
    core->y_lcu = 0;
    core->x_pel = 0;
    core->y_pel = 0;

    ctx->dtr_prev_low = pic_header->dtr;
    ctx->dtr = pic_header->dtr;
    ctx->ptr = pic_header->dtr; /* PTR */

    core->cnt_hmvp_cands = 0;
    com_mset_x64a(core->motion_cands, 0, sizeof(COM_MOTION)*ALLOWED_HMVP_NUM);

    /* clear maps */
    com_mset_x64a(ctx->map.map_scu, 0, sizeof(u32) * ctx->info.f_scu);
    com_mset_x64a(ctx->map.map_cu_mode, 0, sizeof(u32) * ctx->info.f_scu);
#if LIBVC_ON
    if (ctx->info.pic_header.slice_type == SLICE_I || (ctx->info.sqh.library_picture_enable_flag && ctx->info.pic_header.is_RLpic_flag))
#else
    if (ctx->info.pic_header.slice_type == SLICE_I)
#endif
    {
        ctx->last_intra_ptr = ctx->ptr;
    }

    return COM_OK;
}

static void make_stat(DEC_CTX * ctx, int btype, DEC_STAT * stat)
{
    int i, j;
    stat->read = 0;
    stat->ctype = btype;
    stat->stype = 0;
    stat->fnum = -1;
#if LIBVC_ON
    stat->is_RLpic_flag = ctx->info.pic_header.is_RLpic_flag;
#endif
    if (ctx)
    {
        stat->read = COM_BSR_GET_READ_BYTE(&ctx->bs);
        if (btype == COM_CT_SLICE)
        {
            stat->fnum = ctx->pic_cnt;
            stat->stype = ctx->info.pic_header.slice_type;
            /* increase decoded picture count */
            ctx->pic_cnt++;
            stat->poc = ctx->ptr;
            for (i = 0; i < 2; i++)
            {
                stat->refpic_num[i] = ctx->dpm.num_refp[i];
                for (j = 0; j < stat->refpic_num[i]; j++)
                {
#if LIBVC_ON
                    stat->refpic[i][j] = ctx->refp[j][i].pic->ptr;
#else
                    stat->refpic[i][j] = ctx->refp[j][i].ptr;
#endif
                }
            }
        }
        else if (btype == COM_CT_PICTURE)
        {
            stat->fnum = -1;
            stat->stype = ctx->info.pic_header.slice_type;
            stat->poc = ctx->info.pic_header.dtr;
            for (i = 0; i < 2; i++)
            {
                stat->refpic_num[i] = ctx->dpm.num_refp[i];
                for (j = 0; j < stat->refpic_num[i]; j++)
                {
#if LIBVC_ON
                    stat->refpic[i][j] = ctx->refp[j][i].pic->ptr;
#else
                    stat->refpic[i][j] = ctx->refp[j][i].ptr;
#endif
                }
            }
        }
    }

#if PRINT_SQH_PARAM_DEC
    stat->internal_bit_depth = ctx->info.sqh.encoding_precision == 2 ? 10 : 8;;

    stat->intra_tools = (ctx->info.sqh.tscpm_enable_flag << 0) + (ctx->info.sqh.ipf_enable_flag << 1) + (ctx->info.sqh.dt_intra_enable_flag << 2) + (ctx->info.sqh.ipcm_enable_flag << 3);
    
    stat->inter_tools = (ctx->info.sqh.affine_enable_flag << 0) + (ctx->info.sqh.amvr_enable_flag << 1) + (ctx->info.sqh.umve_enable_flag << 2) + (ctx->info.sqh.emvr_enable_flag << 3);
    stat->inter_tools+= (ctx->info.sqh.smvd_enable_flag << 4) + (ctx->info.sqh.num_of_hmvp_cand << 10);
    
    stat->trans_tools = (ctx->info.sqh.secondary_transform_enable_flag << 0) + (ctx->info.sqh.position_based_transform_enable_flag << 1);

    stat->filte_tools = (ctx->info.sqh.sample_adaptive_offset_enable_flag << 0) + (ctx->info.sqh.adaptive_leveling_filter_enable_flag << 1);
#endif
}

static void get_nbr_yuv(int x, int y, int cu_width, int cu_height, u16 avail_cu, COM_PIC *pic_rec, pel nb[N_C][N_REF][MAX_CU_SIZE * 3], int scup, u32 *map_scu, int pic_width_in_scu, int h_scu, u8 include_y, int bit_depth)
{
    int  s_rec;
    pel *rec;
    if (include_y)
    {
        /* Y */
        s_rec = pic_rec->stride_luma;
        rec = pic_rec->y + (y * s_rec) + x;
        com_get_nbr(x, y, cu_width, cu_height, rec, s_rec, avail_cu, nb, scup, map_scu, pic_width_in_scu, h_scu, bit_depth, Y_C);
    }

    cu_width >>= 1;
    cu_height >>= 1;
    x >>= 1;
    y >>= 1;
    s_rec = pic_rec->stride_chroma;
    /* U */
    rec = pic_rec->u + (y * s_rec) + x;
    com_get_nbr(x, y, cu_width, cu_height, rec, s_rec, avail_cu, nb, scup, map_scu, pic_width_in_scu, h_scu, bit_depth, U_C);
    /* V */
    rec = pic_rec->v + (y * s_rec) + x;
    com_get_nbr(x, y, cu_width, cu_height, rec, s_rec, avail_cu, nb, scup, map_scu, pic_width_in_scu, h_scu, bit_depth, V_C);
}

static int dec_eco_unit(DEC_CTX * ctx, DEC_CORE * core, int x, int y, int cu_width_log2, int cu_height_log2, int cud)
{
    int ret, cu_width, cu_height;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int bit_depth = ctx->info.bit_depth_internal;
    s16 resi[N_C][MAX_CU_DIM];
    cu_width = 1 << cu_width_log2;
    cu_height = 1 << cu_height_log2;

    mod_info_curr->x_pos = x;
    mod_info_curr->y_pos = y;
    mod_info_curr->x_scu = PEL2SCU(x);
    mod_info_curr->y_scu = PEL2SCU(y);
    mod_info_curr->scup = mod_info_curr->x_scu + mod_info_curr->y_scu * ctx->info.pic_width_in_scu;
    mod_info_curr->cu_width = cu_width;
    mod_info_curr->cu_height= cu_height;
    mod_info_curr->cu_width_log2 = cu_width_log2;
    mod_info_curr->cu_height_log2 = cu_height_log2;
    mod_info_curr->cud = cud;

    COM_TRACE_COUNTER;
    COM_TRACE_STR("ptr: ");
    COM_TRACE_INT(ctx->ptr);
    COM_TRACE_STR("x pos ");
    COM_TRACE_INT(x);
    COM_TRACE_STR("y pos ");
    COM_TRACE_INT(y);
    COM_TRACE_STR("width ");
    COM_TRACE_INT(cu_width);
    COM_TRACE_STR("height ");
    COM_TRACE_INT(cu_height);
    COM_TRACE_STR("cons mode ");
    COM_TRACE_INT(ctx->cons_pred_mode);
    COM_TRACE_STR("tree status ");
    COM_TRACE_INT(ctx->tree_status);
    COM_TRACE_STR("\n");

    /* parse CU info */
    if (ctx->tree_status != TREE_C)
    {
        ret = dec_decode_cu(ctx, core);
    }
    else
    {
        ret = dec_decode_cu_chroma(ctx, core);
    }
    com_assert_g(ret == COM_OK, ERR);

    /* inverse transform and dequantization */
    if (mod_info_curr->cu_mode != MODE_SKIP)
    {
#if IPCM
        int use_secTrans = ctx->info.sqh.secondary_transform_enable_flag && mod_info_curr->cu_mode == MODE_INTRA && mod_info_curr->ipm[PB0][0] != IPD_IPCM && ctx->tree_status != TREE_C;
        int use_alt4x4Trans = ctx->info.sqh.secondary_transform_enable_flag && mod_info_curr->cu_mode == MODE_INTRA && mod_info_curr->ipm[PB0][0] != IPD_IPCM && ctx->tree_status != TREE_C;
#else
        int use_secTrans = ctx->info.sqh.secondary_transform_enable_flag && mod_info_curr->cu_mode == MODE_INTRA && ctx->tree_status != TREE_C;
        int use_alt4x4Trans = ctx->info.sqh.secondary_transform_enable_flag && mod_info_curr->cu_mode == MODE_INTRA && ctx->tree_status != TREE_C;
#endif
        int secT_Ver_Hor[MAX_NUM_TB] = { 0, 0, 0, 0 };
        if (use_secTrans) // derive secT_Ver_Hor for each tb
        {
            int pb_part_size = mod_info_curr->pb_part;
            int tb_w, tb_h, tb_x, tb_y, tb_scup, tb_x_scu, tb_y_scu;
            for (int pb_idx = 0; pb_idx < core->mod_info_curr.pb_info.num_sub_part; pb_idx++)
            {
                int num_tb_in_pb = get_part_num_tb_in_pb(pb_part_size, pb_idx);
                int pb_w = mod_info_curr->pb_info.sub_w[pb_idx];
                int pb_h = mod_info_curr->pb_info.sub_h[pb_idx];
                int pb_x = mod_info_curr->pb_info.sub_x[pb_idx];
                int pb_y = mod_info_curr->pb_info.sub_y[pb_idx];

                for (int tb_idx = 0; tb_idx < num_tb_in_pb; tb_idx++)
                {
                    get_tb_width_height_in_pb(pb_w, pb_h, pb_part_size, pb_idx, &tb_w, &tb_h);
                    //derive tu basic info
                    int tb_idx_in_cu = tb_idx + get_tb_idx_offset(pb_part_size, pb_idx);
                    get_tb_pos_in_pb(pb_x, pb_y, pb_part_size, tb_w, tb_h, tb_idx, &tb_x, &tb_y);
                    tb_x_scu = PEL2SCU(tb_x);
                    tb_y_scu = PEL2SCU(tb_y);
                    tb_scup = tb_x_scu + (tb_y_scu * ctx->info.pic_width_in_scu);

                    int avail_tb = com_get_avail_intra(tb_x_scu, tb_y_scu, ctx->info.pic_width_in_scu, tb_scup, ctx->map.map_scu);
                    s8 intra_mode = mod_info_curr->ipm[pb_idx][0];
                    int tb_width_log2 = com_tbl_log2[tb_w];
                    int tb_height_log2 = com_tbl_log2[tb_h];
                    assert(tb_width_log2 >= 2 && tb_height_log2 >= 2);

                    if (tb_width_log2 > 2 || tb_height_log2 > 2)
                    {
                        int vt, ht;
                        int block_available_up = IS_AVAIL(avail_tb, AVAIL_UP);
                        int block_available_left = IS_AVAIL(avail_tb, AVAIL_LE);
                        vt = intra_mode < IPD_HOR;
                        ht = (intra_mode > IPD_VER && intra_mode < IPD_CNT) || intra_mode <= IPD_BI;
                        vt = vt && block_available_up;
                        ht = ht && block_available_left;
                        secT_Ver_Hor[tb_idx_in_cu] = (vt << 1) | ht;
                    }
                }
            }
        }

        com_itdq_yuv(mod_info_curr, mod_info_curr->coef, resi, ctx->wq, cu_width_log2, cu_height_log2, core->qp_y, core->qp_u, core->qp_v, bit_depth, secT_Ver_Hor, use_alt4x4Trans);
    }
    if (ctx->tree_status != TREE_C)
    {
        dec_set_dec_info(ctx, core);
    }


    /* prediction */
    if (mod_info_curr->cu_mode != MODE_INTRA)
    {
        if (ctx->info.pic_header.slice_type == SLICE_P)
        {
            assert(REFI_IS_VALID(mod_info_curr->refi[REFP_0]) && !REFI_IS_VALID(mod_info_curr->refi[REFP_1]));
        }

        if (mod_info_curr->affine_flag)
        {
            com_affine_mc(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, bit_depth);
        }
        else
        {
            assert(mod_info_curr->pb_info.sub_scup[0] == mod_info_curr->scup);
            com_mc(&ctx->info, mod_info_curr, ctx->refp, &ctx->map, ctx->tree_status, bit_depth);
        }
        /* reconstruction */
        com_recon_yuv(mod_info_curr->tb_part, x, y, cu_width, cu_height, resi, mod_info_curr->pred, mod_info_curr->num_nz, ctx->pic, ctx->tree_status, bit_depth);
    }
    else
    {
        //pred and recon for luma
        if (ctx->tree_status != TREE_C)
        {
#if IPCM
            if (mod_info_curr->ipm[PB0][0] == IPD_IPCM)
            {
                pel* rec_y;
                int i, j, rec_s;
                rec_s = ctx->pic->stride_luma;
                rec_y = ctx->pic->y + (y * rec_s) + x;

                for (i = 0; i < cu_height; i++)
                {
                    for (j = 0; j < cu_width; j++)
                    {
                        rec_y[i * rec_s + j] = mod_info_curr->coef[Y_C][i * cu_width + j] << (ctx->info.bit_depth_internal - ctx->info.bit_depth_input);
                    }
                }
            }
            else
            {
#endif
                for (int pb_idx = 0; pb_idx < mod_info_curr->pb_info.num_sub_part; pb_idx++)
                {
                    int pb_x = mod_info_curr->pb_info.sub_x[pb_idx];
                    int pb_y = mod_info_curr->pb_info.sub_y[pb_idx];
                    int pb_w = mod_info_curr->pb_info.sub_w[pb_idx];
                    int pb_h = mod_info_curr->pb_info.sub_h[pb_idx];
                    int pb_sucp = mod_info_curr->pb_info.sub_scup[pb_idx];
                    pred_recon_intra_luma_pb(ctx->pic, ctx->map.map_scu, ctx->map.map_ipm, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, resi, core->nb, mod_info_curr,
                                             pb_x, pb_y, pb_w, pb_h, pb_sucp, pb_idx, x, y, cu_width, cu_height, bit_depth);
                }
#if IPCM
            }
#endif
        }

        //pred and recon for uv
        if (ctx->tree_status != TREE_L)
        {
#if IPCM
            if (mod_info_curr->ipm[PB0][0] == IPD_IPCM && mod_info_curr->ipm[PB0][1] == IPD_DM_C)
            {
                pel* rec_u;
                pel* rec_v;
                int i, j, rec_s_c;
                rec_s_c = ctx->pic->stride_chroma;
                rec_u = ctx->pic->u + (y / 2 * rec_s_c) + x / 2;
                rec_v = ctx->pic->v + (y / 2 * rec_s_c) + x / 2;

                for (i = 0; i < cu_height / 2; i++)
                {
                    for (j = 0; j < cu_width / 2; j++)
                    {
                        rec_u[i * rec_s_c + j] = mod_info_curr->coef[U_C][i * cu_width / 2 + j] << (ctx->info.bit_depth_internal - ctx->info.bit_depth_input);
                        rec_v[i * rec_s_c + j] = mod_info_curr->coef[V_C][i * cu_width / 2 + j] << (ctx->info.bit_depth_internal - ctx->info.bit_depth_input);
                    }
                }
            }
            else
            {
#endif
                u16 avail_cu = com_get_avail_intra(mod_info_curr->x_scu, mod_info_curr->y_scu, ctx->info.pic_width_in_scu, mod_info_curr->scup, ctx->map.map_scu);
#if TSCPM
                get_nbr_yuv(x, y, cu_width, cu_height, avail_cu, ctx->pic, core->nb, mod_info_curr->scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, 1, bit_depth);
#else
                get_nbr_yuv(x, y, cu_width, cu_height, avail_cu, ctx->pic, core->nb, mod_info_curr->scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, 0, bit_depth);
#endif
#if TSCPM
                int strideY = ctx->pic->stride_luma;
                pel *piRecoY = ctx->pic->y + (y * strideY) + x;
#endif
                com_ipred_uv(core->nb[1][0] + 3, core->nb[1][1] + 3, mod_info_curr->pred[U_C], mod_info_curr->ipm[PB0][1], mod_info_curr->ipm[PB0][0], cu_width >> 1, cu_height >> 1, bit_depth, avail_cu
#if TSCPM
                             , U_C, piRecoY, strideY, core->nb
#endif
                            );
                com_ipred_uv(core->nb[2][0] + 3, core->nb[2][1] + 3, mod_info_curr->pred[V_C], mod_info_curr->ipm[PB0][1], mod_info_curr->ipm[PB0][0], cu_width >> 1, cu_height >> 1, bit_depth, avail_cu
#if TSCPM
                             , V_C, piRecoY, strideY, core->nb
#endif
                            );
                /* reconstruction */
                com_recon_yuv(mod_info_curr->tb_part, x, y, cu_width, cu_height, resi, mod_info_curr->pred, mod_info_curr->num_nz, ctx->pic, CHANNEL_C, bit_depth);
#if IPCM
            }
#endif
        }
    }
#if TRACE_REC
    {
        int s_rec;
        pel* rec;
        if (ctx->tree_status != TREE_C)
        {
            s_rec = ctx->pic->stride_luma;
            rec = ctx->pic->y + (y * s_rec) + x;
            COM_TRACE_STR("rec y\n");
            for (int j = 0; j < cu_height; j++)
            {
                for (int i = 0; i < cu_width; i++)
                {
                    COM_TRACE_INT(rec[i]);
                    COM_TRACE_STR(" ");
                }
                COM_TRACE_STR("\n");
                rec += s_rec;
            }
        }

        if (ctx->tree_status != TREE_L)
        {
            cu_width = cu_width >> 1;
            cu_height = cu_height >> 1;
            s_rec = ctx->pic->stride_chroma;
            rec = ctx->pic->u + ((y >> 1) * s_rec) + (x >> 1);
            COM_TRACE_STR("rec u\n");
            for (int j = 0; j < cu_height; j++)
            {
                for (int i = 0; i < cu_width; i++)
                {
                    COM_TRACE_INT(rec[i]);
                    COM_TRACE_STR(" ");
                }
                COM_TRACE_STR("\n");
                rec += s_rec;
            }

            rec = ctx->pic->v + ((y >> 1) * s_rec) + (x >> 1);
            COM_TRACE_STR("rec v\n");
            for (int j = 0; j < cu_height; j++)
            {
                for (int i = 0; i < cu_width; i++)
                {
                    COM_TRACE_INT(rec[i]);
                    COM_TRACE_STR(" ");
                }
                COM_TRACE_STR("\n");
                rec += s_rec;
            }
        }
    }
#endif
    return COM_OK;
ERR:
    return ret;
}

static int dec_eco_tree(DEC_CTX * ctx, DEC_CORE * core, int x0, int y0, int cu_width_log2, int cu_height_log2, int cup, int cud, COM_BSR * bs, DEC_SBAC * sbac
                        , const int parent_split, int qt_depth, int bet_depth, u8 cons_pred_mode, u8 tree_status)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int ret;
    s8  split_mode;
    int cu_width, cu_height;
    int bound;
    cu_width = 1 << cu_width_log2;
    cu_height = 1 << cu_height_log2;

    if (cu_width > MIN_CU_SIZE || cu_height > MIN_CU_SIZE)
    {
        COM_TRACE_COUNTER;
        COM_TRACE_STR("x pos ");
        COM_TRACE_INT(core->x_pel + ((cup % (ctx->info.max_cuwh >> MIN_CU_LOG2) << MIN_CU_LOG2)));
        COM_TRACE_STR("y pos ");
        COM_TRACE_INT(core->y_pel + ((cup / (ctx->info.max_cuwh >> MIN_CU_LOG2) << MIN_CU_LOG2)));
        COM_TRACE_STR("width ");
        COM_TRACE_INT(cu_width);
        COM_TRACE_STR("height ");
        COM_TRACE_INT(cu_height);
        COM_TRACE_STR("depth ");
        COM_TRACE_INT(cud);
        COM_TRACE_STR("\n");
        split_mode = dec_eco_split_mode(ctx, bs, sbac, cu_width, cu_height, parent_split, qt_depth, bet_depth, x0, y0);
    }
    else
    {
        split_mode = NO_SPLIT;
    }
    com_set_split_mode(split_mode, cud, cup, cu_width, cu_height, ctx->info.max_cuwh, core->split_mode);
    bound = !(x0 + cu_width <= ctx->info.pic_width && y0 + cu_height <= ctx->info.pic_height);
    if (split_mode != NO_SPLIT)
    {
        COM_SPLIT_STRUCT split_struct;
        com_split_get_part_structure(split_mode, x0, y0, cu_width, cu_height, cup, cud, ctx->info.log2_max_cuwh - MIN_CU_LOG2, &split_struct);
        u8 tree_status_child = TREE_LC;
        u8 cons_pred_mode_child = NO_MODE_CONS;
#if CHROMA_NOT_SPLIT
        tree_status_child = (tree_status == TREE_LC && com_tree_split(cu_width, cu_height, split_mode, ctx->info.pic_header.slice_type)) ? TREE_L : tree_status;
#endif
#if MODE_CONS
        if (cons_pred_mode == NO_MODE_CONS && com_constrain_pred_mode(cu_width, cu_height, split_mode, ctx->info.pic_header.slice_type))
        {
            cons_pred_mode_child = dec_eco_cons_pred_mode_child(bs, sbac);
        }
        else
        {
            cons_pred_mode_child = cons_pred_mode;
        }
#endif
        for (int part_num = 0; part_num < split_struct.part_count; ++part_num)
        {
            int cur_part_num = part_num;
            int log2_sub_cuw = split_struct.log_cuw[cur_part_num];
            int log2_sub_cuh = split_struct.log_cuh[cur_part_num];
            int x_pos = split_struct.x_pos[cur_part_num];
            int y_pos = split_struct.y_pos[cur_part_num];

            if (x_pos < ctx->info.pic_width && y_pos < ctx->info.pic_height)
            {
                ret = dec_eco_tree(ctx, core, x_pos, y_pos, log2_sub_cuw, log2_sub_cuh, split_struct.cup[cur_part_num], split_struct.cud, bs, sbac
                                   , split_mode, INC_QT_DEPTH(qt_depth, split_mode), INC_BET_DEPTH(bet_depth, split_mode), cons_pred_mode_child, tree_status_child);
                com_assert_g(ret == COM_OK, ERR);
            }
        }
#if CHROMA_NOT_SPLIT
        if (tree_status_child == TREE_L && tree_status == TREE_LC)
        {
            ctx->tree_status = TREE_C;
            ctx->cons_pred_mode = NO_MODE_CONS;
            ret = dec_eco_unit(ctx, core, x0, y0, cu_width_log2, cu_height_log2, cud);
            com_assert_g(ret == COM_OK, ERR);
            ctx->tree_status = TREE_LC;
        }
#endif
    }
    else
    {
#if CHROMA_NOT_SPLIT
        ctx->tree_status = tree_status;
#endif
#if MODE_CONS
        ctx->cons_pred_mode = cons_pred_mode;
#endif
        ret = dec_eco_unit(ctx, core, x0, y0, cu_width_log2, cu_height_log2, cud);
        com_assert_g(ret == COM_OK, ERR);

        if (ctx->info.sqh.num_of_hmvp_cand && core->mod_info_curr.cu_mode != MODE_INTRA && !core->mod_info_curr.affine_flag)
        {
            update_skip_candidates(core->motion_cands, &core->cnt_hmvp_cands, ctx->info.sqh.num_of_hmvp_cand, ctx->map.map_mv[mod_info_curr->scup], ctx->map.map_refi[mod_info_curr->scup]); // core->mod_info_curr.mv, core->mod_info_curr.refi);
        }
    }
    return COM_OK;
ERR:
    return ret;
}

int dec_deblock_avs2(DEC_CTX * ctx)
{
    ClearEdgeFilter_avs2(0, 0, ctx->pic->width_luma, ctx->pic->height_luma, ctx->ppbEdgeFilter);
    xSetEdgeFilter_avs2(&ctx->info, &ctx->map, ctx->pic, ctx->ppbEdgeFilter);
    DeblockFrame_avs2(&ctx->info, &ctx->map, ctx->pic, ctx->refp, ctx->ppbEdgeFilter);
    return COM_OK;
}

int dec_sao_avs2(DEC_CTX * ctx)
{
    Copy_frame_for_SAO(ctx->pic_sao, ctx->pic);
    SAOFrame(&ctx->info, &ctx->map, ctx->pic, ctx->pic_sao, ctx->rec_saoBlkParams);
    return COM_OK;
}

void readParaSAO_one_LCU(DEC_CTX* ctx, int y_pel, int x_pel,
                         SAOBlkParam *saoBlkParam, SAOBlkParam *rec_saoBlkParam)
{
    COM_SH_EXT *sh = &(ctx->info.shext);
    int lcuw = 1 << ctx->info.log2_max_cuwh;
    int lcuh = 1 << ctx->info.log2_max_cuwh;
    int lcu_pix_width = min(lcuw, ctx->info.pic_width - x_pel);
    int lcu_pix_height = min(lcuh, ctx->info.pic_height - y_pel);
    int x_in_lcu = x_pel >> ctx->info.log2_max_cuwh;
    int y_in_lcu = y_pel >> ctx->info.log2_max_cuwh;
    int lcu_pos = x_in_lcu + y_in_lcu * ctx->info.pic_width_in_lcu;
    if (!sh->slice_sao_enable[Y_C] && !sh->slice_sao_enable[U_C] && !sh->slice_sao_enable[V_C])
    {
        off_sao(rec_saoBlkParam);
        off_sao(saoBlkParam);
    }
    else
    {
        read_sao_lcu(ctx, lcu_pos, y_pel, x_pel, lcu_pix_width, lcu_pix_height, saoBlkParam, rec_saoBlkParam);
    }
}

void read_sao_lcu(DEC_CTX* ctx, int lcu_pos, int pix_y, int pix_x, int smb_pix_width, int smb_pix_height,
                  SAOBlkParam *sao_cur_param, SAOBlkParam *rec_sao_cur_param)
{
    COM_SH_EXT *sh = &(ctx->info.shext);
    int mb_x = pix_x >> MIN_CU_LOG2;
    int mb_y = pix_y >> MIN_CU_LOG2;
    SAOBlkParam merge_candidate[NUM_SAO_MERGE_TYPES][N_C];
    int merge_avail[NUM_SAO_MERGE_TYPES];
    int MergeLeftAvail, MergeUpAvail;
    int mergemode, saomode, saotype;
    int offset[32];
    int compIdx, i;
    int stBnd[2];
    DEC_SBAC   *sbac;
    COM_BSR     *bs;
    bs = &ctx->bs;
    sbac = GET_SBAC_DEC(bs);

    getSaoMergeNeighbor(&ctx->info, ctx->map.map_patch_idx, ctx->info.pic_width_in_scu, ctx->info.pic_width_in_lcu, lcu_pos, mb_y, mb_x,
                        ctx->rec_saoBlkParams, merge_avail, merge_candidate);
    MergeLeftAvail = merge_avail[0];
    MergeUpAvail = merge_avail[1];
    mergemode = 0;
    if (MergeLeftAvail + MergeUpAvail > 0)
    {
        mergemode = dec_eco_sao_mergeflag(sbac, bs, MergeLeftAvail, MergeUpAvail);
    }
    if (mergemode)
    {

        if (mergemode == 2)
        {
            copySAOParam_for_blk(rec_sao_cur_param, merge_candidate[SAO_MERGE_LEFT]);
        }
        else
        {
            assert(mergemode == 1);
            copySAOParam_for_blk(rec_sao_cur_param, merge_candidate[SAO_MERGE_ABOVE]);
        }
        copySAOParam_for_blk(sao_cur_param, rec_sao_cur_param);
    }
    else
    {
        for (compIdx = Y_C; compIdx < N_C; compIdx++)
        {
            if (!sh->slice_sao_enable[compIdx])
            {
                sao_cur_param[compIdx].modeIdc = SAO_MODE_OFF;
            }
            else
            {
                saomode = dec_eco_sao_mode(sbac, bs);
                switch (saomode)
                {
                case 0:
                    sao_cur_param[compIdx].modeIdc = SAO_MODE_OFF;
                    break;
                case 1:
                    sao_cur_param[compIdx].modeIdc = SAO_MODE_NEW;
                    sao_cur_param[compIdx].typeIdc = SAO_TYPE_BO;
                    break;
                case 2:
                    sao_cur_param[compIdx].modeIdc = SAO_MODE_NEW;
                    sao_cur_param[compIdx].typeIdc = SAO_TYPE_EO_0;
                    break;
                default:
                    assert(1);
                    break;
                }

                if (saomode)
                {
                    dec_eco_sao_offset(sbac, bs, &(sao_cur_param[compIdx]), offset);

                    if (sao_cur_param[compIdx].typeIdc == SAO_TYPE_BO)
                    {
                        dec_eco_sao_BO_start(sbac, bs, &(sao_cur_param[compIdx]), stBnd);
                        memset(sao_cur_param[compIdx].offset, 0, sizeof(int)*MAX_NUM_SAO_CLASSES);

                        for (i = 0; i < 2; i++)
                        {
                            sao_cur_param[compIdx].offset[stBnd[i]] = offset[i * 2];
                            sao_cur_param[compIdx].offset[(stBnd[i] + 1) % 32] = offset[i * 2 + 1];
                        }
                    }
                    else
                    {
                        saotype = dec_eco_sao_EO_type(sbac, bs, &(sao_cur_param[compIdx]));
                        assert(sao_cur_param[compIdx].typeIdc == SAO_TYPE_EO_0);
                        sao_cur_param[compIdx].typeIdc = saotype;
                        sao_cur_param[compIdx].offset[SAO_CLASS_EO_FULL_VALLEY] = offset[0];
                        sao_cur_param[compIdx].offset[SAO_CLASS_EO_HALF_VALLEY] = offset[1];
                        sao_cur_param[compIdx].offset[SAO_CLASS_EO_PLAIN] = 0;
                        sao_cur_param[compIdx].offset[SAO_CLASS_EO_HALF_PEAK] = offset[2];
                        sao_cur_param[compIdx].offset[SAO_CLASS_EO_FULL_PEAK] = offset[3];
                    }
                }
            }
        }
        copySAOParam_for_blk(rec_sao_cur_param, sao_cur_param);
    }
}


int dec_alf_avs2(DEC_CTX * ctx, COM_PIC *pic_rec)
{
    BOOL currAlfEnable = !(ctx->Dec_ALF->m_alfPictureParam[Y_C]->alf_flag == 0 &&
                           ctx->Dec_ALF->m_alfPictureParam[U_C]->alf_flag == 0 && ctx->Dec_ALF->m_alfPictureParam[V_C]->alf_flag == 0);
    if (currAlfEnable)
    {
        ctx->pic_alf_Dec->stride_luma = pic_rec->stride_luma;
        ctx->pic_alf_Dec->stride_chroma = pic_rec->stride_chroma;
        Copy_frame_for_ALF(ctx->pic_alf_Dec, pic_rec);
        ALFProcess_dec(ctx, ctx->Dec_ALF->m_alfPictureParam, pic_rec, ctx->pic_alf_Dec);
    }
    return COM_OK;
}

int dec_pic(DEC_CTX * ctx, DEC_CORE * core, COM_SQH *sqh, COM_PIC_HEADER * ph, COM_SH_EXT * shext)
{
    COM_BSR   * bs;
    DEC_SBAC  * sbac;
    int         ret;
    int         size;
    COM_PIC_HEADER *sh = &ctx->info.pic_header;
    int last_lcu_qp;
    int last_lcu_delta_qp;
#if PATCH
    int patch_cur_index = -1;
    PATCH_INFO *patch = ctx->patch;
    int patch_cur_lcu_x;
    int patch_cur_lcu_y;
    u32 * map_scu_temp;
    s8(*map_refi_temp)[REFP_NUM];
    s16(*map_mv_temp)[REFP_NUM][MV_D];
    u32 *map_cu_mode_temp;
    map_scu_temp = com_malloc_fast(sizeof(u32)* ctx->info.f_scu);
    map_refi_temp = com_malloc_fast(sizeof(s8)* ctx->info.f_scu * REFP_NUM);
    map_mv_temp = com_malloc_fast(sizeof(s16)* ctx->info.f_scu * REFP_NUM * MV_D);
    map_cu_mode_temp = com_malloc_fast(sizeof(u32)* ctx->info.f_scu);
    com_mset_x64a(map_scu_temp, 0, sizeof(u32)* ctx->info.f_scu);
    com_mset_x64a(map_refi_temp, -1, sizeof(s8)* ctx->info.f_scu * REFP_NUM);
    com_mset_x64a(map_mv_temp, 0, sizeof(s16)* ctx->info.f_scu * REFP_NUM * MV_D);
    com_mset_x64a(map_cu_mode_temp, 0, sizeof(u32)* ctx->info.f_scu);
#endif
    size = sizeof(s8) * ctx->info.f_scu * REFP_NUM;
    com_mset_x64a(ctx->map.map_refi, -1, size);
    size = sizeof(s16) * ctx->info.f_scu * REFP_NUM * MV_D;
    com_mset_x64a(ctx->map.map_mv, 0, size);
    bs = &ctx->bs;
    sbac = GET_SBAC_DEC(bs);
    /* reset SBAC */
    dec_sbac_init(bs);
    com_sbac_ctx_init(&(sbac->ctx));

    last_lcu_qp = ctx->info.shext.slice_qp;
    last_lcu_delta_qp = 0;
#if PATCH
    /*initial patch info*/
    patch->x_pel = patch->x_pat = patch->y_pel = patch->y_pat = patch->idx = 0;
    patch_cur_lcu_x = 0;
    patch_cur_lcu_y = 0;
    patch->left_pel = patch->x_pel;
    patch->up_pel = patch->y_pel;
    patch->right_pel = patch->x_pel + (*(patch->width_in_lcu + patch->x_pat) << ctx->info.log2_max_cuwh);
    patch->down_pel = patch->y_pel + (*(patch->height_in_lcu + patch->y_pat) << ctx->info.log2_max_cuwh);
    ctx->lcu_cnt = ctx->info.f_lcu;
#endif
    while (1)
    {
        int lcu_qp;
        int adj_qp_cb, adj_qp_cr;
#if PATCH
        /*set patch idx*/
        if (patch_cur_index != patch->idx)
            dec_set_patch_idx(ctx->map.map_patch_idx, patch, ctx->info.pic_width, ctx->info.pic_height);
        patch_cur_index = patch->idx;
#endif

        // reset HMVP list before each LCU line for parallel computation
#if PATCH
        if (core->x_lcu * ctx->info.max_cuwh == patch->left_pel)
        {
#else
        if (core->x_lcu == 0)
        {
#endif
            core->cnt_hmvp_cands = 0;
            com_mset_x64a(core->motion_cands, 0, sizeof(COM_MOTION)*ALLOWED_HMVP_NUM);
        }
        com_assert_rv(core->lcu_num < ctx->info.f_lcu, COM_ERR_UNEXPECTED);

        if (ctx->info.shext.fixed_slice_qp_flag)
        {
            lcu_qp = ctx->info.shext.slice_qp;
        }
        else
        {
            last_lcu_delta_qp = dec_eco_lcu_delta_qp(bs, sbac, last_lcu_delta_qp);
            lcu_qp = last_lcu_qp + last_lcu_delta_qp;
        }

        core->qp_y = lcu_qp;
        adj_qp_cb = core->qp_y + sh->chroma_quant_param_delta_cb - ctx->info.qp_offset_bit_depth;
        adj_qp_cr = core->qp_y + sh->chroma_quant_param_delta_cr - ctx->info.qp_offset_bit_depth;
        adj_qp_cb = COM_CLIP( adj_qp_cb, MIN_QUANT - 16, MAX_QUANT_BASE );
        adj_qp_cr = COM_CLIP( adj_qp_cr, MIN_QUANT - 16, MAX_QUANT_BASE );
        if (adj_qp_cb >= 0)
        {
            adj_qp_cb = com_tbl_qp_chroma_adjust[COM_MIN(MAX_QUANT_BASE, adj_qp_cb)];
        }
        if (adj_qp_cr >= 0)
        {
            adj_qp_cr = com_tbl_qp_chroma_adjust[COM_MIN(MAX_QUANT_BASE, adj_qp_cr)];
        }
        core->qp_u = COM_CLIP( adj_qp_cb + ctx->info.qp_offset_bit_depth, MIN_QUANT, MAX_QUANT_BASE + ctx->info.qp_offset_bit_depth );
        core->qp_v = COM_CLIP( adj_qp_cr + ctx->info.qp_offset_bit_depth, MIN_QUANT, MAX_QUANT_BASE + ctx->info.qp_offset_bit_depth );
        last_lcu_qp = lcu_qp;

        if (ctx->info.sqh.sample_adaptive_offset_enable_flag)
        {
            int lcu_pos = core->x_lcu + core->y_lcu * ctx->info.pic_width_in_lcu;
            readParaSAO_one_LCU(ctx, core->y_pel, core->x_pel, ctx->saoBlkParams[lcu_pos], ctx->rec_saoBlkParams[lcu_pos]);
        }

        for (int compIdx = 0; compIdx < N_C; compIdx++)
        {
            int lcu_pos = core->x_lcu + core->y_lcu * ctx->info.pic_width_in_lcu;
            if (ctx->pic_alf_on[compIdx])
            {
                ctx->Dec_ALF->m_AlfLCUEnabled[lcu_pos][compIdx] = dec_eco_AlfLCUCtrl(bs, sbac, compIdx, 0);
            }
            else
            {
                ctx->Dec_ALF->m_AlfLCUEnabled[lcu_pos][compIdx] = FALSE;
            }
        }

        /* invoke coding_tree() recursion */
        com_mset(core->split_mode, 0, sizeof(s8) * MAX_CU_DEPTH * NUM_BLOCK_SHAPE * MAX_CU_CNT_IN_LCU);
        ret = dec_eco_tree(ctx, core, core->x_pel, core->y_pel, ctx->info.log2_max_cuwh, ctx->info.log2_max_cuwh, 0, 0, bs, sbac
                           , NO_SPLIT, 0, 0, NO_MODE_CONS, TREE_LC);
        com_assert_g(COM_SUCCEEDED(ret), ERR);
        /* set split flags to map */
        com_mcpy(ctx->map.map_split[core->lcu_num], core->split_mode, sizeof(s8) * MAX_CU_DEPTH * NUM_BLOCK_SHAPE * MAX_CU_CNT_IN_LCU);
        /* read end_of_picture_flag */
#if PATCH
        /* aec_lcu_stuffing_bit and byte alignment for bit = 1 case inside the function */
        dec_sbac_decode_bin_trm(bs, sbac);
        ctx->lcu_cnt--;
        if (ctx->lcu_cnt <= 0)
        {
            /*update and store map_scu*/
            de_copy_lcu_scu(map_scu_temp, ctx->map.map_scu, map_refi_temp, ctx->map.map_refi, map_mv_temp, ctx->map.map_mv, map_cu_mode_temp, ctx->map.map_cu_mode, ctx->patch, ctx->info.pic_width, ctx->info.pic_height);
            break;
        }
#else
        if (dec_sbac_decode_bin_trm(bs, sbac))
        {
            break;
        }
#endif
        core->x_lcu++;
#if PATCH
        /*prepare next patch*/
        if (core->x_lcu >= *(patch->width_in_lcu + patch->x_pat) + patch_cur_lcu_x)
        {
            core->x_lcu = patch_cur_lcu_x;
            core->y_lcu++;
            if (core->y_lcu >= *(patch->height_in_lcu + patch->y_pat) + patch_cur_lcu_y)
            {
                /*decode patch end*/
                ret = dec_eco_send(bs);
                com_assert_rv(ret == COM_OK, ret);
                while (com_bsr_next(bs, 24) != 0x1)
                {
                    com_bsr_read(bs, 8);
                };
                /*decode patch head*/
                ret = dec_eco_patch_header(bs, sqh, ph, shext,patch);
                last_lcu_qp = shext->slice_qp;
                last_lcu_delta_qp = 0;
                com_assert_rv(ret == COM_OK, ret);
                /*update and store map_scu*/
                de_copy_lcu_scu(map_scu_temp, ctx->map.map_scu, map_refi_temp, ctx->map.map_refi, map_mv_temp, ctx->map.map_mv, map_cu_mode_temp, ctx->map.map_cu_mode, ctx->patch, ctx->info.pic_width, ctx->info.pic_height);
                com_mset_x64a(ctx->map.map_scu, 0, sizeof(u32)* ctx->info.f_scu);
                com_mset_x64a(ctx->map.map_refi, -1, sizeof(s8)* ctx->info.f_scu * REFP_NUM);
                com_mset_x64a(ctx->map.map_mv, 0, sizeof(s16)* ctx->info.f_scu * REFP_NUM * MV_D);
                com_mset_x64a(ctx->map.map_cu_mode, 0, sizeof(u32) * ctx->info.f_scu);
                /* reset SBAC */
                dec_sbac_init(bs);
                com_sbac_ctx_init(&(sbac->ctx));
                /*set patch location*/
                patch->x_pat = patch->idx % patch->columns;
                patch->y_pat = patch->idx / patch->columns;
                core->x_lcu = core->y_lcu = 0;
                /*set location at above-left of patch*/
                for (int i = 0; i < patch->x_pat; i++)
                {
                    core->x_lcu += *(patch->width_in_lcu + i);
                }
                for (int i = 0; i < patch->y_pat; i++)
                {
                    core->y_lcu += *(patch->height_in_lcu + i);
                }
                patch_cur_lcu_x = core->x_lcu;
                patch_cur_lcu_y = core->y_lcu;
                core->x_pel = core->x_lcu << ctx->info.log2_max_cuwh;
                core->y_pel = core->y_lcu << ctx->info.log2_max_cuwh;
                patch->x_pel = core->x_pel;
                patch->y_pel = core->y_pel;
                /*reset the patch boundary*/
                patch->left_pel = patch->x_pel;
                patch->up_pel = patch->y_pel;
                patch->right_pel = patch->x_pel + (*(patch->width_in_lcu + patch->x_pat) << ctx->info.log2_max_cuwh);
                patch->down_pel = patch->y_pel + (*(patch->height_in_lcu + patch->y_pat) << ctx->info.log2_max_cuwh);
            }
        }
#else
        if (core->x_lcu == ctx->info.pic_width_in_lcu)
        {
            core->x_lcu = 0;
            core->y_lcu++;
        }
#endif
        core->x_pel = core->x_lcu << ctx->info.log2_max_cuwh;
        core->y_pel = core->y_lcu << ctx->info.log2_max_cuwh;
#if PATCH
        core->lcu_num = core->x_lcu + core->y_lcu*ctx->info.pic_width_in_lcu;
#else
        core->lcu_num++;
#endif
    }
#if PATCH
    /*get scu from storage*/
    patch->left_pel = 0;
    patch->up_pel = 0;
    patch->right_pel = ctx->info.pic_width;
    patch->down_pel = ctx->info.pic_height;
    de_copy_lcu_scu(ctx->map.map_scu, map_scu_temp, ctx->map.map_refi, map_refi_temp, ctx->map.map_mv, map_mv_temp, ctx->map.map_cu_mode, map_cu_mode_temp, ctx->patch, ctx->info.pic_width, ctx->info.pic_height);
#endif

#if CHECK_ALL_CTX
    {
        static char check_point[sizeof(COM_SBAC_CTX) / sizeof(SBAC_CTX_MODEL)];
        static int fst_frm = 1;
        int i, num = sizeof(COM_SBAC_CTX) / sizeof(SBAC_CTX_MODEL);
        SBAC_CTX_MODEL *p = (SBAC_CTX_MODEL*)(&GET_SBAC_DEC(bs)->ctx);

        if (fst_frm == 1)
        {
            fst_frm = 0;
            memset(check_point, 0, num);
        }
        for (i = 0; i < num; i++)
        {
            unsigned long value;
            int check_val = (p[i] >> 16) & 0xFFFF;
            _BitScanReverse(&value, check_val);

            //if (i % 50 == 0) printf("\n");
            if (check_val && value > 0)
            {
                check_point[i] = 1;
            }
            //printf("%3d", value);
        }

#define CHECK_ONE_VAR(x) {                                              \
            int start_idx = offsetof(COM_SBAC_CTX, x) / 4;              \
            int i, length = sizeof(*(&GET_SBAC_DEC(bs)->ctx.x)) / 4;    \
            printf("\n%20s(%3d,%2d) : ", #x, start_idx, length);        \
            for (i = start_idx; i < start_idx + length; i++) {          \
                printf("%d ", check_point[i]);                          \
            }                                                           \
        }
        CHECK_ONE_VAR(skip_flag);
        CHECK_ONE_VAR(skip_idx_ctx);
        CHECK_ONE_VAR(direct_flag);
        CHECK_ONE_VAR(umve_flag);
        CHECK_ONE_VAR(umve_base_idx);
        CHECK_ONE_VAR(umve_step_idx);
        CHECK_ONE_VAR(umve_dir_idx);
        CHECK_ONE_VAR(inter_dir);
        CHECK_ONE_VAR(intra_dir);
        CHECK_ONE_VAR(pred_mode);
        CHECK_ONE_VAR(cons_mode);
        CHECK_ONE_VAR(ipf_flag);
        CHECK_ONE_VAR(refi);
        CHECK_ONE_VAR(mvr_idx);
        CHECK_ONE_VAR(affine_mvr_idx);
        CHECK_ONE_VAR(mvp_from_hmvp_flag);
        CHECK_ONE_VAR(mvd);
        CHECK_ONE_VAR(ctp_zero_flag);
        CHECK_ONE_VAR(cbf);
        CHECK_ONE_VAR(tb_split);
        CHECK_ONE_VAR(run);
        CHECK_ONE_VAR(last1);
        CHECK_ONE_VAR(last2);
        CHECK_ONE_VAR(level);
        CHECK_ONE_VAR(split_flag);
        CHECK_ONE_VAR(bt_split_flag);
        CHECK_ONE_VAR(split_dir);
        CHECK_ONE_VAR(split_mode);
        CHECK_ONE_VAR(affine_flag);
        CHECK_ONE_VAR(affine_mrg_idx);
        CHECK_ONE_VAR(smvd_flag);
        CHECK_ONE_VAR(part_size);
        CHECK_ONE_VAR(sao_merge_flag);
        CHECK_ONE_VAR(sao_mode);
        CHECK_ONE_VAR(sao_offset);
        CHECK_ONE_VAR(alf_lcu_enable);
        CHECK_ONE_VAR(delta_qp);

        //for (i = 0; i < num; i++) {
        //    if (i % 50 == 0) {
        //        printf("\n");
        //    }
        //    else if(i % 10 == 0) {
        //        printf(" (%3d) ",i);
        //    }
        //    printf("%2d", check_point[i]);
        //}
    }
#endif

    /*decode patch end*/
    ret = dec_eco_send(bs);
    while (com_bsr_next(bs, 24) != 0x1)
    {
        com_bsr_read(bs, 8);
    }
    com_assert_rv(ret == COM_OK, ret);
    return COM_OK;
ERR:
    return ret;
}
#if PATCH
void de_copy_lcu_scu(u32 * scu_temp, u32 * scu_best, s8(*refi_temp)[REFP_NUM], s8(*refi_best)[REFP_NUM], s16(*mv_temp)[REFP_NUM][MV_D], s16(*mv_best)[REFP_NUM][MV_D], u32 *cu_mode_temp, u32 *cu_mode_best, PATCH_INFO * patch, int pic_width, int pic_height)
{
    /*get the boundary*/
    int left_scu = patch->left_pel >> MIN_CU_LOG2;
    int right_scu = patch->right_pel >> MIN_CU_LOG2;
    int down_scu = patch->down_pel >> MIN_CU_LOG2;
    int up_scu = patch->up_pel >> MIN_CU_LOG2;
    int width_scu = pic_width >> MIN_CU_LOG2;
    int height_scu = pic_height >> MIN_CU_LOG2;
    /*copy*/
    int scu = up_scu*width_scu + left_scu;
    for (int j = up_scu; j < COM_MIN(height_scu, down_scu); j++)
    {
        int temp = scu;
        for (int i = left_scu; i < COM_MIN(width_scu, right_scu); i++)
        {
            //map_patch_idx[scu]=(s8)patch->idx;
            scu_temp[scu] = scu_best[scu];
            cu_mode_temp[scu] = cu_mode_best[scu];
            for (int m = 0; m < REFP_NUM; m++)
            {
                refi_temp[scu][m] = refi_best[scu][m];
            }
            for (int m = 0; m < REFP_NUM; m++)
            {
                for (int n = 0; n < MV_D; n++)
                {
                    mv_temp[scu][m][n] = mv_best[scu][m][n];
                }
            }
            scu++;
        }
        scu = temp;
        scu += width_scu;
    }
}

void dec_set_patch_idx(s8 *map_patch_idx, PATCH_INFO * patch, int pic_width, int pic_height)
{
    /*get the boundary*/
    int left_scu = patch->left_pel >> MIN_CU_LOG2;
    int right_scu = patch->right_pel >> MIN_CU_LOG2;
    int down_scu = patch->down_pel >> MIN_CU_LOG2;
    int up_scu = patch->up_pel >> MIN_CU_LOG2;
    int width_scu = pic_width >> MIN_CU_LOG2;
    int height_scu = pic_height >> MIN_CU_LOG2;
    /*set idx*/
    int scu = up_scu*width_scu + left_scu;
    for (int j = up_scu; j < COM_MIN(height_scu, down_scu); j++)
    {
        int temp = scu;
        for (int i = left_scu; i < COM_MIN(width_scu, right_scu); i++)
        {
            map_patch_idx[scu] = (s8)patch->idx;
            scu++;
        }
        scu = temp;
        scu += width_scu;
    }
}
#endif

int dec_ready(DEC_CTX *ctx)
{
    int ret = COM_OK;
    DEC_CORE *core = NULL;
    com_assert(ctx);
    core = core_alloc();
    com_assert_gv(core != NULL, ret, COM_ERR_OUT_OF_MEMORY, ERR);
    ctx->core = core;
    return COM_OK;
ERR:
    if (core)
    {
        core_free(core);
    }
    return ret;
}

void dec_flush(DEC_CTX * ctx)
{
    if (ctx->core)
    {
        core_free(ctx->core);
        ctx->core = NULL;
    }
}

int dec_cnk(DEC_CTX * ctx, COM_BITB * bitb, DEC_STAT * stat)
{
    COM_BSR  *bs;
    COM_PIC_HEADER   *pic_header;
    COM_SQH * sqh;
    COM_SH_EXT *shext;
    COM_CNKH *cnkh;
    int        ret = COM_OK;

    if (stat)
    {
        com_mset(stat, 0, sizeof(DEC_STAT));
    }
    bs  = &ctx->bs;
    sqh = &ctx->info.sqh;
    pic_header = &ctx->info.pic_header;
    shext = &ctx->info.shext;
    cnkh = &ctx->info.cnkh;

    /* set error status */
    ctx->bs_err = (u8)bitb->err;
#if TRACE_RDO_EXCLUDE_I
    if (pic_header->slice_type != SLICE_I)
    {
#endif
        COM_TRACE_SET(1);
#if TRACE_RDO_EXCLUDE_I
    }
    else
    {
        COM_TRACE_SET(0);
    }
#endif
    /* bitstream reader initialization */
    com_bsr_init(bs, bitb->addr, bitb->ssize, NULL);
    SET_SBAC_DEC(bs, &ctx->sbac_dec);

    if (bs->cur[3] == 0xB0)
    {
        cnkh->ctype = COM_CT_SQH;
        ret = dec_eco_sqh(bs, sqh);
        com_assert_rv(COM_SUCCEEDED(ret), ret);
#if LIBVC_ON
        ctx->dpm.libvc_data->is_libpic_processing = sqh->library_stream_flag;
        ctx->dpm.libvc_data->library_picture_enable_flag = sqh->library_picture_enable_flag;
        /*
        if (ctx->dpm.libvc_data->library_picture_enable_flag && !ctx->dpm.libvc_data->is_libpic_prepared)
        {
            ret = COM_ERR_UNEXPECTED;
            printf("\nError: when decode seq.bin with library picture enable, you need to input libpic.bin at the same time by using param: --input_libpics.");
            com_assert_rv(ctx->dpm.libvc_data->library_picture_enable_flag == ctx->dpm.libvc_data->is_libpic_prepared, ret);
        }
        */
#endif

#if EXTENSION_USER_DATA
        extension_and_user_data(ctx, bs, 0, sqh, pic_header);
#endif
        if( !ctx->init_flag )
        {
            ret = sequence_init(ctx, sqh);
            com_assert_rv(COM_SUCCEEDED(ret), ret);
            g_DOIPrev = g_CountDOICyCleTime = 0;
            ctx->init_flag = 1;
        }
    }
    else if( bs->cur[3] == 0xB1 )
    {
        ctx->init_flag = 0;
        cnkh->ctype = COM_CT_SEQ_END;
    }
    else if (bs->cur[3] == 0xB3 || bs->cur[3] == 0xB6)
    {
        cnkh->ctype = COM_CT_PICTURE;
        /* decode slice header */
        pic_header->low_delay = sqh->low_delay;
        int need_minus_256 = 0;
        ret = dec_eco_pic_header(bs, pic_header, sqh, &need_minus_256);
        if (need_minus_256)
        {
            com_picman_dpbpic_doi_minus_cycle_length( &ctx->dpm );
        }

        ctx->wq[0] = pic_header->wq_4x4_matrix;
        ctx->wq[1] = pic_header->wq_8x8_matrix;

        if (!sqh->library_stream_flag)
        {
            com_picman_check_repeat_doi(&ctx->dpm, pic_header);
        }

#if EXTENSION_USER_DATA && WRITE_MD5_IN_USER_DATA
        extension_and_user_data(ctx, bs, 1, sqh, pic_header);
#endif
        com_constrcut_ref_list_doi(pic_header);

        //add by Yuqun Fan, init rpl list at ph instead of sh
#if HLS_RPL
#if LIBVC_ON
        if (!sqh->library_stream_flag)
#endif
        {
            ret = com_picman_refpic_marking_decoder(&ctx->dpm, pic_header);
            com_assert_rv(ret == COM_OK, ret);
        }
        com_cleanup_useless_pic_buffer_in_pm(&ctx->dpm);

        /* reference picture lists construction */
        ret = com_picman_refp_rpl_based_init_decoder(&ctx->dpm, pic_header, ctx->refp);
#else
        /* initialize reference pictures */
        //ret = com_picman_refp_init(&ctx->dpm, ctx->info.sqh.num_ref_pics_act, sh->slice_type, ctx->ptr, ctx->info.sh.temporal_id, ctx->last_intra_ptr, ctx->refp);
#endif
        com_assert_rv(COM_SUCCEEDED(ret), ret);

    }
    else if (bs->cur[3] >= 0x00 && bs->cur[3] <= 0x8E)
    {
        cnkh->ctype = COM_CT_SLICE;
        ret = dec_eco_patch_header(bs, sqh, pic_header, shext, ctx->patch);
        /* initialize slice */
        ret = slice_init(ctx, ctx->core, pic_header);
        com_assert_rv(COM_SUCCEEDED(ret), ret);
        /* get available frame buffer for decoded image */
        ctx->pic = com_picman_get_empty_pic(&ctx->dpm, &ret);
        com_assert_rv(ctx->pic, ret);
        /* get available frame buffer for decoded image */
        ctx->map.map_refi = ctx->pic->map_refi;
        ctx->map.map_mv = ctx->pic->map_mv;
        /* decode slice layer */
        ret = dec_pic(ctx, ctx->core, sqh, pic_header, shext);
        com_assert_rv(COM_SUCCEEDED(ret), ret);
        /* deblocking filter */
        if (ctx->info.pic_header.loop_filter_disable_flag == 0)
        {
            ret = dec_deblock_avs2(ctx);
            com_assert_rv(COM_SUCCEEDED(ret), ret);
        }
        /* sao filter */
        if (ctx->info.sqh.sample_adaptive_offset_enable_flag)
        {
            ret = dec_sao_avs2(ctx);
            com_assert_rv(ret == COM_OK, ret);
        }
        /* ALF */
        if (ctx->info.sqh.adaptive_leveling_filter_enable_flag)
        {
            ret = dec_alf_avs2(ctx, ctx->pic);
            com_assert_rv(COM_SUCCEEDED(ret), ret);
        }
        /* MD5 check for testing encoder-decoder match*/
        if (ctx->use_pic_sign && ctx->pic_sign_exist)
        {
            ret = dec_picbuf_check_signature(ctx->pic, ctx->pic_sign);
            com_assert_rv(COM_SUCCEEDED(ret), ret);
            ctx->pic_sign_exist = 0; /* reset flag */
        }
#if PIC_PAD_SIZE_L > 0
        /* expand pixels to padding area */
        dec_picbuf_expand(ctx, ctx->pic);
#endif
        /* put decoded picture to DPB */
#if LIBVC_ON
        if (sqh->library_stream_flag)
        {
            ret = com_picman_put_libpic(&ctx->dpm, ctx->pic, ctx->info.pic_header.slice_type, ctx->ptr, pic_header->decode_order_index, ctx->info.pic_header.temporal_id, 1, ctx->refp, pic_header);
        }
        else
#endif
        {
            ret = com_picman_put_pic(&ctx->dpm, ctx->pic, ctx->info.pic_header.slice_type, ctx->ptr, pic_header->decode_order_index, 
                                     pic_header->picture_output_delay, ctx->info.pic_header.temporal_id, 1, ctx->refp);
            assert((&ctx->dpm)->cur_pb_size <= sqh->max_dpb_size);
        }
        com_assert_rv(COM_SUCCEEDED(ret), ret);
    }
    else
    {
        return COM_ERR_MALFORMED_BITSTREAM;
    }
    make_stat(ctx, cnkh->ctype, stat);
    return ret;
}

int dec_pull_frm(DEC_CTX *ctx, COM_IMGB **imgb, int state)
{
    int ret;
    COM_PIC *pic;
    *imgb = NULL;
    int library_picture_index;
#if LIBVC_ON
    // output lib pic and corresponding library_picture_index
    if (ctx->info.sqh.library_stream_flag)
    {
        pic = ctx->pic;
        library_picture_index = ctx->info.pic_header.library_picture_index;

        //output to the buffer outside the decoder
        ret = com_picman_out_libpic(pic, library_picture_index, &ctx->dpm);

        //Don't output the reconstructed libpics to yuv. Because lib pic should not display.

        return ret;
    }
    else
#endif
    {
        pic = com_picman_out_pic( &ctx->dpm, &ret, ctx->info.pic_header.decode_order_index, state ); //MX: doi is not increase mono, but in the range of [0,255]
        if (pic)
        {
            com_assert_rv(pic->imgb != NULL, COM_ERR);
            /* increase reference count */
            pic->imgb->addref(pic->imgb);
            *imgb = pic->imgb;
        }
        return ret;
    }
}

DEC dec_create(DEC_CDSC * cdsc, int * err)
{
    DEC_CTX *ctx = NULL;
    int ret;
#if ENC_DEC_TRACE
    fopen_s(&fp_trace, "dec_trace.txt", "w+");
#endif
    ctx = ctx_alloc();
    com_assert_gv(ctx != NULL, ret, COM_ERR_OUT_OF_MEMORY, ERR);
    com_mcpy(&ctx->cdsc, cdsc, sizeof(DEC_CDSC));
    ret = com_scan_tbl_init();
    com_assert_g(ret == COM_OK, ERR);
    ret = dec_ready(ctx);
    com_assert_g(ret == COM_OK, ERR);
    /* Set CTX variables to default value */
    ctx->magic = DEC_MAGIC_CODE;
    ctx->id = (DEC)ctx;
    ctx->init_flag = 0;

    init_dct_coef();
    return (ctx->id);
ERR:
    if (ctx)
    {
        dec_flush(ctx);
        ctx_free(ctx);
    }
    if (err) *err = ret;
    return NULL;
}

void dec_delete(DEC id)
{
    DEC_CTX *ctx;
    DEC_ID_TO_CTX_R(id, ctx);
#if ENC_DEC_TRACE
    fclose(fp_trace);
#endif
    sequence_deinit(ctx);
#if LIBVC_ON
    ctx->dpm.libvc_data = NULL;
#endif
    dec_flush(ctx);
    ctx_free(ctx);
    com_scan_tbl_delete();
}

int dec_config(DEC id, int cfg, void * buf, int * size)
{
    DEC_CTX *ctx;
    DEC_ID_TO_CTX_RV(id, ctx, COM_ERR_INVALID_ARGUMENT);
    switch (cfg)
    {
    /* set config ************************************************************/
    case DEC_CFG_SET_USE_PIC_SIGNATURE:
        ctx->use_pic_sign = (*((int *)buf)) ? 1 : 0;
        break;
    /* get config ************************************************************/
    default:
        com_assert_rv(0, COM_ERR_UNSUPPORTED);
    }
    return COM_OK;
}

int dec_decode(DEC id, COM_BITB * bitb, DEC_STAT * stat)
{
    return 0;
}