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

#ifndef _ENC_ECO_H_
#define _ENC_ECO_H_

#ifdef __cplusplus
extern "C"
{
#endif
#include "enc_def.h"

#define GET_SBAC_ENC(bs)   ((ENC_SBAC *)(bs)->pdata[1])

int enc_eco_cnkh(COM_BSW * bs, COM_CNKH * cnkh);
int enc_eco_sqh(ENC_CTX *ctx, COM_BSW * bs, COM_SQH * sqh);
int enc_eco_pic_header(COM_BSW * bs, COM_PIC_HEADER * sh, COM_SQH * sqh);
int enc_eco_patch_header(COM_BSW * bs, COM_SQH *sqh, COM_PIC_HEADER *ph, COM_SH_EXT * sh, u8 patch_idx, PATCH_INFO* patch);
#if PATCH
int enc_eco_send(COM_BSW * bs);
#endif
int enc_eco_udata(ENC_CTX * ctx, COM_BSW * bs);

int encode_pred_mode(COM_BSW * bs, u8 pred_mode, ENC_CTX * ctx);
int encode_ipf_flag(COM_BSW *bs, u8 ipf_flag);

int encode_mvd(COM_BSW * bs, s16 mvd[MV_D]);
void enc_sbac_init(COM_BSW * bs);

void enc_sbac_finish(COM_BSW *bs, int is_ipcm);
void enc_sbac_encode_bin(u32 bin, ENC_SBAC *sbac, SBAC_CTX_MODEL *ctx_model, COM_BSW *bs);
void enc_sbac_encode_bin_trm(u32 bin, ENC_SBAC *sbac, COM_BSW *bs);
int encode_coef(COM_BSW * bs, s16 coef[N_C][MAX_CU_DIM], int cu_width_log2, int cu_height_log2, u8 pred_mode, COM_MODE *mi, u8 tree_status, ENC_CTX * ctx);

int enc_eco_unit(ENC_CTX * ctx, ENC_CORE * core, int x, int y, int cup, int cu_width, int cu_height);
#if CHROMA_NOT_SPLIT
int enc_eco_unit_chroma(ENC_CTX * ctx, ENC_CORE * core, int x, int y, int cup, int cu_width, int cu_height);
#endif
int enc_eco_split_mode(COM_BSW *bs, ENC_CTX *c, ENC_CORE *core, int cud, int cup, int cu_width, int cu_height, int lcu_s
                       , const int parent_split, int qt_depth, int bet_depth, int x, int y);
#if MODE_CONS
void enc_eco_cons_pred_mode_child(COM_BSW * bs, u8 cons_pred_mode_child);
#endif
int enc_extension_and_user_data(ENC_CTX* ctx, COM_BSW * bs, int i, u8 isExtension, COM_SQH* sqh, COM_PIC_HEADER* pic_header);

void enc_eco_lcu_delta_qp(COM_BSW *bs, int val, int last_dqp);
void enc_eco_slice_end_flag(COM_BSW * bs, int flag);
int encode_mvd(COM_BSW *bs, s16 mvd[MV_D]);
int encode_refidx(COM_BSW * bs, int num_refp, int refi);
void encode_inter_dir(COM_BSW * bs, s8 refi[REFP_NUM], int part_size, ENC_CTX * ctx);
void encode_skip_flag(COM_BSW * bs, ENC_SBAC *sbac, int flag, ENC_CTX * ctx);

void encode_umve_flag(COM_BSW *bs, int flag);
void encode_umve_idx(COM_BSW *bs, int umve_idx);

void enc_eco_split_flag(ENC_CTX *c, int cu_width, int cu_height, int x, int y, COM_BSW * bs, ENC_SBAC *sbac, int flag);
void encode_skip_idx(COM_BSW *bs, int skip_idx, int num_hmvp_cands, ENC_CTX * ctx);
void encode_direct_flag(COM_BSW *bs, int t_direct_flag, ENC_CTX * ctx);
//! \todo Change list of arguments
void enc_eco_xcoef(COM_BSW *bs, s16 *coef, int log2_w, int log2_h, int num_sig, int ch_type);
//! \todo Change list of arguments
int encode_intra_dir(COM_BSW *bs, u8 ipm, u8 mpm[2]);
#if TSCPM
int encode_intra_dir_c(COM_BSW *bs, u8 ipm, u8 ipm_l, u8 tscpm_enable_flag);
#else
int encode_intra_dir_c(COM_BSW *bs, u8 ipm, u8 ipm_l);
#endif
#if IPCM
void encode_ipcm(ENC_SBAC *sbac, COM_BSW *bs, s16 pcm[MAX_CU_DIM], int tb_width, int tb_height, int cu_width, int bit_depth, int ch_type);
#endif
int encode_mvr_idx(COM_BSW *bs, u8 mvr_idx, BOOL is_affine_mode);

#if EXT_AMVR_HMVP
void encode_extend_amvr_flag(COM_BSW *bs, u8 mvp_from_hmvp_flag);
#endif

void encode_affine_flag(COM_BSW * bs, int flag, ENC_CTX * ctx);
void encode_affine_mrg_idx( COM_BSW * bs, s16 affine_mrg_idx, ENC_CTX * ctx);

#if SMVD
void encode_smvd_flag( COM_BSW * bs, int flag );
#endif

#if DT_SYNTAX
void encode_part_size(ENC_CTX *ctx, COM_BSW * bs, int part_size, int cu_w, int cu_h, int pred_mode);
#endif

int enc_eco_DB_param(COM_BSW * bs, COM_PIC_HEADER *sh);

void  enc_eco_sao_mrg_flag(ENC_SBAC *sbac, COM_BSW *bs, int mergeleft_avail, int mergeup_avail, SAOBlkParam *saoBlkParam);
void  enc_eco_sao_mode(ENC_SBAC *sbac, COM_BSW *bs, SAOBlkParam *saoBlkParam);
void  enc_eco_sao_offset(ENC_SBAC *sbac, COM_BSW *bs, SAOBlkParam *saoBlkParam);
void  enc_eco_sao_type(ENC_SBAC *sbac, COM_BSW *bs, SAOBlkParam *saoBlkParam);

int enc_eco_ALF_param(COM_BSW * bs, COM_PIC_HEADER *sh);
void enc_eco_AlfLCUCtrl(ENC_SBAC *sbac, COM_BSW *bs, int iflag);
void enc_eco_AlfCoeff(COM_BSW *bs, ALFParam *Alfp);

void Demulate(COM_BSW * bs);

#ifdef __cplusplus
}
#endif
#endif /* _ENC_ECO_H_ */
