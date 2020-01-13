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

#ifndef _COM_TBL_H_
#define _COM_TBL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "com_def.h"

#if TSCPM
extern int g_aiTscpmDivTable64[64];
#endif
extern const s8 com_tbl_log2[257];
#if FIXED_SPLIT
extern const int tbl_target_size_list[27][2];
#endif

extern s8 com_tbl_tm2[NUM_TRANS_TYPE][2][2];
extern s8 com_tbl_tm4[NUM_TRANS_TYPE][4][4];
extern s8 com_tbl_tm8[NUM_TRANS_TYPE][8][8];
extern s8 com_tbl_tm16[NUM_TRANS_TYPE][16][16];
extern s8 com_tbl_tm32[NUM_TRANS_TYPE][32][32];
extern s8 com_tbl_tm64[NUM_TRANS_TYPE][64][64];

extern const s16 tab_c4_trans[4][4];
extern const s16 tab_c8_trans[4][4];

extern int com_tbl_subset_inter[2];

extern int com_scan_sr[MAX_TR_SIZE*MAX_TR_SIZE];
extern u16 *com_scan_tbl[COEF_SCAN_TYPE_NUM][MAX_CU_LOG2][MAX_CU_LOG2];
extern const int com_tbl_dq_scale[80];
extern const int com_tbl_dq_shift[80];

extern const s16 tbl_mc_l_coeff_hp[16][8];
extern const s16 tbl_mc_c_coeff_hp[32][4];
extern const s16 tbl_mc_l_coeff[4][8];
extern const s16 tbl_mc_c_coeff[8][4];

extern const u8 com_tbl_df_st[4][52];

extern const int com_tbl_ipred_dxdy[IPD_CNT][2];
extern const s16 com_tbl_ipred_adi[32][4];

extern const int com_tbl_qp_chroma_adjust[64];
extern const int com_tbl_qp_chroma_adjust_enc[64];

extern const u16 tab_cycno_lgpmps_mps[1 << 14];

extern int saoclip[NUM_SAO_OFFSET][3];
extern int EO_OFFSET_MAP[8];
extern int deltaband_cost[17];

extern void init_dct_coef();


extern const int tab_wq_param_default[2][6];
extern const u8 tab_WqMDefault4x4[16];
extern const u8 tab_WqMDefault8x8[64];

void set_pic_wq_matrix_by_param(int *param_vector, int mode, u8 *pic_wq_matrix4x4, u8 *pic_wq_matrix8x8);
void init_pic_wq_matrix(u8 *pic_wq_matrix4x4, u8 *pic_wq_matrix8x8);

#ifdef __cplusplus
}
#endif

#endif /* _COM_TBL_H_ */
