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

#ifndef _ENC_DEF_H_
#define _ENC_DEF_H_

#include "com_def.h"
#include "enc_bsw.h"
#include "enc_sad.h"

/* support RDOQ */
#define SCALE_BITS               15    /* Inherited from TMuC, pressumably for fractional bit estimates in RDOQ */
#if USE_RDOQ
#define ERR_SCALE_PRECISION_BITS 20
#endif
/* encoder magic code */
#define ENC_MAGIC_CODE         0x45565945 /* EVYE */

#define GOP_P                    4

enum PIC_BUFS
{
    PIC_IDX_ORG = 0,
    PIC_IDX_REC = 1,
    PIC_BUF_NUM = 2,
};

/* motion vector accuracy level for inter-mode decision */
#define ME_LEV_IPEL              1
#define ME_LEV_HPEL              2
#define ME_LEV_QPEL              3

/* maximum inbuf count */
#define ENC_MAX_INBUF_CNT      32

/* maximum cost value */
#define MAX_COST                (1.7e+308)


/* virtual frame depth B picture */
#define FRM_DEPTH_0                   0
#define FRM_DEPTH_1                   1
#define FRM_DEPTH_2                   2
#define FRM_DEPTH_3                   3
#define FRM_DEPTH_4                   4
#define FRM_DEPTH_5                   5
#define FRM_DEPTH_MAX                 6
/* I-slice, P-slice, B-slice + depth + 1 (max for GOP 8 size)*/
#define LIST_NUM                      1

/* instance identifier for encoder */
typedef void  * ENC;

/*****************************************************************************
 * original picture buffer structure
 *****************************************************************************/
typedef struct _ENC_PICO
{
    /* original picture store */
    COM_PIC                pic;
    /* input picture count */
    u32                     pic_icnt;
    /* be used for encoding input */
    u8                      is_used;

    /* address of sub-picture */
    COM_PIC              * spic;
} ENC_PICO;


/*****************************************************************************
 * intra prediction structure
 *****************************************************************************/
typedef struct _ENC_PINTRA
{
    /* temporary prediction buffer */
    pel                 pred[N_C][MAX_CU_DIM];
    pel                 pred_cache[IPD_CNT][MAX_CU_DIM]; // only for luma

    /* reconstruction buffer */
    pel                 rec[N_C][MAX_CU_DIM];

    /* address of original (input) picture buffer */
    pel               * addr_org[N_C];
    /* stride of original (input) picture buffer */
    int                 stride_org[N_C];

    /* address of reconstruction picture buffer */
    pel               * addr_rec_pic[N_C];
    /* stride of reconstruction picture buffer */
    int                 stride_rec[N_C];

    /* QP for luma */
    u8                  qp_y;
    /* QP for chroma */
    u8                  qp_u;
    u8                  qp_v;

    int                 slice_type;

    int                 complexity;
    void              * pdata[4];
    int               * ndata[4];

    int                 bit_depth;
} ENC_PINTRA;

/*****************************************************************************
 * inter prediction structure
 *****************************************************************************/

#define MV_RANGE_MIN           0
#define MV_RANGE_MAX           1
#define MV_RANGE_DIM           2

typedef struct _ENC_PINTER ENC_PINTER;
struct _ENC_PINTER
{
    int bit_depth;
    /* temporary prediction buffer (only used for ME)*/
    pel  pred_buf[MAX_CU_DIM];
    /* reconstruction buffer */
    pel  rec_buf[N_C][MAX_CU_DIM];

    s16  mvp_scale[REFP_NUM][MAX_NUM_ACTIVE_REF_FRAME][MV_D];
    s16  mv_scale[REFP_NUM][MAX_NUM_ACTIVE_REF_FRAME][MV_D];

    u8   curr_mvr;
    int  max_imv[MV_D];

    int max_search_range;

    CPMV  affine_mvp_scale[REFP_NUM][MAX_NUM_ACTIVE_REF_FRAME][VER_NUM][MV_D];
    CPMV  affine_mv_scale[REFP_NUM][MAX_NUM_ACTIVE_REF_FRAME][VER_NUM][MV_D];
    int best_mv_uni[REFP_NUM][MAX_NUM_ACTIVE_REF_FRAME][MV_D];
    pel  p_error[MAX_CU_DIM];
    int  i_gradient[2][MAX_CU_DIM];

