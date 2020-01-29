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
#include "kernel.h"

int pintra_init_frame(ENC_CTX * ctx)
{
    ENC_PINTRA * pi;
    COM_PIC     * pic;
    pi = &ctx->pintra;
    pic = PIC_ORG(ctx);
    pi->addr_org[Y_C] = pic->y;
    pi->addr_org[U_C] = pic->u;
    pi->addr_org[V_C] = pic->v;
    pi->stride_org[Y_C] = pic->stride_luma;
    pi->stride_org[U_C] = pic->stride_chroma;
    pi->stride_org[V_C] = pic->stride_chroma;

    pic = PIC_REC(ctx);
    pi->addr_rec_pic[Y_C] = pic->y;
    pi->addr_rec_pic[U_C] = pic->u;
    pi->addr_rec_pic[V_C] = pic->v;
    pi->stride_rec[Y_C] = pic->stride_luma;
    pi->stride_rec[U_C] = pic->stride_chroma;
    pi->stride_rec[V_C] = pic->stride_chroma;
    pi->slice_type = ctx->slice_type;
    pi->bit_depth = ctx->info.bit_depth_internal;
    return COM_OK;
}

int pintra_init_lcu(ENC_CTX * ctx, ENC_CORE * core)
{
    return COM_OK;
}

