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

#ifndef _DEC_ECO_H_
#define _DEC_ECO_H_

#include "dec_def.h"

#define CHECK_ALL_CTX 0

#define GET_SBAC_DEC(bs)   ((DEC_SBAC *)((bs)->pdata[1]))
#define SET_SBAC_DEC(bs, sbac) ((bs)->pdata[1] = (sbac))

u32  dec_sbac_decode_bin(COM_BSR *bs, DEC_SBAC *sbac, SBAC_CTX_MODEL *model);
int  dec_sbac_decode_bin_trm(COM_BSR *bs, DEC_SBAC *sbac);

int  dec_eco_cnkh(COM_BSR * bs, COM_CNKH * cnkh);
int  dec_eco_sqh(COM_BSR * bs, COM_SQH * sqh);

int  dec_eco_pic_header(COM_BSR * bs, COM_PIC_HEADER * pic_header, COM_SQH * sqh, int* need_minus_256);
int  dec_eco_patch_header(COM_BSR * bs, COM_SQH *sqh, COM_PIC_HEADER * ph, COM_SH_EXT * pic_header, PATCH_INFO *patch);
#if PATCH
int  dec_eco_send(COM_BSR * bs);
#endif
#if EXTENSION_USER_DATA
int  extension_and_user_data(DEC_CTX * ctx, COM_BSR * bs, int i, COM_SQH * sqh, COM_PIC_HEADER* pic_header);
#endif
int  dec_eco_udata(DEC_CTX * ctx, COM_BSR * bs);
void dec_sbac_init(COM_BSR * bs);

int  decode_inter_dir(COM_BSR * bs, DEC_SBAC * sbac, int part_size, DEC_CTX * ctx);
int  decode_intra_dir(COM_BSR * bs, DEC_SBAC * sbac, u8 mpm[2]);
int  decode_intra_dir_c(COM_BSR * bs, DEC_SBAC * sbac, u8 ipm_l
#if TSCPM
                        , u8 tscpm_enable_flag
#endif
                       );
#if IPCM
void decode_ipcm(COM_BSR * bs, DEC_SBAC * sbac, int tb_width, int tb_height, int cu_width, s16 pcm[MAX_CU_DIM], int bit_depth, int ch_type);
#endif

int  dec_eco_lcu_delta_qp(COM_BSR * bs, DEC_SBAC * sbac, int last_dqp);

int  decode_coef(DEC_CTX * ctx, DEC_CORE * core);

int  dec_decode_cu(DEC_CTX * ctx, DEC_CORE * core);
#if CHROMA_NOT_SPLIT
int  dec_decode_cu_chroma(DEC_CTX * ctx, DEC_CORE * core);
#endif

s8   dec_eco_split_mode(DEC_CTX * ctx, COM_BSR *bs, DEC_SBAC *sbac, int cu_width, int cu_height
                        , const int parent_split, int qt_depth, int bet_depth, int x, int y);

#if MODE_CONS
u8   dec_eco_cons_pred_mode_child(COM_BSR * bs, DEC_SBAC * sbac);
#endif

void dec_eco_affine_motion_derive(DEC_CTX *ctx, DEC_CORE *core, COM_BSR *bs, DEC_SBAC *sbac, int inter_dir, int cu_width, int cu_height, CPMV affine_mvp[VER_NUM][MV_D], s16 mvd[MV_D]);

int  dec_eco_DB_param(COM_BSR *bs, COM_PIC_HEADER *pic_header);

extern int EO_OFFSET_INV__MAP[];
int  dec_eco_sao_mergeflag(DEC_SBAC *sbac, COM_BSR *bs, int mergeleft_avail, int mergeup_avail);

int  dec_eco_sao_mode(DEC_SBAC *sbac, COM_BSR *bs);

int  dec_eco_sao_offset(DEC_SBAC *sbac, COM_BSR *bs, SAOBlkParam *saoBlkParam, int *offset);

int  dec_eco_sao_EO_type(DEC_SBAC *sbac, COM_BSR *bs, SAOBlkParam *saoBlkParam);

void dec_eco_sao_BO_start(DEC_SBAC *sbac, COM_BSR *bs, SAOBlkParam *saoBlkParam, int stBnd[2]);

int  dec_eco_ALF_param(COM_BSR *bs, COM_PIC_HEADER *pic_header);

int  dec_eco_AlfCoeff(COM_BSR * bs, ALFParam *Alfp);

u32  dec_eco_AlfLCUCtrl(COM_BSR * bs, DEC_SBAC * sbac, int compIdx, int ctx_idx);

#endif /* _DEC_ECO_H_ */