    s16  org_bi[MAX_CU_DIM];
    s32  mot_bits[REFP_NUM];

    u8   num_refp;
    /* minimum clip value */
    s16  min_mv_offset[MV_D];
    /* maximum clip value */
    s16  max_mv_offset[MV_D];
    /* search range for int-pel */
    s16  search_range_ipel[MV_D];
    /* search range for sub-pel */
    s16  search_range_spel[MV_D];
    s8  (*search_pattern_hpel)[2];
    u8   search_pattern_hpel_cnt;
    s8  (*search_pattern_qpel)[2];
    u8   search_pattern_qpel_cnt;

    /* original (input) picture buffer */
    COM_PIC        *pic_org;
    /* address of original (input) picture buffer */
    pel             *Yuv_org[N_C];
    /* stride of original (input) picture buffer */
    int              stride_org[N_C];
    /* motion vector map */
    s16            (*map_mv)[REFP_NUM][MV_D];
    /* picture width in SCU unit */
    int              pic_width_in_scu;
    /* QP for luma of current encoding CU */
    int               qp_y;
    /* QP for chroma of current encoding CU */
    int               qp_u;
    int               qp_v;
    u32              lambda_mv;
    /* reference pictures */
    COM_REFP      (*refp)[REFP_NUM];
    int              slice_type;
    /* search level for motion estimation */
    int              me_level;
    int              complexity;
    void            *pdata[4];
    int             *ndata[4];
    /* current frame numbser */
    int              ptr;
    /* gop size */
    int              gop_size;
    /* ME function (Full-ME or Fast-ME) */
    u32            (*fn_me)(ENC_PINTER *pi, int x, int y, int w, int h, int cu_x, int cu_y, int cu_stride, s8 *refi, int lidx, s16 mvp[MV_D], s16 mv[MV_D], int bi);
    /* AFFINE ME function (Gradient-ME) */
    u32            (*fn_affine_me)(ENC_PINTER *pi, int x, int y, int cu_width_log2, int cu_height_log2, s8 *refi, int lidx, CPMV mvp[VER_NUM][MV_D], CPMV mv[VER_NUM][MV_D], int bi, int vertex_num, int sub_w, int sub_h);
};