//Note: this is PB-based RDO
double pintra_residue_rdo(ENC_CTX *ctx, ENC_CORE *core, pel *org_luma, pel *org_cb, pel *org_cr, int s_org, int s_org_c, int cu_width_log2, int cu_height_log2,
                                 s32 *dist, int bChroma, int pb_idx, int x, int y)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    ENC_PINTRA *pi = &ctx->pintra;
    int bit_depth = ctx->info.bit_depth_internal;
    s16 coef_tmp[N_C][MAX_CU_DIM];
    s16 resi[MAX_CU_DIM];
    int cu_width, cu_height, bit_cnt;
    double cost = 0;
    u16 avail_tb;
    int s_mod = pi->stride_rec[Y_C];
    pel* mod;
    int num_nz_temp[MAX_NUM_TB][N_C];

    cu_width = 1 << cu_width_log2;
    cu_height = 1 << cu_height_log2;
    if (!bChroma)
    {
        int pb_part_size = mod_info_curr->pb_part;
        int num_tb_in_pb = get_part_num_tb_in_pb(pb_part_size, pb_idx);
        int pb_w = mod_info_curr->pb_info.sub_w[pb_idx];
        int pb_h = mod_info_curr->pb_info.sub_h[pb_idx];
        int pb_x = mod_info_curr->pb_info.sub_x[pb_idx];
        int pb_y = mod_info_curr->pb_info.sub_y[pb_idx];
        int tb_w, tb_h, tb_x, tb_y, tb_scup, tb_x_scu, tb_y_scu, coef_offset_tb;
        pel* pred_tb;
        cu_plane_nz_cln(mod_info_curr->num_nz, Y_C);

        get_tb_width_height_in_pb(pb_w, pb_h, pb_part_size, pb_idx, &tb_w, &tb_h);
        for (int tb_idx = 0; tb_idx < num_tb_in_pb; tb_idx++)
        {
            //derive tu basic info
            get_tb_pos_in_pb(pb_x, pb_y, pb_part_size, tb_w, tb_h, tb_idx, &tb_x, &tb_y);
            coef_offset_tb = tb_idx * tb_w * tb_h;
            tb_x_scu = PEL2SCU(tb_x);
            tb_y_scu = PEL2SCU(tb_y);
            tb_scup = tb_x_scu + (tb_y_scu * ctx->info.pic_width_in_scu);
            //get start of tb residual
            pel* resi_tb = resi + coef_offset_tb; //residual is stored sequentially, not in raster, like coef
            pel* rec_tb = pi->rec[Y_C] + (tb_y - mod_info_curr->y_pos) * cu_width + (tb_x - mod_info_curr->x_pos);

            avail_tb = com_get_avail_intra(tb_x_scu, tb_y_scu, ctx->info.pic_width_in_scu, tb_scup, ctx->map.map_scu);

            s8 intra_mode = mod_info_curr->ipm[pb_idx][0];
            int tb_width_log2 = com_tbl_log2[tb_w];
            int tb_height_log2 = com_tbl_log2[tb_h];
            assert(tb_width_log2 > 0 && tb_height_log2 > 0);
            int use_secTrans = ctx->info.sqh.secondary_transform_enable_flag && (tb_width_log2 > 2 || tb_height_log2 > 2);
            int use_alt4x4Trans = ctx->info.sqh.secondary_transform_enable_flag;
            int secT_Ver_Hor = 0;
            if (use_secTrans)
            {
                int vt, ht;
                int block_available_up = IS_AVAIL(avail_tb, AVAIL_UP);
                int block_available_left = IS_AVAIL(avail_tb, AVAIL_LE);
                vt = intra_mode < IPD_HOR;
                ht = (intra_mode > IPD_VER && intra_mode < IPD_CNT) || intra_mode <= IPD_BI;
                vt = vt && block_available_up;
                ht = ht && block_available_left;
                secT_Ver_Hor = (vt << 1) | ht;
            }

            //prediction according to the input ipm
            if (num_tb_in_pb == 1)
                pred_tb = pi->pred_cache[intra_mode];
            else
            {
                mod = pi->addr_rec_pic[Y_C] + (tb_y * s_mod) + tb_x;
                pred_tb = mod_info_curr->pred[Y_C]; // pred is temp memory
                com_get_nbr(tb_x, tb_y, tb_w, tb_h, mod, s_mod, avail_tb, core->nb, tb_scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, bit_depth, Y_C);
                KERNEL(com_ipred)(core->nb[0][0] + 3, core->nb[0][1] + 3, pred_tb, intra_mode, tb_w, tb_h, bit_depth, avail_tb, mod_info_curr->ipf_flag);
            }

            pel* org_luma_tb = org_luma + (tb_x - pb_x) + (tb_y - pb_y) * s_org;
            //trans & inverse trans
            enc_diff_16b(tb_width_log2, tb_height_log2, org_luma_tb, pred_tb, s_org, tb_w, tb_w, coef_tmp[Y_C]);
            mod_info_curr->num_nz[tb_idx][Y_C] = enc_tq_nnz(ctx, mod_info_curr, Y_C, 0, core->qp_y, ctx->lambda[0], coef_tmp[Y_C], coef_tmp[Y_C], tb_width_log2, tb_height_log2, pi->slice_type, Y_C, 1, secT_Ver_Hor, use_alt4x4Trans);

            s16* coef_tb = mod_info_curr->coef[Y_C] + coef_offset_tb;
            com_mcpy(coef_tb, coef_tmp[Y_C], sizeof(s16) * (tb_w * tb_h));
            if (mod_info_curr->num_nz[tb_idx][Y_C])
            {
                com_itdq(mod_info_curr, Y_C, 0, coef_tmp[Y_C], resi_tb, ctx->wq, tb_width_log2, tb_height_log2, core->qp_y, bit_depth, secT_Ver_Hor, use_alt4x4Trans);
            }

            //to fit the interface of com_recon()
            for (int comp = 0; comp < N_C; comp++)
                num_nz_temp[TB0][comp] = mod_info_curr->num_nz[tb_idx][comp];
            com_recon(SIZE_2Nx2N, resi_tb, pred_tb, num_nz_temp, Y_C, tb_w, tb_h, cu_width, rec_tb, bit_depth);

            //dist calc
            cost += enc_ssd_16b(tb_width_log2, tb_height_log2, rec_tb, org_luma_tb, cu_width, s_org, bit_depth); // stride for rec_tb is cu_width
            if (mod_info_curr->pb_part == SIZE_2Nx2N)
            {
                calc_delta_dist_filter_boundary(ctx, core, PIC_REC(ctx), PIC_ORG(ctx), cu_width, cu_height, pi->rec, cu_width, x, y, 1, mod_info_curr->num_nz[TB0][Y_C] != 0, NULL, NULL, 0);
                cost += ctx->delta_dist;
            }

            //update map tb
            update_intra_info_map_scu(ctx->map.map_scu, ctx->map.map_ipm, pb_x, pb_y, pb_w, pb_h, ctx->info.pic_width_in_scu, intra_mode);
            copy_rec_y_to_pic(rec_tb, tb_x, tb_y, tb_w, tb_h, cu_width, PIC_REC(ctx));
        }

        *dist = (s32)cost;
        if (pb_idx == 0)
            SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
        else
            SBAC_LOAD(core->s_temp_run, core->s_temp_prev_comp_best);
        //enc_sbac_bit_reset(&core->s_temp_run);
        bit_cnt = enc_get_bit_number(&core->s_temp_run);
        enc_bit_est_pb_intra_luma(ctx, core, ctx->info.pic_header.slice_type, mod_info_curr->coef, pb_idx);
        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
        cost += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
    }
    else
    {
#if TSCPM
        int strideY = pi->stride_rec[Y_C];
        pel *piRecoY = pi->addr_rec_pic[Y_C] + (y * strideY) + x;  //x y 即亮度块在pic中的坐标
#endif

        u16 avail_cu = com_get_avail_intra(mod_info_curr->x_scu, mod_info_curr->y_scu, ctx->info.pic_width_in_scu, mod_info_curr->scup, ctx->map.map_scu);
        KERNEL(com_ipred_uv)(core->nb[1][0] + 3, core->nb[1][1] + 3, pi->pred[U_C], mod_info_curr->ipm[PB0][1], mod_info_curr->ipm[PB0][0], cu_width >> 1, cu_height >> 1, bit_depth, avail_cu
#if TSCPM
                     , U_C, piRecoY, strideY, core->nb
#endif
                    );
        enc_diff_16b(cu_width_log2 - 1, cu_height_log2 - 1, org_cb, pi->pred[U_C], s_org_c, cu_width >> 1, cu_width >> 1, coef_tmp[U_C]);
        mod_info_curr->num_nz[TB0][U_C] = enc_tq_nnz(ctx, mod_info_curr, U_C, 0, core->qp_u, ctx->lambda[1], coef_tmp[U_C], coef_tmp[U_C], cu_width_log2 - 1, cu_height_log2 - 1, pi->slice_type, U_C, 1, 0, 0);
        com_mcpy(mod_info_curr->coef[U_C], coef_tmp[U_C], sizeof(u16) * (cu_width * cu_height));
        if (mod_info_curr->num_nz[TB0][U_C])
        {
            com_itdq(mod_info_curr, U_C, 0, coef_tmp[U_C], resi, ctx->wq, mod_info_curr->cu_width_log2 - 1, mod_info_curr->cu_height_log2 - 1, core->qp_u, bit_depth, 0, 0);
        }

        com_recon(SIZE_2Nx2N, resi, pi->pred[U_C], mod_info_curr->num_nz, U_C, cu_width >> 1, cu_height >> 1, cu_width >> 1, pi->rec[U_C], bit_depth);
        KERNEL(com_ipred_uv)(core->nb[2][0] + 3, core->nb[2][1] + 3, pi->pred[V_C], mod_info_curr->ipm[PB0][1], mod_info_curr->ipm[PB0][0], cu_width >> 1, cu_height >> 1, bit_depth, avail_cu
#if TSCPM
                     , V_C, piRecoY, strideY, core->nb
#endif
                    );
        enc_diff_16b(cu_width_log2 - 1, cu_height_log2 - 1, org_cr, pi->pred[V_C], s_org_c, cu_width >> 1, cu_width >> 1, coef_tmp[V_C]);
        mod_info_curr->num_nz[TB0][V_C] = enc_tq_nnz(ctx, mod_info_curr, V_C, 0, core->qp_v, ctx->lambda[2], coef_tmp[V_C], coef_tmp[V_C], cu_width_log2 - 1, cu_height_log2 - 1, pi->slice_type, V_C, 1, 0, 0);
        com_mcpy(mod_info_curr->coef[V_C], coef_tmp[V_C], sizeof(u16) * (cu_width * cu_height));
        SBAC_LOAD(core->s_temp_run, core->s_temp_prev_comp_best);
        //enc_sbac_bit_reset(&core->s_temp_run);
        bit_cnt = enc_get_bit_number(&core->s_temp_run);
        enc_bit_est_intra_chroma(ctx, core, mod_info_curr->coef);
        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
        cost += ctx->dist_chroma_weight[0] * enc_ssd_16b(cu_width_log2 - 1, cu_height_log2 - 1, pi->rec[U_C], org_cb, cu_width >> 1, s_org_c, bit_depth);
        if (mod_info_curr->num_nz[TB0][V_C])
        {
            com_itdq(mod_info_curr, V_C, 0, coef_tmp[V_C], resi, ctx->wq, mod_info_curr->cu_width_log2 - 1, mod_info_curr->cu_height_log2 - 1, core->qp_v, bit_depth, 0, 0);
        }

        com_recon(SIZE_2Nx2N, resi, pi->pred[V_C], mod_info_curr->num_nz, V_C, cu_width >> 1, cu_height >> 1, cu_width >> 1, pi->rec[V_C], bit_depth);
        cost += ctx->dist_chroma_weight[1] * enc_ssd_16b(cu_width_log2 - 1, cu_height_log2 - 1, pi->rec[V_C], org_cr, cu_width >> 1, s_org_c, bit_depth);
        *dist = (s32)cost;
        cost += enc_ssd_16b(cu_width_log2, cu_height_log2, pi->rec[Y_C], org_luma, cu_width, s_org, bit_depth);
        cost += RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
    }
    return cost;
}

