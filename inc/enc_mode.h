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

#ifndef _ENC_MODE_H_
#define _ENC_MODE_H_

#ifdef __cplusplus
extern "C"
{
#endif

int enc_mode_init_frame(ENC_CTX *ctx);

int enc_mode_init_lcu(ENC_CTX *ctx, ENC_CORE *core);

int enc_mode_analyze_lcu(ENC_CTX *ctx, ENC_CORE *core);
#if FS_ALL_COMBINATION
void derive_split_combination_CTU(ENC_CTX *ctx, ENC_CORE *core);
#endif

void enc_bit_est_intra(ENC_CTX * ctx, ENC_CORE * core, s32 slice_type, s16 coef[N_C][MAX_CU_DIM]);
void enc_bit_est_pb_intra_luma(ENC_CTX * ctx, ENC_CORE * core, s32 slice_type, s16 coef[N_C][MAX_CU_DIM], int pb_part_idx);
void enc_bit_est_tb_intra_luma(ENC_CTX *ctx, ENC_CORE *core, s32 slice_type, int tb_idx, int cbf_y);

void enc_bit_est_intra_chroma(ENC_CTX * ctx, ENC_CORE * core, s16 coef[N_C][MAX_CU_DIM]);

void enc_bit_est_inter(ENC_CTX * ctx, ENC_CORE * core, s32 slice_type);

void enc_bit_est_inter_comp(ENC_CTX *ctx, ENC_CORE * core, s16 coef[MAX_CU_DIM], int ch_type);

void enc_bit_est_affine_mvp(ENC_CTX * ctx, ENC_CORE * core, s32 slice_type, s8 refi[REFP_NUM], s16 mvd[REFP_NUM][VER_NUM][MV_D], int vertex_num);

//void enc_sbac_bit_reset(ENC_SBAC * sbac);
u32 enc_get_bit_number(ENC_SBAC *sbac);
void enc_init_bits_est();
void enc_init_bef_data(ENC_CORE* core, ENC_CTX* ctx);

#if RDO_DBK
void calc_delta_dist_filter_boundary(ENC_CTX* ctx, ENC_CORE *core, COM_PIC *pic_rec, COM_PIC *pic_org, int cu_width, int cu_height, pel(*src)[MAX_CU_DIM],
                                     int s_src, int x, int y, u8 intra_flag, u8 cu_cbf, s8 *refi, s16(*mv)[MV_D], u8 is_mv_from_mvf);
#endif

double SAORDCost_for_mode_merge(ENC_CTX *ctx, ENC_CORE *core, COM_BSW *sao_bs_temp,int lcu_pos, int pix_y, int pix_x,
                                double* sao_labmda, SAOStatData **saostatData, SAOBlkParam **rec_saoBlkParam,
                                SAOBlkParam *sao_cur_param, SAOBlkParam *rec_sao_cur_param, int *MergeLeftAvail, int *MergeUpAvail);
double SAORDCost_for_mode_new_YUV_sep(ENC_CTX *ctx, ENC_CORE *core, COM_BSW *sao_bs_temp, int MergeLeftAvail, int MergeUpAvail, double* sao_lambda, SAOStatData **saostatData, SAOBlkParam *sao_cur_param, SAOBlkParam *rec_sao_cur_param);
void getParaSAO_one_LCU(ENC_CTX *ctx, ENC_CORE *core, COM_BSW *sao_bs_temp, int lcu_pos, int mb_y, int mb_x,
                        SAOStatData **saostatData, SAOBlkParam *saoBlkParam, SAOBlkParam **rec_saoBlkParam, double* sao_labmda);

void getStatblk(COM_PIC  *pic_org, COM_PIC  *pic_sao, SAOStatData *saostatsData, int bit_depth, int compIdx, int pix_y, int pix_x, int smb_pix_height,
                int smb_pix_width, int smb_available_left, int smb_available_right, int smb_available_up, int smb_available_down,
                int smb_available_upleft, int smb_available_upright, int smb_available_leftdown, int smb_available_rightdwon);

void enc_SAO_rdo(ENC_CTX *ctx, ENC_CORE *core, COM_BSW *sao_bs);

void getStatisticsSAO_one_LCU(COM_INFO *info, COM_MAP *map, COM_PIC  *pic_org, COM_PIC  *pic_sao, int mb_y, int mb_x, int smb_mb_height, int smb_mb_width,
                              SAOStatData **saostatData);
void find_offset(int compIdx, int typeIdc, SAOStatData **saostatData, SAOBlkParam *saoBlkParam, double lambda);
int offset_estimation(int typeIdx, int classIdx, double lambda, int offset_ori, int count, long long int diff, double *bestCost);
void writeParaSAO_one_LCU(ENC_CTX *ctx, int y_pel, int x_pel,
                          SAOBlkParam *saoBlkParam);

void write_sao_lcu(ENC_CTX *ctx, int lcu_pos, int pix_y, int pix_x, SAOBlkParam *sao_cur_param);

int enc_sao_avs2(ENC_CTX * ctx, COM_PIC * pic);

int rdoq_est_ctp_zero_flag[2];
int rdoq_est_cbf[NUM_QT_CBF_CTX][2];

s32 rdoq_est_run[NUM_SBAC_CTX_RUN][2];
s32 rdoq_est_level[NUM_SBAC_CTX_LEVEL][2];
s32 rdoq_est_last[2][NUM_SBAC_CTX_LAST1][NUM_SBAC_CTX_LAST2][2];

#ifdef __cplusplus
}
#endif

#endif /* _ENC_MODE_H_ */