/* encoder parameter */
typedef struct _ENC_PARAM
{
    /* picture size of input sequence (width) */
    int            horizontal_size;
    /* picture size of input sequence (height) */
    int            vertical_size;
    
    /* picture size of pictures in DPB (width) */
    int            pic_width;  // be a multiple of 8 (MINI_SIZE)
    /* picture size of pictures in DPB (height) */
    int            pic_height; // be a multiple of 8 (MINI_SIZE)
    /* qp value for I- and P- slice */
    int            qp;
    /* frame per second */
    int            fps;
    /* I-frame period */
    int            i_period;
    /* force I-frame */
    int            f_ifrm;
    /* picture bit depth*/
    int            bit_depth_input;
    int            bit_depth_internal;
    /* use picture signature embedding */
    int            use_pic_sign;
    int            max_b_frames;
    /* start bumping process if force_output is on */
    int            force_output;
    int            disable_hgop;
    int            gop_size;
    int            use_dqp;
    int            frame_qp_add;           /* 10 bits*/
    int            pb_frame_qp_offset;
#if IPCM
    int            ipcm_enable_flag;
#endif
    int            amvr_enable_flag;
    int            affine_enable_flag;
    int            smvd_enable_flag;
    int            use_deblock;
    int            num_of_hmvp_cand;
    int            ipf_flag;
#if TSCPM
    int            tscpm_enable_flag;
#endif
    int            umve_enable_flag;
#if EXT_AMVR_HMVP
    int            emvr_enable_flag;
#endif
#if DT_PARTITION
    int            dt_intra_enable_flag;
#endif
    int            wq_enable;
    int            seq_wq_mode;
    char           seq_wq_user[2048];
    int            pic_wq_data_idx;
    char           pic_wq_user[2048];
    int            wq_param;
    int            wq_model;
    char           wq_param_detailed[256];
    char           wq_param_undetailed[256];

    int            sample_adaptive_offset_enable_flag;
    int            adaptive_leveling_filter_enable_flag;
    int            secondary_transform_enable_flag;
    u8             position_based_transform_enable_flag;
    u8             library_picture_enable_flag;
    u8             delta_qp_flag;
    u8             chroma_format;
    u8             encoding_precision;
#if HLS_RPL
    COM_RPL        rpls_l0[MAX_NUM_RPLS];
    COM_RPL        rpls_l1[MAX_NUM_RPLS];
    int            rpls_l0_cfg_num;
    int            rpls_l1_cfg_num;
#endif
#if PATCH
    int            patch_stable;
    int            cross_patch_loop_filter;
    int            patch_uniform;
    int            patch_ref_colocated;
    int            patch_width_in_lcu;
    int            patch_height_in_lcu;
    int            patch_columns;
    int            patch_rows;
    int            patch_column_width[64];
    int            patch_row_height[128];
#endif
#if LIBVC_ON
    int            qp_offset_libpic;
#endif
    int            sub_sample_ratio;
    int            frames_to_be_encoded;
    u8             ctu_size;
    u8             min_cu_size;
    u8             max_part_ratio;
    u8             max_split_times;
    u8             min_qt_size;
    u8             max_bt_size;
    u8             max_eqt_size;
    u8             max_dt_size;
    int            qp_offset_cb;
    int            qp_offset_cr;
    int            qp_offset_adp;
    int            bit_depth;
} ENC_PARAM;

typedef struct _ENC_SBAC
{
    u32            range;
    u32            code;
    int            left_bits;
    u32            stacked_ff;
    u32            pending_byte;
    u32            is_pending_byte;
    COM_SBAC_CTX  ctx;
    u32            bitcounter;
    u8             is_bitcount;
} ENC_SBAC;

typedef struct _ENC_CU_DATA
{
    s8  split_mode[MAX_CU_DEPTH][NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU];
#if TB_SPLIT_EXT
    int* pb_part;
    int* tb_part;
#endif
    u8  *pred_mode;
    u8  **mpm;
    u8  **mpm_ext;
    s8  **ipm;

    s8  **refi;
    u8  *mvr_idx;
    u8  *umve_flag;
    u8  *umve_idx;
    u8  *skip_idx;
#if EXT_AMVR_HMVP
    u8  *mvp_from_hmvp_flag;
#endif

    s16 mv[MAX_CU_CNT_IN_LCU][REFP_NUM][MV_D];
    s16 mvd[MAX_CU_CNT_IN_LCU][REFP_NUM][MV_D];
    int *num_nz_coef[N_C];
    u32 *map_scu;
    u8  *affine_flag;
#if SMVD
    u8  *smvd_flag;
#endif
    u32 *map_cu_mode;
#if TB_SPLIT_EXT
    u32 *map_pb_tb_part;
#endif
    s8  *depth;
    u8 *ipf_flag;
    s16 *coef[N_C];
    pel *reco[N_C];
} ENC_CU_DATA;

typedef struct _ENC_BEF_DATA
{
    int    visit;
    int    nosplit;
    int    split;
    int    split_visit;

    double split_cost[MAX_SPLIT_NUM];

    int    mvr_idx_history;

    int    affine_flag_history;

#if EXT_AMVR_HMVP
    int    mvr_hmvp_idx_history;
#endif

#if TR_SAVE_LOAD
    u8     num_inter_pred;
    u16    inter_pred_dist[NUM_SL_INTER];
    u8     inter_tb_part[NUM_SL_INTER];  // luma TB part size for inter prediction block
#endif
#if DT_SAVE_LOAD
    u8     best_part_size_intra[2];
    u8     num_intra_history;
#endif
} ENC_BEF_DATA;