#define NUM_IPM_CAND 3
#define PUT_IPM2LIST(list, cnt, ipm)\
{\
    int idx_list, is_check = 0;\
    for(idx_list = 0; idx_list < (cnt); idx_list++)\
        if((ipm) == (list)[idx_list]) is_check = 1;\
    if(is_check == 0)\
    {\
        (list)[(cnt)] = (ipm); (cnt)++;\
    }\
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

static int make_ipred_list(ENC_CTX * ctx, ENC_CORE * core, int width, int height, int cu_width_log2, int cu_height_log2, pel * org, int s_org, int * ipred_list, int part_idx, u16 avail_cu
                           , int skip_ipd
                          )
{
    ENC_PINTRA * pi = &ctx->pintra;
    int bit_depth = ctx->info.bit_depth_internal;
    int pred_cnt, i, j;
    double cost, cand_cost[IPD_RDO_CNT];
    u32 cand_satd_cost[IPD_RDO_CNT];
    u32 cost_satd;
    const int ipd_rdo_cnt = (width >= height * 4 || height >= width * 4) ? IPD_RDO_CNT - 1 : IPD_RDO_CNT;

    for (i = 0; i < ipd_rdo_cnt; i++)
    {
        ipred_list[i] = IPD_DC;
        cand_cost[i] = MAX_COST;
        cand_satd_cost[i] = COM_UINT32_MAX;
    }
    pred_cnt = IPD_CNT;
    for (i = 0; i < IPD_CNT; i++)
    {
        if (skip_ipd == 1 && (i == IPD_PLN || i == IPD_BI || i == IPD_DC))
        {
            continue;
        }

        int bit_cnt, shift = 0;
        pel * pred_buf = NULL;
        pred_buf = pi->pred_cache[i];
        KERNEL(com_ipred)(core->nb[0][0] + 3, core->nb[0][1] + 3, pred_buf, i, width, height, bit_depth, avail_cu, core->mod_info_curr.ipf_flag);
        cost_satd = calc_satd_16b(width, height, org, pred_buf, s_org, width, bit_depth);
        cost = (double)cost_satd;
        SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
        //enc_sbac_bit_reset(&core->s_temp_run);
        bit_cnt = enc_get_bit_number(&core->s_temp_run);
        encode_intra_dir(&core->bs_temp, (u8)i, core->mod_info_curr.mpm[part_idx]);
        bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
        cost += RATE_TO_COST_SQRT_LAMBDA(ctx->sqrt_lambda[0], bit_cnt);
        while (shift < ipd_rdo_cnt && cost < cand_cost[ipd_rdo_cnt - 1 - shift])
        {
            shift++;
        }
        if (shift != 0)
        {
            for (j = 1; j < shift; j++)
            {
                ipred_list[ipd_rdo_cnt - j] = ipred_list[ipd_rdo_cnt - 1 - j];
                cand_cost[ipd_rdo_cnt - j] = cand_cost[ipd_rdo_cnt - 1 - j];
                cand_satd_cost[ipd_rdo_cnt - j] = cand_satd_cost[ipd_rdo_cnt - 1 - j];
            }
            ipred_list[ipd_rdo_cnt - shift] = i;
            cand_cost[ipd_rdo_cnt - shift] = cost;
            cand_satd_cost[ipd_rdo_cnt - shift] = cost_satd;
        }
    }
    pred_cnt = ipd_rdo_cnt;
    for (i = ipd_rdo_cnt - 1; i >= 0; i--)
    {
        if (cand_satd_cost[i] > core->inter_satd * (1.1))
        {
            pred_cnt--;
        }
        else
        {
            break;
        }
    }
    return COM_MIN(pred_cnt, ipd_rdo_cnt);
}

double analyze_intra_cu(ENC_CTX *ctx, ENC_CORE *core)
{
    ENC_PINTRA *pi = &ctx->pintra;
    COM_MODE *bst_info = &core->mod_info_best;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int bit_depth = ctx->info.bit_depth_internal;
    int i, j, s_org, s_org_c, s_mod, s_mod_c;
    int best_ipd[MAX_NUM_TB] = { IPD_INVALID, IPD_INVALID, IPD_INVALID, IPD_INVALID };
    int best_ipd_pb_part[MAX_NUM_TB] = { IPD_INVALID, IPD_INVALID, IPD_INVALID, IPD_INVALID };
    int best_ipd_c = IPD_INVALID;
    s32 best_dist_y = 0, best_dist_c = 0;
    s32 best_dist_y_pb_part[MAX_NUM_TB] = { 0, 0, 0, 0 };
    u8  best_mpm_pb_part[MAX_NUM_TB][2];
    u8  best_mpm[MAX_NUM_TB][2];
    s16  coef_y_pb_part[MAX_CU_DIM];
    pel  rec_y_pb_part[MAX_CU_DIM];
    int  num_nz_y_pb_part[MAX_NUM_TB];
    int ipm_l2c = 0;
    int chk_bypass = 0;
    int bit_cnt = 0;
    int ipred_list[IPD_CNT];
    int pred_cnt = IPD_CNT;
    pel *org, *mod;
    pel *org_cb, *org_cr;
    pel *mod_cb, *mod_cr;
    double cost_temp, cost_best = MAX_COST;
    double cost_pb_temp, cost_pb_best;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;
    int cu_x = mod_info_curr->x_pos;
    int cu_y = mod_info_curr->y_pos;
    int ipd_buf[4] = { IPD_INVALID, IPD_INVALID, IPD_INVALID, IPD_INVALID };
    int ipd_add[4][3] = { { IPD_INVALID, IPD_INVALID, IPD_INVALID },{ IPD_INVALID, IPD_INVALID, IPD_INVALID },
        { IPD_INVALID, IPD_INVALID, IPD_INVALID },{ IPD_INVALID, IPD_INVALID, IPD_INVALID }
    };
#if DT_SAVE_LOAD
    ENC_BEF_DATA* pData = &core->bef_data[cu_width_log2 - 2][cu_height_log2 - 2][core->cup];
#endif
#if TB_SPLIT_EXT
    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.pb_part, &core->mod_info_curr.pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.tb_part, &core->mod_info_curr.tb_info);
#endif
    s_mod = pi->stride_rec[Y_C];
    s_org = pi->stride_org[Y_C];
    s_mod_c = pi->stride_rec[U_C];
    s_org_c = pi->stride_org[U_C];
    mod = pi->addr_rec_pic[Y_C] + (y * s_mod) + x;
    org = pi->addr_org[Y_C] + (y * s_org) + x;
    int part_size_idx, pb_part_idx;
    core->best_pb_part_intra = SIZE_2Nx2N;

    if (ctx->tree_status == TREE_LC || ctx->tree_status == TREE_L)
    {
        int allowed_part_size[7] = { SIZE_2Nx2N };
        int num_allowed_part_size = 1;
        int dt_allow = ctx->info.sqh.dt_intra_enable_flag ? com_dt_allow(mod_info_curr->cu_width, mod_info_curr->cu_height, MODE_INTRA, ctx->info.sqh.max_dt_size) : 0;
#if DT_INTRA_BOUNDARY_FILTER_OFF
        if (core->mod_info_curr.ipf_flag)
            dt_allow = 0;
#endif
        //prepare allowed PU partition
        if (dt_allow & 0x1) // horizontal
        {
            allowed_part_size[num_allowed_part_size++] = SIZE_2NxhN;
        }
        if (dt_allow & 0x2) // vertical
        {
            allowed_part_size[num_allowed_part_size++] = SIZE_hNx2N;
        }
        if (dt_allow & 0x1) // horizontal
        {
            allowed_part_size[num_allowed_part_size++] = SIZE_2NxnU;
            allowed_part_size[num_allowed_part_size++] = SIZE_2NxnD;
        }
        if (dt_allow & 0x2) // vertical
        {
            allowed_part_size[num_allowed_part_size++] = SIZE_nLx2N;
            allowed_part_size[num_allowed_part_size++] = SIZE_nRx2N;
        }

        cost_best = MAX_COST;
#if DT_INTRA_FAST_BY_RD
        u8 try_non_2NxhN = 1, try_non_hNx2N = 1;
        double cost_2Nx2N = MAX_COST, cost_hNx2N = MAX_COST, cost_2NxhN = MAX_COST;
#endif
        for (part_size_idx = 0; part_size_idx < num_allowed_part_size; part_size_idx++)
        {
            PART_SIZE pb_part_size = allowed_part_size[part_size_idx];
            PART_SIZE tb_part_size = get_tb_part_size_by_pb(pb_part_size, MODE_INTRA);
            set_pb_part(mod_info_curr, pb_part_size);
            set_tb_part(mod_info_curr, tb_part_size);
            get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_pos, mod_info_curr->y_pos, mod_info_curr->cu_width, mod_info_curr->cu_height, pb_part_size, &mod_info_curr->pb_info);
            get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_pos, mod_info_curr->y_pos, mod_info_curr->cu_width, mod_info_curr->cu_height, tb_part_size, &mod_info_curr->tb_info);
            assert(mod_info_curr->pb_info.sub_scup[0] == mod_info_curr->scup);
            cost_temp = 0;
            memset(num_nz_y_pb_part, 0, MAX_NUM_TB * sizeof(int));

            //DT fast algorithm here
#if DT_INTRA_FAST_BY_RD
            if (((pb_part_size == SIZE_2NxnU || pb_part_size == SIZE_2NxnD) && !try_non_2NxhN)
                    || ((pb_part_size == SIZE_nLx2N || pb_part_size == SIZE_nRx2N) && !try_non_hNx2N))
            {
                continue;
            }
#endif
#if DT_SAVE_LOAD
            if (pb_part_size != SIZE_2Nx2N && pData->num_intra_history > 1 && pData->best_part_size_intra[0] == SIZE_2Nx2N && pData->best_part_size_intra[1] == SIZE_2Nx2N)
                break;
#endif
            //end of DT fast algorithm

            // pb-based intra mode candidate selection
            for (pb_part_idx = 0; pb_part_idx < mod_info_curr->pb_info.num_sub_part; pb_part_idx++)
            {
                int pb_x = mod_info_curr->pb_info.sub_x[pb_part_idx];
                int pb_y = mod_info_curr->pb_info.sub_y[pb_part_idx];
                int pb_w = mod_info_curr->pb_info.sub_w[pb_part_idx];
                int pb_h = mod_info_curr->pb_info.sub_h[pb_part_idx];
                int pb_scup = mod_info_curr->pb_info.sub_scup[pb_part_idx];
                int pb_x_scu = PEL2SCU(pb_x);
                int pb_y_scu = PEL2SCU(pb_y);
                int pb_coef_offset = get_coef_offset_tb(mod_info_curr->x_pos, mod_info_curr->y_pos, pb_x, pb_y, cu_width, cu_height, tb_part_size);
                int tb_idx_offset = get_tb_idx_offset(pb_part_size, pb_part_idx);
                int num_tb_in_pb = get_part_num_tb_in_pb(pb_part_size, pb_part_idx);
                int skip_ipd = 0;
                if (((pb_part_size == SIZE_2NxnU || pb_part_size == SIZE_nLx2N) && pb_part_idx == 1) ||
                        ((pb_part_size == SIZE_2NxnD || pb_part_size == SIZE_nRx2N) && pb_part_idx == 0))
                {
                    skip_ipd = 1;
                }

                cost_pb_best = MAX_COST;
                cu_nz_cln(mod_info_curr->num_nz);

                mod = pi->addr_rec_pic[Y_C] + (pb_y * s_mod) + pb_x;
                org = pi->addr_org[Y_C] + (pb_y * s_org) + pb_x;

                u16 avail_cu = com_get_avail_intra(pb_x_scu, pb_y_scu, ctx->info.pic_width_in_scu, pb_scup, ctx->map.map_scu);
                com_get_nbr(pb_x, pb_y, pb_w, pb_h, mod, s_mod, avail_cu, core->nb, pb_scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, bit_depth, Y_C);
                com_get_mpm(pb_x_scu, pb_y_scu, ctx->map.map_scu, ctx->map.map_ipm, pb_scup, ctx->info.pic_width_in_scu, mod_info_curr->mpm[pb_part_idx]);

                pred_cnt = make_ipred_list(ctx, core, pb_w, pb_h, cu_width_log2, cu_height_log2, org, s_org, ipred_list, pb_part_idx, avail_cu
                                           , skip_ipd
                                          );
                if (skip_ipd == 1)
                {
                    if (pb_part_size == SIZE_2NxnU)
                    {
                        if (ipd_add[0][0] == 1)
                            ipred_list[pred_cnt++] = IPD_PLN;
                        if (ipd_add[0][1] == 1)
                            ipred_list[pred_cnt++] = IPD_BI;
                        if (ipd_add[0][2] == 1)
                            ipred_list[pred_cnt++] = IPD_DC;
                    }
                    else if (pb_part_size == SIZE_2NxnD)
                    {
                        if (ipd_add[1][0] == 1)
                            ipred_list[pred_cnt++] = IPD_PLN;
                        if (ipd_add[1][1] == 1)
                            ipred_list[pred_cnt++] = IPD_BI;
                        if (ipd_add[1][2] == 1)
                            ipred_list[pred_cnt++] = IPD_DC;
                    }
                    else if (pb_part_size == SIZE_nLx2N)
                    {
                        if (ipd_add[2][0] == 1)
                            ipred_list[pred_cnt++] = IPD_PLN;
                        if (ipd_add[2][1] == 1)
                            ipred_list[pred_cnt++] = IPD_BI;
                        if (ipd_add[2][2] == 1)
                            ipred_list[pred_cnt++] = IPD_DC;
                    }
                    else if (pb_part_size == SIZE_nRx2N)
                    {
                        if (ipd_add[3][0] == 1)
                            ipred_list[pred_cnt++] = IPD_PLN;
                        if (ipd_add[3][1] == 1)
                            ipred_list[pred_cnt++] = IPD_BI;
                        if (ipd_add[3][2] == 1)
                            ipred_list[pred_cnt++] = IPD_DC;
                    }
                }

                if (pred_cnt == 0)
                {
                    return MAX_COST;
                }
                for (j = 0; j < pred_cnt; j++) /* Y */
                {
                    s32 dist_t = 0;
                    i = ipred_list[j];
                    mod_info_curr->ipm[pb_part_idx][0] = (s8)i;
                    mod_info_curr->ipm[pb_part_idx][1] = IPD_INVALID;
                    cost_pb_temp = KERNEL(pintra_residue_rdo)(ctx, core, org, NULL, NULL, s_org, s_org_c, cu_width_log2, cu_height_log2, &dist_t, 0, pb_part_idx, x, y);
#if PRINT_CU_LEVEL_2
                    printf("\nluma pred mode %2d cost_pb_temp %10.1f", i, cost_pb_temp);
                    double val = 2815.9;
                    if (cost_pb_temp - val < 0.1 && cost_pb_temp - val > -0.1)
                    {
                        int a = 0;
                    }
#endif
                    if (cost_pb_temp < cost_pb_best)
                    {
                        cost_pb_best = cost_pb_temp;
                        best_dist_y_pb_part[pb_part_idx] = dist_t;
                        best_ipd_pb_part[pb_part_idx] = i;
                        best_mpm_pb_part[pb_part_idx][0] = mod_info_curr->mpm[pb_part_idx][0];
                        best_mpm_pb_part[pb_part_idx][1] = mod_info_curr->mpm[pb_part_idx][1];

                        com_mcpy(coef_y_pb_part + pb_coef_offset, mod_info_curr->coef[Y_C], pb_w * pb_h * sizeof(s16));
                        for (int j = 0; j < pb_h; j++)
                        {
                            int rec_offset = ((pb_y - cu_y) + j) * cu_width + (pb_x - cu_x);
                            com_mcpy(rec_y_pb_part + rec_offset, pi->rec[Y_C] + rec_offset, pb_w * sizeof(pel));
                        }

                        for (int tb_idx = 0; tb_idx < num_tb_in_pb; tb_idx++)
                        {
                            num_nz_y_pb_part[tb_idx + tb_idx_offset] = mod_info_curr->num_nz[tb_idx][Y_C];
                        }
                        SBAC_STORE(core->s_temp_prev_comp_best, core->s_temp_run);
                    }
                }
                cost_temp += cost_pb_best;
                if (pb_part_size == SIZE_2NxhN || pb_part_size == SIZE_hNx2N)
                {
                    ipd_buf[pb_part_idx] = best_ipd_pb_part[pb_part_idx];
                }

                //update map - pb
                update_intra_info_map_scu(ctx->map.map_scu, ctx->map.map_ipm, pb_x, pb_y, pb_w, pb_h, ctx->info.pic_width_in_scu, best_ipd_pb_part[pb_part_idx]);
                copy_rec_y_to_pic(rec_y_pb_part + (pb_y - cu_y) * cu_width + (pb_x - cu_x), pb_x, pb_y, pb_w, pb_h, cu_width, PIC_REC(ctx));
            }

            if (pb_part_size == SIZE_2NxhN || pb_part_size == SIZE_hNx2N)
            {
                int mem_offset = pb_part_size == SIZE_hNx2N ? 2 : 0;
                if (ipd_buf[1] == IPD_PLN || ipd_buf[2] == IPD_PLN || ipd_buf[3] == IPD_PLN)
                {
                    ipd_add[mem_offset + 0][0] = 1;
                }
                if (ipd_buf[1] == IPD_BI || ipd_buf[2] == IPD_BI || ipd_buf[3] == IPD_BI)
                {
                    ipd_add[mem_offset + 0][1] = 1;
                }
                if (ipd_buf[1] == IPD_DC || ipd_buf[2] == IPD_DC || ipd_buf[3] == IPD_DC)
                {
                    ipd_add[mem_offset + 0][2] = 1;
                }

                if (ipd_buf[0] == IPD_PLN || ipd_buf[1] == IPD_PLN || ipd_buf[2] == IPD_PLN)
                {
                    ipd_add[mem_offset + 1][0] = 1;
                }
                if (ipd_buf[0] == IPD_BI || ipd_buf[1] == IPD_BI || ipd_buf[2] == IPD_BI)
                {
                    ipd_add[mem_offset + 1][1] = 1;
                }
                if (ipd_buf[0] == IPD_DC || ipd_buf[1] == IPD_DC || ipd_buf[2] == IPD_DC)
                {
                    ipd_add[mem_offset + 1][2] = 1;
                }
            }

            com_mcpy(pi->rec[Y_C], rec_y_pb_part, cu_width * cu_height * sizeof(pel));
#if RDO_DBK
            if (mod_info_curr->pb_part != SIZE_2Nx2N)
            {
                int cbf_y = num_nz_y_pb_part[0] + num_nz_y_pb_part[1] + num_nz_y_pb_part[2] + num_nz_y_pb_part[3];
                calc_delta_dist_filter_boundary(ctx, core, PIC_REC(ctx), PIC_ORG(ctx), cu_width, cu_height, pi->rec, cu_width, x, y, 1, cbf_y, NULL, NULL, 0);
                cost_temp += ctx->delta_dist;
                best_dist_y_pb_part[PB0] += (s32)ctx->delta_dist; //add delta SSD to the first PB
            }
#endif
#if DT_INTRA_FAST_BY_RD
            if (pb_part_size == SIZE_2Nx2N)
                cost_2Nx2N = cost_temp;
            else if (pb_part_size == SIZE_2NxhN)
            {
                cost_2NxhN = cost_temp;
                assert(cost_2Nx2N != MAX_COST);
                if (cost_2NxhN > cost_2Nx2N * 1.05)
                    try_non_2NxhN = 0;
            }
            else if (pb_part_size == SIZE_hNx2N)
            {
                cost_hNx2N = cost_temp;
                assert(cost_2Nx2N != MAX_COST);
                if (cost_hNx2N > cost_2Nx2N * 1.05)
                    try_non_hNx2N = 0;
            }

            if (cost_hNx2N != MAX_COST && cost_2NxhN != MAX_COST)
            {
                if (cost_hNx2N > cost_2NxhN * 1.1)
                    try_non_hNx2N = 0;
                else if (cost_2NxhN > cost_hNx2N * 1.1)
                    try_non_2NxhN = 0;
            }
#endif
            //save luma cb decision for each pb_part_size
            if (cost_temp < cost_best)
            {
                cost_best = cost_temp;
                best_dist_y = 0;
                for (int pb_idx = 0; pb_idx < mod_info_curr->pb_info.num_sub_part; pb_idx++)
                {
                    best_dist_y += best_dist_y_pb_part[pb_idx];
                    best_ipd[pb_idx] = best_ipd_pb_part[pb_idx];
                    best_mpm[pb_idx][0] = best_mpm_pb_part[pb_idx][0];
                    best_mpm[pb_idx][1] = best_mpm_pb_part[pb_idx][1];
                }
                com_mcpy(bst_info->coef[Y_C], coef_y_pb_part, cu_width * cu_height * sizeof(s16));
                com_mcpy(bst_info->rec[Y_C], rec_y_pb_part, cu_width * cu_height * sizeof(pel));
                assert(mod_info_curr->pb_info.num_sub_part <= mod_info_curr->tb_info.num_sub_part);
                for (int tb_idx = 0; tb_idx < mod_info_curr->tb_info.num_sub_part; tb_idx++)
                {
                    bst_info->num_nz[tb_idx][Y_C] = num_nz_y_pb_part[tb_idx];
                }
#if TB_SPLIT_EXT
                core->best_pb_part_intra = mod_info_curr->pb_part;
                core->best_tb_part_intra = mod_info_curr->tb_part;
#endif
                SBAC_STORE(core->s_temp_pb_part_best, core->s_temp_prev_comp_best);
            }
        }
        pel *pBestSrc = bst_info->rec[Y_C];
        pel *pModDst = pi->addr_rec_pic[Y_C] + (y * s_mod) + x;
        for (int h = 0; h < cu_height; h++)
        {
            com_mcpy(pModDst, pBestSrc, cu_width * sizeof(pel));
            pModDst += s_mod;
            pBestSrc += cu_width;
        }
    }

    if (ctx->tree_status == TREE_LC || ctx->tree_status == TREE_C)
    {
        //chroma RDO
        SBAC_STORE(core->s_temp_prev_comp_best, core->s_temp_pb_part_best);
        org = pi->addr_org[Y_C] + (y * s_org) + x;
#if TSCPM
        mod = pi->addr_rec_pic[Y_C] + (y * s_mod) + x;
#endif
        mod_cb = pi->addr_rec_pic[U_C] + ((y >> 1) * s_mod_c) + (x >> 1);
        mod_cr = pi->addr_rec_pic[V_C] + ((y >> 1) * s_mod_c) + (x >> 1);
        org_cb = pi->addr_org[U_C] + ((y >> 1) * s_org_c) + (x >> 1);
        org_cr = pi->addr_org[V_C] + ((y >> 1) * s_org_c) + (x >> 1);

        u16 avail_cu = com_get_avail_intra(mod_info_curr->x_scu, mod_info_curr->y_scu, ctx->info.pic_width_in_scu, mod_info_curr->scup, ctx->map.map_scu);
#if TSCPM
        com_get_nbr(x,      y,      cu_width,      cu_height,      mod,    s_mod,   avail_cu, core->nb, mod_info_curr->scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, bit_depth, Y_C);
#endif
        com_get_nbr(x >> 1, y >> 1, cu_width >> 1, cu_height >> 1, mod_cb, s_mod_c, avail_cu, core->nb, mod_info_curr->scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, bit_depth, U_C);
        com_get_nbr(x >> 1, y >> 1, cu_width >> 1, cu_height >> 1, mod_cr, s_mod_c, avail_cu, core->nb, mod_info_curr->scup, ctx->map.map_scu, ctx->info.pic_width_in_scu, ctx->info.pic_height_in_scu, bit_depth, V_C);

        cost_best = MAX_COST;

#if CHROMA_NOT_SPLIT
        //get luma pred mode
        if (ctx->tree_status == TREE_C)
        {
            assert(cu_width >= 8 && cu_height >= 8);
            int luma_scup = PEL2SCU(x + (cu_width - 1)) + PEL2SCU(y + (cu_height - 1)) * ctx->info.pic_width_in_scu;
            best_ipd[PB0] = ctx->map.map_ipm[luma_scup];
#if IPCM
            assert((best_ipd[PB0] >= 0 && best_ipd[PB0] < IPD_CNT) || best_ipd[PB0] == IPD_IPCM);
#else
            assert(best_ipd[PB0] >= 0 && best_ipd[PB0] < IPD_CNT);
#endif
            assert(MCU_GET_INTRA_FLAG(ctx->map.map_scu[luma_scup]));
        }
#endif
        ipm_l2c = best_ipd[PB0];
        mod_info_curr->ipm[PB0][0] = (s8)best_ipd[PB0];
        COM_IPRED_CONV_L2C_CHK(ipm_l2c, chk_bypass);
#if TSCPM
        for (i = 0; i < IPD_CHROMA_CNT + 1; i++) /* UV */
#else
        for (i = 0; i < IPD_CHROMA_CNT; i++) /* UV */
#endif
        {
            s32 dist_t = 0;
            mod_info_curr->ipm[PB0][1] = (s8)i;
            if (i != IPD_DM_C && chk_bypass && i == ipm_l2c)
            {
                continue;
            }
#if IPCM
            if (i == IPD_DM_C && best_ipd[PB0] == IPD_IPCM)
            {
                continue;
            }
#endif
#if TSCPM
            if (!ctx->info.sqh.tscpm_enable_flag && i == IPD_TSCPM_C)
            {
                continue;
            }
#endif

            cost_temp = KERNEL(pintra_residue_rdo)(ctx, core, org, org_cb, org_cr, s_org, s_org_c, cu_width_log2, cu_height_log2, &dist_t, 1, 0, x, y);
#if PRINT_CU_LEVEL_2
            printf("\nchro pred mode %2d cost_temp %10.1f", i, cost_temp);
            double val = 2815.9;
            if (cost_temp - val < 0.1 && cost_temp - val > -0.1)
            {
                int a = 0;
            }
#endif
            if (cost_temp < cost_best)
            {
                cost_best = cost_temp;
                best_dist_c = dist_t;
                best_ipd_c = i;
                for (j = U_C; j < N_C; j++)
                {
                    int size_tmp = (cu_width * cu_height) >> (j == 0 ? 0 : 2);
                    com_mcpy(bst_info->coef[j], mod_info_curr->coef[j], size_tmp * sizeof(s16));
                    com_mcpy(bst_info->rec[j], pi->rec[j], size_tmp * sizeof(pel));
                    bst_info->num_nz[TB0][j] = mod_info_curr->num_nz[TB0][j];
                }
            }
        }
    }

    int pb_part_size_best = core->best_pb_part_intra;
    int tb_part_size_best = get_tb_part_size_by_pb(pb_part_size_best, MODE_INTRA);
    int num_pb_best = get_part_num(pb_part_size_best);
    int num_tb_best = get_part_num(tb_part_size_best);
    //some check
    if (ctx->tree_status == TREE_LC)
    {
        assert(best_ipd_c != IPD_INVALID);
    }
    else if (ctx->tree_status == TREE_L)
    {
        assert(bst_info->num_nz[TBUV0][U_C] == 0);
        assert(bst_info->num_nz[TBUV0][V_C] == 0);
        assert(best_dist_c == 0);
    }
    else if (ctx->tree_status == TREE_C)
    {
        assert(best_ipd_c != IPD_INVALID);
        assert(bst_info->num_nz[TBUV0][Y_C] == 0);
        assert(num_pb_best == 1 && num_tb_best == 1);
        assert(best_dist_y == 0);
    }
    else
        assert(0);
    //end of checks

    for (pb_part_idx = 0; pb_part_idx < num_pb_best; pb_part_idx++)
    {
        core->mod_info_best.mpm[pb_part_idx][0] = best_mpm[pb_part_idx][0];
        core->mod_info_best.mpm[pb_part_idx][1] = best_mpm[pb_part_idx][1];
        core->mod_info_best.ipm[pb_part_idx][0] = (s8)best_ipd[pb_part_idx];
        core->mod_info_best.ipm[pb_part_idx][1] = (s8)best_ipd_c;
        mod_info_curr->ipm[pb_part_idx][0] = core->mod_info_best.ipm[pb_part_idx][0];
        mod_info_curr->ipm[pb_part_idx][1] = core->mod_info_best.ipm[pb_part_idx][1];
    }
    for (int tb_part_idx = 0; tb_part_idx < num_tb_best; tb_part_idx++)
    {
        for (j = 0; j < N_C; j++)
        {
            mod_info_curr->num_nz[tb_part_idx][j] = bst_info->num_nz[tb_part_idx][j];
        }
    }
