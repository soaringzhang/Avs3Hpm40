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

#ifndef _DEC_DEF_H_
#define _DEC_DEF_H_

#include "com_def.h"
#include "dec_bsr.h"

/* decoder magic code */
#define DEC_MAGIC_CODE          0x45565944 /* EVYD */

/*****************************************************************************
* ALF structure
*****************************************************************************/
typedef struct DecALFVar
{
    int **m_filterCoeffSym;
    int m_varIndTab[NO_VAR_BINS];
    pel **m_varImg;
    BOOL **m_AlfLCUEnabled;
    ALFParam **m_alfPictureParam;
} DecALFVar;

/*****************************************************************************
 * SBAC structure
 *****************************************************************************/
typedef struct _DEC_SBAC
{
    u32            range;
    u32            value;
    int            bits_Needed;
    u32            prev_bytes;
    COM_SBAC_CTX  ctx;
} DEC_SBAC;

/*****************************************************************************
 * CORE information used for decoding process.
 *
 * The variables in this structure are very often used in decoding process.
 *****************************************************************************/
typedef struct _DEC_CORE
{
    COM_MODE       mod_info_curr;
    /* neighbor pixel buffer for intra prediction */
    pel            nb[N_C][N_REF][MAX_CU_SIZE * 3];

    


    /* QP for Luma of current encoding MB */
    int             qp_y;
    /* QP for Chroma of current encoding MB */
    int             qp_u;
    int             qp_v;

    /************** current LCU *************/
    /* address of current LCU,  */
    int            lcu_num;
    /* X address of current LCU */
    int            x_lcu;
    /* Y address of current LCU */
    int            y_lcu;
    /* left pel position of current LCU */
    int            x_pel;
    /* top pel position of current LCU */
    int            y_pel;
    /* split mode map for current LCU */

    s8             split_mode[MAX_CU_DEPTH][NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU];
    /* platform specific data, if needed */
    void          *pf;

    COM_MOTION     motion_cands[ALLOWED_HMVP_NUM];
    s8             cnt_hmvp_cands;
} DEC_CORE;

/******************************************************************************
 * CONTEXT used for decoding process.
 *
 * All have to be stored are in this structure.
 *****************************************************************************/
typedef struct _DEC_CTX DEC_CTX;
struct _DEC_CTX
{
    COM_INFO              info;
    /* magic code */
    u32                   magic;
    /* DEC identifier */
    DEC                   id;
    /* CORE information used for fast operation */
    DEC_CORE             *core;
    /* current decoding bitstream */
    COM_BSR               bs;

    /* decoded picture buffer management */
    COM_PM                dpm;
    /* create descriptor */
    DEC_CDSC              cdsc;

    /* current decoded (decoding) picture buffer */
    COM_PIC              *pic;
    /* SBAC */
    DEC_SBAC              sbac_dec;
    u8                    init_flag;

    COM_MAP               map;
#if CHROMA_NOT_SPLIT
    u8                    tree_status;
#endif
#if MODE_CONS
    u8                    cons_pred_mode;
#endif

    /* total count of remained LCU for decoding one picture. if a picture is
    decoded properly, this value should reach to zero */
    int                   lcu_cnt;

    int                 **ppbEdgeFilter[LOOPFILTER_DIR_TYPE];

    COM_PIC              *pic_sao;
    SAOStatData        ***saostatData; //[SMB][comp][types]
    SAOBlkParam         **saoBlkParams; //[SMB][comp]
    SAOBlkParam         **rec_saoBlkParams;//[SMB][comp]

    COM_PIC              *pic_alf_Dec;
    COM_PIC              *pic_alf_Rec;
    int                   pic_alf_on[N_C];
    int                ***Coeff_all_to_write_ALF;
    DecALFVar            *Dec_ALF;

    u8                    ctx_flags[NUM_CNID];

    /**************************************************************************/
    /* current slice number, which is increased whenever decoding a slice.
    when receiving a slice for new picture, this value is set to zero.
    this value can be used for distinguishing b/w slices */
    u16                   slice_num;
    /* last coded intra picture's presentation temporal reference */
    int                   last_intra_ptr;
    /* current picture's decoding temporal reference */
    int                   dtr;
    /* previous picture's decoding temporal reference low part */
    int                   dtr_prev_low;
    /* previous picture's decoding temporal reference high part */
    int                   dtr_prev_high;
    /* current picture's presentation temporal reference */
    int                   ptr;
    /* the number of currently decoded pictures */
    int                   pic_cnt;
    /* picture buffer allocator */
    PICBUF_ALLOCATOR      pa;
    /* bitstream has an error? */
    u8                    bs_err;
    /* reference picture (0: foward, 1: backward) */
    COM_REFP              refp[MAX_NUM_REF_PICS][REFP_NUM];
    /* flag for picture signature enabling */
    u8                    use_pic_sign;
    /* picture signature (MD5 digest 128bits) */
    u8                    pic_sign[16];
    /* flag to indicate picture signature existing or not */
    u8                    pic_sign_exist;

#if PATCH
    int                   patch_column_width[64];
    int                   patch_row_height[128];
    PATCH_INFO           *patch;
#endif

    u8                   *wq[2];
};

/* prototypes of internal functions */
int  dec_pull_frm(DEC_CTX *ctx, COM_IMGB **imgb, int state /*0: STATE_DECODING, 1: STATE_BUMPING*/);
int  dec_ready(DEC_CTX * ctx);
void dec_flush(DEC_CTX * ctx);
int  dec_cnk(DEC_CTX * ctx, COM_BITB * bitb, DEC_STAT * stat);

int  dec_deblock_avs2(DEC_CTX * ctx);

int  dec_sao_avs2(DEC_CTX * ctx);
void readParaSAO_one_LCU(DEC_CTX* ctx, int y_pel, int x_pel,
                         SAOBlkParam *saoBlkParam, SAOBlkParam *rec_saoBlkParam);
void read_sao_lcu(DEC_CTX* ctx, int lcu_pos, int pix_y, int pix_x, int smb_pix_width, int smb_pix_height,
                  SAOBlkParam *sao_cur_param, SAOBlkParam *rec_sao_cur_param);

int  dec_pic(DEC_CTX * ctx, DEC_CORE * core, COM_SQH *sqh, COM_PIC_HEADER * ph, COM_SH_EXT * shext);
#if PATCH
void de_copy_lcu_scu(u32 * scu_temp, u32 * scu_best, s8(*refi_temp)[REFP_NUM], s8(*refi_best)[REFP_NUM], s16(*mv_temp)[REFP_NUM][MV_D],
                     s16(*mv_best)[REFP_NUM][MV_D], u32 *cu_mode_temp, u32 *cu_mode_best, PATCH_INFO * patch, int pic_width, int pic_height);
void dec_set_patch_idx(s8 *map_patch_idx, PATCH_INFO * patch, int pic_width, int pic_height);
#endif
#include "dec_util.h"
#include "dec_eco.h"
#include "com_picman.h"

#endif /* _DEC_DEF_H_ */