typedef struct _EncALFVar
{
    AlfCorrData **m_alfCorr[N_C];
    AlfCorrData **m_alfNonSkippedCorr[N_C];
    AlfCorrData  *m_alfCorrMerged[N_C];

    double  *m_y_merged[NO_VAR_BINS];
    double **m_E_merged[NO_VAR_BINS];
    double   m_y_temp[ALF_MAX_NUM_COEF];
    double   m_pixAcc_merged[NO_VAR_BINS];

    double **m_E_temp;
    ALFParam  **m_alfPictureParam;

    int *m_coeffNoFilter[NO_VAR_BINS];

    int **m_filterCoeffSym;
    int m_varIndTab[NO_VAR_BINS];

    int *m_numSlicesDataInOneLCU;

    pel **m_varImg;
    BOOL **m_AlfLCUEnabled;
    unsigned int m_uiBitIncrement;

#if ALF_LOW_LANTENCY_ENC
    AlfCorrData ** **m_alfPrevCorr;           //[TemporalLayer] [YUV] [NumCUInFrame] ->AlfCorrData
    ALFParam ** *m_temporal_alfPictureParam;  //[TemporalLayer] [YUV] -> ALFParam
#endif
} EncALFVar;

/*****************************************************************************
 * CORE information used for encoding process.
 *
 * The variables in this structure are very often used in encoding process.
 *****************************************************************************/
typedef struct _ENC_CORE
{
    /* mode decision structure */
    COM_MODE       mod_info_best;
    COM_MODE       mod_info_curr;
#if TB_SPLIT_EXT
    COM_MODE       mod_info_save;
    //intra rdo copy the current best info directly into core->mod_info_best; need an internal pb_part for intra
    int            best_pb_part_intra;
    int            best_tb_part_intra;
#endif

    /* coefficient buffer of current CU */
    s16            coef[N_C][MAX_CU_DIM];
    /* CU data for RDO */
    ENC_CU_DATA  cu_data_best[MAX_CU_DEPTH][MAX_CU_DEPTH];
    ENC_CU_DATA  cu_data_temp[MAX_CU_DEPTH][MAX_CU_DEPTH];
    /* temporary coefficient buffer */
    s16            ctmp[N_C][MAX_CU_DIM];

    /* neighbor pixel buffer for intra prediction */
    pel            nb[N_C][N_REF][MAX_CU_SIZE * 3];
    /* current encoding LCU number */
    int            lcu_num;

    /* QP for luma of current encoding CU */
    int             qp_y;
    /* QP for chroma of current encoding CU */
    int             qp_u;
    int             qp_v;
    /* X address of current LCU */
    int            x_lcu;
    /* Y address of current LCU */
    int            y_lcu;
    /* X address of current CU in SCU unit */
    int            x_scu;
    /* Y address of current CU in SCU unit */
    int            y_scu;
    /* left pel position of current LCU */
    int            x_pel;
    /* top pel position of current LCU */
    int            y_pel;
    
    /* CU position in current LCU in SCU unit */
    int            cup;
    /* CU depth */
    int            cud;

    /* skip flag for MODE_INTER */
    u8             skip_flag;

    /* split flag for Qt_split_flag */
    u8             split_flag;

    

    /* platform specific data, if needed */
    void          *pf;
    /* bitstream structure for RDO */
    COM_BSW       bs_temp;
    /* SBAC structure for full RDO */
    ENC_SBAC     s_curr_best[MAX_CU_DEPTH][MAX_CU_DEPTH];
    ENC_SBAC     s_next_best[MAX_CU_DEPTH][MAX_CU_DEPTH];
    ENC_SBAC     s_temp_best;
    ENC_SBAC     s_temp_run;
    ENC_SBAC     s_temp_prev_comp_best;
    ENC_SBAC     s_temp_prev_comp_run;
#if TB_SPLIT_EXT
    ENC_SBAC     s_temp_pb_part_best;
#endif
    ENC_SBAC     s_curr_before_split[MAX_CU_DEPTH][MAX_CU_DEPTH];
    ENC_BEF_DATA bef_data[MAX_CU_DEPTH][MAX_CU_DEPTH][MAX_CU_CNT_IN_LCU];
#if TR_SAVE_LOAD
    u8           best_tb_part_hist;
#endif
#if TR_EARLY_TERMINATE
    s64          dist_pred_luma;
#endif

    ENC_SBAC     s_sao_init, s_sao_cur_blk, s_sao_next_blk;
    ENC_SBAC     s_sao_cur_type, s_sao_next_type;
    ENC_SBAC     s_sao_cur_mergetype, s_sao_next_mergetype;

    ENC_SBAC     s_alf_cu_ctr;
    ENC_SBAC     s_alf_initial;

    double         cost_best;
    u32            inter_satd;

    s32            dist_cu;
    s32            dist_cu_best; //dist of the best intra mode (note: only updated in intra coding now)

    // for storing the update-to-date motion list
    COM_MOTION motion_cands[ALLOWED_HMVP_NUM];
    s8 cnt_hmvp_cands;

#if EXT_AMVR_HMVP
    u8    skip_mvps_check;
#endif
} ENC_CORE;