#if TB_SPLIT_EXT
    mod_info_curr->pb_part = core->best_pb_part_intra;
    mod_info_curr->tb_part = core->best_tb_part_intra;
#endif
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.pb_part, &core->mod_info_curr.pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.tb_part, &core->mod_info_curr.tb_info);

    SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
    //enc_sbac_bit_reset(&core->s_temp_run);
    bit_cnt = enc_get_bit_number(&core->s_temp_run);
    enc_bit_est_intra(ctx, core, ctx->info.pic_header.slice_type, bst_info->coef);
    bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
    cost_best = (best_dist_y + best_dist_c) + RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
    SBAC_STORE(core->s_temp_best, core->s_temp_run);
    core->dist_cu = best_dist_y + best_dist_c;
#if DT_SAVE_LOAD
    if (pData->num_intra_history < 2 && core->mod_info_curr.ipf_flag == 0 && ctx->tree_status != TREE_C)
    {
        pData->best_part_size_intra[pData->num_intra_history++] = core->best_pb_part_intra;
    }
#endif
    return cost_best;
}

#if IPCM
double analyze_ipcm_cu(ENC_CTX *ctx, ENC_CORE *core)
{
    ENC_PINTRA *pi = &ctx->pintra;
    COM_MODE *bst_info = &core->mod_info_best;
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    int i, j, s_org, s_org_c;

    int best_ipd[MAX_NUM_TB] = { IPD_INVALID, IPD_INVALID, IPD_INVALID, IPD_INVALID };
    int best_ipd_c = IPD_INVALID;
    s32 best_dist_y = 0, best_dist_c = 0;
    int bit_cnt = 0;
    pel *org;
    pel *org_cb, *org_cr;
    double cost_best = MAX_COST;
    int x = mod_info_curr->x_pos;
    int y = mod_info_curr->y_pos;
    int cu_width_log2 = mod_info_curr->cu_width_log2;
    int cu_height_log2 = mod_info_curr->cu_height_log2;
    int cu_width = 1 << cu_width_log2;
    int cu_height = 1 << cu_height_log2;

#if TB_SPLIT_EXT
    init_pb_part(&core->mod_info_curr);
    init_tb_part(&core->mod_info_curr);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.pb_part, &core->mod_info_curr.pb_info);
    get_part_info(ctx->info.pic_width_in_scu, mod_info_curr->x_scu << 2, mod_info_curr->y_scu << 2, cu_width, cu_height, core->mod_info_curr.tb_part, &core->mod_info_curr.tb_info);
#endif

    if (ctx->tree_status == TREE_LC || ctx->tree_status == TREE_L)
    {
        s_org = pi->stride_org[Y_C];
        org = pi->addr_org[Y_C] + (y * s_org) + x;
        int pb_part_idx = 0;
        core->best_pb_part_intra = SIZE_2Nx2N;

        for (i = 0; i < cu_height; i++)
        {
            for (j = 0; j < cu_width; j++)
            {
                bst_info->rec[Y_C][i * cu_width + j] = org[i * s_org + j];
                bst_info->coef[Y_C][i * cu_width + j] = org[i * s_org + j] >> (ctx->info.bit_depth_internal - ctx->info.bit_depth_input);
            }
        }

        int pb_x = mod_info_curr->pb_info.sub_x[pb_part_idx];
        int pb_y = mod_info_curr->pb_info.sub_y[pb_part_idx];
        int pb_w = mod_info_curr->pb_info.sub_w[pb_part_idx];
        int pb_h = mod_info_curr->pb_info.sub_h[pb_part_idx];
        int pb_scup = mod_info_curr->pb_info.sub_scup[pb_part_idx];
        int pb_x_scu = PEL2SCU(pb_x);
        int pb_y_scu = PEL2SCU(pb_y);

        com_get_mpm(pb_x_scu, pb_y_scu, ctx->map.map_scu, ctx->map.map_ipm, pb_scup, ctx->info.pic_width_in_scu, mod_info_curr->mpm[pb_part_idx]);
        update_intra_info_map_scu(ctx->map.map_scu, ctx->map.map_ipm, pb_x, pb_y, pb_w, pb_h, ctx->info.pic_width_in_scu, IPD_IPCM);

        best_ipd[PB0] = IPD_IPCM;

        mod_info_curr->num_nz[0][0] = bst_info->num_nz[0][0] = 0;
        copy_rec_y_to_pic(bst_info->rec[Y_C] + (pb_y - y) * cu_width + (pb_x - x), pb_x, pb_y, pb_w, pb_h, cu_width, PIC_REC(ctx));
        com_mcpy(pi->rec[Y_C], bst_info->rec[Y_C], cu_width * cu_height * sizeof(pel));
#if RDO_DBK
        calc_delta_dist_filter_boundary(ctx, core, PIC_REC(ctx), PIC_ORG(ctx), cu_width, cu_height, pi->rec, cu_width, x, y, 1, 0, NULL, NULL, 0);
        best_dist_y += (s32)ctx->delta_dist; //add delta SSD to the first PB
#endif
    }

    if (ctx->tree_status == TREE_LC || ctx->tree_status == TREE_C)
    {
        s_org_c = pi->stride_org[U_C];
        org_cb = pi->addr_org[U_C] + ((y >> 1) * s_org_c) + (x >> 1);
        org_cr = pi->addr_org[V_C] + ((y >> 1) * s_org_c) + (x >> 1);
        core->best_pb_part_intra = SIZE_2Nx2N;

#if CHROMA_NOT_SPLIT
        //get luma pred mode
        if (ctx->tree_status == TREE_C)
        {
            assert(cu_width >= 8 && cu_height >= 8);
            int luma_scup = PEL2SCU(x + (cu_width - 1)) + PEL2SCU(y + (cu_height - 1)) * ctx->info.pic_width_in_scu;
            best_ipd[PB0] = ctx->map.map_ipm[luma_scup];
            assert((best_ipd[PB0] >= 0 && best_ipd[PB0] < IPD_CNT) || best_ipd[PB0] == IPD_IPCM);
            assert(MCU_GET_INTRA_FLAG(ctx->map.map_scu[luma_scup]));
        }
#endif
        if (best_ipd[PB0] == IPD_IPCM)
        {
            best_ipd_c = IPD_DM_C;

            for (i = 0; i < cu_height / 2; i++)
            {
                for (j = 0; j < cu_width / 2; j++)
                {
                    bst_info->rec[U_C][i * cu_width / 2 + j] = org_cb[i * s_org_c + j];
                    bst_info->coef[U_C][i * cu_width / 2 + j] = org_cb[i * s_org_c + j] >> (ctx->info.bit_depth_internal - ctx->info.bit_depth_input);
                    bst_info->rec[V_C][i * cu_width / 2 + j] = org_cr[i * s_org_c + j];
                    bst_info->coef[V_C][i * cu_width / 2 + j] = org_cr[i * s_org_c + j] >> (ctx->info.bit_depth_internal - ctx->info.bit_depth_input);
                }
            }
        }
        else
        {
            return MAX_COST;
        }

        for (j = 1; j < N_C; j++)
        {
            mod_info_curr->num_nz[0][j] = bst_info->num_nz[0][j] = 0;
        }
    }

    int pb_part_idx = 0;

    core->mod_info_best.mpm[pb_part_idx][0] = mod_info_curr->mpm[pb_part_idx][0];
    core->mod_info_best.mpm[pb_part_idx][1] = mod_info_curr->mpm[pb_part_idx][1];
    core->mod_info_best.ipm[pb_part_idx][0] = best_ipd[PB0];
    core->mod_info_best.ipm[pb_part_idx][1] = best_ipd_c;
    mod_info_curr->ipm[pb_part_idx][0] = core->mod_info_best.ipm[pb_part_idx][0];
    mod_info_curr->ipm[pb_part_idx][1] = core->mod_info_best.ipm[pb_part_idx][1];

    SBAC_LOAD(core->s_temp_run, core->s_curr_best[cu_width_log2 - 2][cu_height_log2 - 2]);
    bit_cnt = enc_get_bit_number(&core->s_temp_run);
    enc_bit_est_intra(ctx, core, ctx->info.pic_header.slice_type, bst_info->coef);
    bit_cnt = enc_get_bit_number(&core->s_temp_run) - bit_cnt;
    cost_best = (best_dist_y + best_dist_c) + RATE_TO_COST_LAMBDA(ctx->lambda[0], bit_cnt);
    SBAC_STORE(core->s_temp_best, core->s_temp_run);
    core->dist_cu = best_dist_y + best_dist_c;

    return cost_best;
}
#endif

int pintra_set_complexity(ENC_CTX * ctx, int complexity)
{
    ENC_PINTRA * pi;
    pi = &ctx->pintra;
    pi->complexity = complexity;
    return COM_OK;
}