/******************************************************************************
 * CONTEXT used for encoding process.
 *
 * All have to be stored are in this structure.
 *****************************************************************************/
typedef struct _ENC_CTX ENC_CTX;
struct _ENC_CTX
{
    COM_INFO              info;
    /* address of current input picture, ref_picture  buffer structure */
    ENC_PICO            *pico_buf[ENC_MAX_INBUF_CNT];
    /* address of current input picture buffer structure */
    ENC_PICO            *pico;
    /* index of current input picture buffer in pico_buf[] */
    u32                    pico_idx;
    int                    pico_max_cnt;
    /* magic code */
    u32                    magic;
    /* ENC identifier */
    ENC                  id;
    /* address of core structure */
    ENC_CORE             *core;
    /* address indicating original picture and reconstructed picture */
    COM_PIC              *pic[PIC_BUF_NUM];
    /* reference picture (0: foward, 1: backward) */
    COM_REFP              refp[MAX_NUM_REF_PICS][REFP_NUM];
    /* encoding parameter */
    ENC_PARAM             param;
    /* bitstream structure */
    COM_BSW                bs;
    /* bitstream structure for RDO */
    COM_BSW                bs_temp;

    /* reference picture manager */
    COM_PM                 rpm;

    /* current encoding picture count(This is not PicNum or FrameNum.
    Just count of encoded picture correctly) */
    u32                    pic_cnt;
    /* current picture input count (only update when CTX0) */
    int                    pic_icnt;
    /* total input picture count (only used for bumping process) */
    u32                    pic_ticnt;
    /* remaining pictures is encoded to p or b slice (only used for bumping process) */
    u8                     force_slice;
    /* ignored pictures for force slice count (unavailable pictures cnt in gop,\
    only used for bumping process) */
    int                     force_ignored_cnt;
    /* initial frame return number(delayed input count) due to B picture or Forecast */
    int                    frm_rnum;
    /* current encoding slice number in one picture */
    int                    slice_num;
    /* first mb number of current encoding slice in one picture */
    int                    sl_first_mb;
    /* current slice type */
    u8                     slice_type;
    /* slice depth for current picture */
    u8                     slice_depth;
    /* whether current picture is referred or not */
    u8                     ref_depth;
    /* flag whether current picture is refecened picture or not */
    u8                     slice_ref_flag;
    /* current picture POC number */
    int                    poc;
    /* maximum CU depth */
    u8                     max_cud;
    ENC_SBAC               sbac_enc;
    /* address of inbufs */
    COM_IMGB              *inbuf[ENC_MAX_INBUF_CNT];
    /* last coded intra picture's presentation temporal reference */
    int                    last_intra_ptr;

    /* total count of remained LCU for encoding one picture. if a picture is
    encoded properly, this value should reach to zero */
    int                    lcu_cnt;

    /* log2 of SCU count in a LCU row */
    u8                     log2_culine;
    /* log2 of SCU count in a LCU (== log2_culine * 2) */
    u8                     log2_cudim;
    /* intra prediction analysis */
    ENC_PINTRA             pintra;
    /* inter prediction analysis */
    ENC_PINTER             pinter;
    /* picture buffer allocator */
    PICBUF_ALLOCATOR       pa;
    /* current picture's decoding temporal reference */
    int                    dtr;
    /* current picture's presentation temporal reference */
    int                    ptr;
    /*current picutre's layer id for hierachical structure */
    u8                     temporal_id;

    /* cu data for current LCU */
    ENC_CU_DATA           *map_cu_data;

    COM_MAP                map;
#if CHROMA_NOT_SPLIT
    u8                     tree_status;
#endif
#if MODE_CONS
    u8                     cons_pred_mode;
#endif

#if RDO_DBK
    COM_PIC               *pic_dbk;      //one picture that arranges cu pixels and neighboring pixels for deblocking (just to match the interface of deblocking functions)
    s64                    delta_dist;   //delta distortion from filtering (negative values mean distortion reduced)
    s64                    dist_nofilt;  //distortion of not filtered samples
    s64                    dist_filter;  //distortion of filtered samples
#endif

    int **ppbEdgeFilter[LOOPFILTER_DIR_TYPE];

    COM_PIC               *pic_sao;
    SAOStatData         ***saostatData; //[SMB][comp][types]
    SAOBlkParam          **saoBlkParams; //[SMB][comp]
    SAOBlkParam          **rec_saoBlkParams;//[SMB][comp]

    int                    pic_alf_on[N_C];
    EncALFVar             *Enc_ALF;
    COM_PIC               *pic_alf_Org;
    COM_PIC               *pic_alf_Rec;

    double                 lambda[3];
    double                 sqrt_lambda[3];
    double                 dist_chroma_weight[2];
#if PATCH
    PATCH_INFO             *patch;
#endif
#if LIBVC_ON
    int                    is_RLpic_flag;
#endif
    u8 *wq[2];
#if FIXED_SPLIT
    int                    ctu_idx_in_sequence;
    int                    ctu_idx_in_picture;
    int                    ctu_idx_in_seq_I_slice_ctu;
    int                    ctu_idx_in_seq_B_slice_ctu;
    SPLIT_MODE             split_combination[6];
#endif
};

/*****************************************************************************
 * API for encoder only
 *****************************************************************************/
ENC enc_create(ENC_PARAM * param, int * err);
void enc_delete(ENC id);
int enc_encode(ENC id, COM_BITB * bitb, ENC_STAT * stat);

int enc_pic_prepare(ENC_CTX * ctx, COM_BITB * bitb);
int enc_pic_finish(ENC_CTX * ctx, COM_BITB * bitb, ENC_STAT * stat);
int enc_pic(ENC_CTX * ctx, COM_BITB * bitb, ENC_STAT * stat);
int enc_deblock_avs2(ENC_CTX * ctx, COM_PIC * pic);
int enc_push_frm(ENC_CTX * ctx, COM_IMGB * img);
int enc_ready(ENC_CTX * ctx);
void enc_flush(ENC_CTX * ctx);
int enc_picbuf_get_inbuf(ENC_CTX * ctx, COM_IMGB ** img);
#if REPEAT_SEQ_HEADER
int init_seq_header(ENC_CTX * ctx, COM_BITB * bitb);
#else
int enc_seq_header(ENC_CTX * ctx, COM_BITB * bitb, ENC_STAT * stat);
#endif
#if PATCH
void en_copy_lcu_scu(u32 * scu_temp, u32 * scu_best, s8(*refi_temp)[REFP_NUM], s8(*refi_best)[REFP_NUM], s16(*mv_temp)[REFP_NUM][MV_D], s16(*mv_best)[REFP_NUM][MV_D], u32 *cu_mode_temp, u32 *cu_mode_best, PATCH_INFO * patch, int pic_width, int pic_height);
void enc_set_patch_idx(s8 * map_patch_idx, PATCH_INFO * patch, int pic_width, int pic_height);
#endif
#include "enc_util.h"
#include "enc_eco.h"
#include "enc_mode.h"
#include "enc_tq.h"
#include "enc_pintra.h"
#include "enc_pinter.h"
#include "enc_tbl.h"


#include "kernel.h"

#endif /* _ENC_DEF_H_ */
