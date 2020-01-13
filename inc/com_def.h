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

#ifndef _COM_DEF_H_
#define _COM_DEF_H_

#include "com_typedef.h"
#include "com_port.h"

#if TSCPM
#define MAX_INT                     2147483647  ///< max. value of signed 32-bit integer
#endif

/* profile & level */
#define PROFILE_ID 0x22
#define LEVEL_ID 0x6A

/* MCABAC (START) */
#define PROB_BITS                         11 // LPS_PROB(10-bit) + MPS(1-bit)
#define PROB_MASK                         ((1 << PROB_BITS) - 1) // mask for LPS_PROB + MPS
#define MAX_PROB                          ((1 << PROB_BITS) - 1) // equal to PROB_LPS + PROB_MPS, 0x7FF
#define HALF_PROB                         (MAX_PROB >> 1)
#define QUAR_HALF_PROB                    (1 << (PROB_BITS-3))
#define LG_PMPS_SHIFTNO                   2

#define PROB_INIT                         (HALF_PROB << 1) /* 1/2 of initialization = (HALF_PROB << 1)+ MPS(0) */

#define READ_BITS_INIT                    16
#define DEC_RANGE_SHIFT                   (READ_BITS_INIT-(PROB_BITS-2))

#define COM_BSR_IS_BYTE_ALIGN(bs)         !((bs)->leftbits & 0x7)

/* MCABAC (END) */

/* Multiple Reference (START) */
#define MAX_NUM_ACTIVE_REF_FRAME_B         2  /* Maximum number of active reference frames for RA condition */
#define MAX_NUM_ACTIVE_REF_FRAME_LDB       4  /* Maximum number of active reference frames for LDB condition */
#define MV_SCALE_PREC                      14 /* Scaling precision for motion vector prediction (2^MVP_SCALING_PRECISION) */
/* Multiple Reference (END) */

/* Max. and min. Quantization parameter */
#define MIN_QUANT                          0
#define MAX_QUANT_BASE                     63


/* AMVR (START) */
#define MAX_NUM_MVR                        5  /* 0 (1/4-pel) ~ 4 (4-pel) */
#if BD_AFFINE_AMVR
#define MAX_NUM_AFFINE_MVR                 3
#endif
#define FAST_MVR_IDX                       2
#define SKIP_MVR_IDX                       1
/* AMVR (END)  */

/* DB_AVS2 (START) */
#define EDGE_TYPE_LUMA                     1
#define EDGE_TYPE_ALL                      2
#define LOOPFILTER_DIR_TYPE                2
#define LOOPFILTER_SIZE_IN_BIT             2
#define LOOPFILTER_SIZE                    (1 << LOOPFILTER_SIZE_IN_BIT)
#define LOOPFILTER_GRID                    8
#define DB_CROSS_SLICE                     1
/* DB_AVS2 (END) */

/* SAO_AVS2 (START) */
#define NUM_BO_OFFSET                      32
#define MAX_NUM_SAO_CLASSES                32
#define NUM_SAO_BO_CLASSES_LOG2            5
#define NUM_SAO_BO_CLASSES_IN_BIT          5
#define NUM_SAO_EO_TYPES_LOG2              2
#define NUM_SAO_BO_CLASSES                 (1<<NUM_SAO_BO_CLASSES_LOG2)
#define SAO_SHIFT_PIX_NUM                  4
/* SAO_AVS2 (END) */

/* ALF_AVS2 (START) */
#define ALF_LOW_LANTENCY_ENC               0
#define ALF_MAX_NUM_COEF                   9
#define NO_VAR_BINS                        16
#define ALF_REDESIGN_ITERATION             3
#define LOG2_VAR_SIZE_H                    2
#define LOG2_VAR_SIZE_W                    2
#define ALF_FOOTPRINT_SIZE                 7
#define DF_CHANGED_SIZE                    3
#define ALF_NUM_BIT_SHIFT                  6
/* ALF_AVS2 (END) */

/* Common stuff (START) */
#if defined(_MSC_VER)
#define ALIGNED_(x) __declspec(align(x))
#define FORCE_INLINE __forceinline
//#define INLINE __inline
#else
#if defined(__GNUC__)
#define ALIGNED_(x) __attribute__ ((aligned(x)))
#define FORCE_INLINE __attribute__((always_inline))
//#define INLINE __inline__
#endif
#endif

#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

typedef int BOOL;
#define TRUE  1
#define FALSE 0

/* AFFINE (START) */
#define VER_NUM                             4
#define AFFINE_MAX_NUM_LT                   3 ///< max number of motion candidates in top-left corner
#define AFFINE_MAX_NUM_RT                   2 ///< max number of motion candidates in top-right corner
#define AFFINE_MAX_NUM_LB                   2 ///< max number of motion candidates in left-bottom corner
#define AFFINE_MAX_NUM_RB                   1 ///< max number of motion candidates in right-bottom corner
#define AFFINE_MIN_BLOCK_SIZE               4 ///< Minimum affine MC block size

#define AFF_MAX_NUM_MRG                     5 // maximum affine merge candidates
#define AFF_MODEL_CAND                      2 // maximum affine model based candidate

#define MAX_MEMORY_ACCESS_BI                ((8 + 7) * (8 + 7) / 64)
#define MAX_MEMORY_ACCESS_UNI               ((8 + 7) * (4 + 7) / 32)

// AFFINE ME configuration (non-normative)
#define AF_ITER_UNI                         7 // uni search iteration time
#define AF_ITER_BI                          5 // bi search iteration time
#define AFFINE_BI_ITER                      1

#define AFF_SIZE                            16
/* AFFINE (END) */

/* IPF (START) */
#define NUM_IPF_CTX                         1 ///< number of context models for MPI Idx coding
#if DT_PARTITION
#define DT_INTRA_BOUNDARY_FILTER_OFF        1 ///< turn off boundary filter if intra DT is ON
#endif
/* IPF (END) */

/* For debugging (START) */
#define ENC_DEC_TRACE                       0
#if ENC_DEC_TRACE
#define MVF_TRACE                           1 ///< use for tracing MVF
#define TRACE_REC                           0 ///< trace reconstructed pixels
#define TRACE_RDO                           0 //!< Trace only encode stream (0), only RDO (1) or all of them (2)
#define TRACE_BIN                           0 //!< trace each bin
#if TRACE_RDO
#define TRACE_RDO_EXCLUDE_I                 0 //!< Exclude I frames
#endif
extern FILE *fp_trace;
extern int fp_trace_print;
extern int fp_trace_counter;
#if TRACE_RDO == 1
#define COM_TRACE_SET(A) fp_trace_print=!A
#elif TRACE_RDO == 2
#define COM_TRACE_SET(A)
#else
#define COM_TRACE_SET(A) fp_trace_print=A
#endif
#define COM_TRACE_STR(STR) if(fp_trace_print) { fprintf(fp_trace, STR); fflush(fp_trace); }
#define COM_TRACE_INT(INT) if(fp_trace_print) { fprintf(fp_trace, "%d ", INT); fflush(fp_trace); }
#define COM_TRACE_COUNTER  COM_TRACE_INT(fp_trace_counter++); COM_TRACE_STR("\t")
#define COM_TRACE_MV(X, Y) if(fp_trace_print) { fprintf(fp_trace, "(%d, %d) ", X, Y); fflush(fp_trace); }
#define COM_TRACE_FLUSH    if(fp_trace_print) fflush(fp_trace)
#if TRACE_BIN
#define COM_TRACE_DOUBLE(D) if(fp_trace_print) { fprintf(fp_trace, "%e ", D); fflush(fp_trace); }
#endif
#else
#define COM_TRACE_SET(A)
#define COM_TRACE_STR(str)
#define COM_TRACE_INT(INT)
#define COM_TRACE_COUNTER
#define COM_TRACE_MV(X, Y)
#define COM_TRACE_FLUSH
#endif
/* For debugging (END) */

#define STRIDE_IMGB2PIC(s_imgb)          ((s_imgb)>>1)

#define Y_C                                0  /* Y luma */
#define U_C                                1  /* Cb Chroma */
#define V_C                                2  /* Cr Chroma */
#define N_C                                3  /* number of color component */

#define REFP_0                             0
#define REFP_1                             1
#define REFP_NUM                           2

/* X direction motion vector indicator */
#define MV_X                               0
/* Y direction motion vector indicator */
#define MV_Y                               1
/* Maximum count (dimension) of motion */
#define MV_D                               2

#define N_REF                              2  /* left, up, right */

#define NUM_NEIB                           1  //since SUCO is not implemented

#define MINI_SIZE                          8
#define MAX_CU_LOG2                        7
#define MIN_CU_LOG2                        2
#define MAX_CU_SIZE                       (1 << MAX_CU_LOG2)
#define MIN_CU_SIZE                       (1 << MIN_CU_LOG2)
#define MAX_CU_DIM                        (MAX_CU_SIZE * MAX_CU_SIZE)
#define MIN_CU_DIM                        (MIN_CU_SIZE * MIN_CU_SIZE)
#define MAX_CU_DEPTH                       6  /* 128x128 ~ 4x4 */

#define MAX_TR_LOG2                        6  /* 64x64 */
#define MIN_TR_LOG2                        1  /* 2x2 */
#define MAX_TR_SIZE                        (1 << MAX_TR_LOG2)
#define MIN_TR_SIZE                        (1 << MIN_TR_LOG2)
#define MAX_TR_DIM                         (MAX_TR_SIZE * MAX_TR_SIZE)
#define MIN_TR_DIM                         (MIN_TR_SIZE * MIN_TR_SIZE)

#if TB_SPLIT_EXT
#define MAX_NUM_PB                         4
#define MAX_NUM_TB                         4
#define PB0                                0  // default PB idx
#define TB0                                0  // default TB idx
#define TBUV0                              0  // default TB idx for chroma

#define NUM_SL_INTER                       10
#define NUM_SL_INTRA                       8
#endif

/* maximum CB count in a LCB */
#define MAX_CU_CNT_IN_LCU                  (MAX_CU_DIM/MIN_CU_DIM)
/* pixel position to SCB position */
#define PEL2SCU(pel)                       ((pel) >> MIN_CU_LOG2)

#define PIC_PAD_SIZE_L                     (MAX_CU_SIZE + 16)
#define PIC_PAD_SIZE_C                     (PIC_PAD_SIZE_L >> 1)

#define NUM_AVS2_SPATIAL_MV                3
#define NUM_SKIP_SPATIAL_MV                6
#define MVPRED_L                           0
#define MVPRED_U                           1
#define MVPRED_UR                          2
#define MVPRED_xy_MIN                      3

/* for GOP 16 test, increase to 32 */
/* maximum reference picture count. Originally, Max. 16 */
/* for GOP 16 test, increase to 32 */
#define MAX_NUM_REF_PICS                   17

#define MAX_NUM_ACTIVE_REF_FRAME           4

/* DPB Extra size */
#define EXTRA_FRAME                        MAX_NUM_REF_PICS

/* maximum picture buffer size */
#define MAX_PB_SIZE                       (MAX_NUM_REF_PICS + EXTRA_FRAME)

/* Neighboring block availability flag bits */
#define AVAIL_BIT_UP                       0
#define AVAIL_BIT_LE                       1
#define AVAIL_BIT_UP_LE                    2


/* Neighboring block availability flags */
#define AVAIL_UP                          (1 << AVAIL_BIT_UP)
#define AVAIL_LE                          (1 << AVAIL_BIT_LE)
#define AVAIL_UP_LE                       (1 << AVAIL_BIT_UP_LE)


/* MB availability check macro */
#define IS_AVAIL(avail, pos)            (((avail)&(pos)) == (pos))
/* MB availability set macro */
#define SET_AVAIL(avail, pos)             (avail) |= (pos)
/* MB availability remove macro */
#define REM_AVAIL(avail, pos)             (avail) &= (~(pos))
/* MB availability into bit flag */
#define GET_AVAIL_FLAG(avail, bit)      (((avail)>>(bit)) & 0x1)

/*****************************************************************************
 * slice type
 *****************************************************************************/
#define SLICE_I                            COM_ST_I
#define SLICE_P                            COM_ST_P
#define SLICE_B                            COM_ST_B

#define IS_INTRA_SLICE(slice_type)       ((slice_type) == SLICE_I))
#define IS_INTER_SLICE(slice_type)      (((slice_type) == SLICE_P) || ((slice_type) == SLICE_B))

/*****************************************************************************
 * prediction mode
 *****************************************************************************/
#define MODE_INTRA                         0
#define MODE_INTER                         1
#define MODE_SKIP                          2
#define MODE_DIR                           3

/*****************************************************************************
 * prediction direction
 *****************************************************************************/
enum PredDirection
{
    PRED_L0 = 0,
    PRED_L1 = 1,
    PRED_BI = 2,
    PRED_DIR_NUM = 3,
};

#if BD_AFFINE_AMVR
#define Tab_Affine_AMVR(x)                 ((x==0) ? 2 : ((x==1) ? 4 : 0) )
#endif

/*****************************************************************************
 * skip / direct mode
 *****************************************************************************/
#define TRADITIONAL_SKIP_NUM               (PRED_DIR_NUM + 1)
#define ALLOWED_HMVP_NUM                   8
#define MAX_SKIP_NUM                       (TRADITIONAL_SKIP_NUM + ALLOWED_HMVP_NUM)

#define UMVE_BASE_NUM                      2
#define UMVE_REFINE_STEP                   5
#define UMVE_MAX_REFINE_NUM                (UMVE_REFINE_STEP * 4)

#define MAX_INTER_SKIP_RDO                 MAX_SKIP_NUM

#if EXT_AMVR_HMVP
#define SKIP_SMVD                          1
#define THRESHOLD_MVPS_CHECK               1.1
#endif

/*****************************************************************************
 * intra prediction direction
 *****************************************************************************/
#define IPD_DC                             0
#define IPD_PLN                            1  /* Luma, Planar */
#define IPD_BI                             2  /* Luma, Bilinear */

#define IPD_DM_C                           0  /* Chroma, DM*/
#define IPD_DC_C                           1  /* Chroma, DC */
#define IPD_HOR_C                          2  /* Chroma, Horizontal*/
#define IPD_VER_C                          3  /* Chroma, Vertical */
#define IPD_BI_C                           4  /* Chroma, Bilinear */
#if TSCPM
#define IPD_TSCPM_C                        5
#endif

#define IPD_CHROMA_CNT                     5

#define IPD_INVALID                       (-1)

#define IPD_RDO_CNT                        5

#define IPD_HOR                            24 /* Luma, Horizontal */
#define IPD_VER                            12 /* Luma, Vertical */

#define IPD_CNT                            33
#if IPCM
#define IPD_IPCM                           33
#endif

#define IPD_DIA_R                         18 /* Luma, Right diagonal */
#define IPD_DIA_L                         3  /* Luma, Left diagonal */
#define IPD_DIA_U                         32 /* Luma, up diagonal */

/*****************************************************************************
* Transform
*****************************************************************************/


/*****************************************************************************
 * reference index
 *****************************************************************************/
#define REFI_INVALID                      (-1)
#define REFI_IS_VALID(refi)               ((refi) >= 0)
#define SET_REFI(refi, idx0, idx1)        (refi)[REFP_0] = (idx0); (refi)[REFP_1] = (idx1)

/*****************************************************************************
 * macros for CU map

 - [ 0: 6] : slice number (0 ~ 128)
 - [ 7:14] : reserved
 - [15:15] : 1 -> intra CU, 0 -> inter CU
 - [16:22] : QP
 - [23:23] : skip mode flag
 - [24:24] : luma cbf
 - [25:30] : reserved
 - [31:31] : 0 -> no encoded/decoded CU, 1 -> encoded/decoded CU
 *****************************************************************************/
/* set intra CU flag to map */
#define MCU_SET_INTRA_FLAG(m)           (m)=((m)|(1<<15))
/* get intra CU flag from map */
#define MCU_GET_INTRA_FLAG(m)           (int)(((m)>>15) & 1)
/* clear intra CU flag in map */
#define MCU_CLEAR_INTRA_FLAG(m)         (m)=((m) & 0xFFFF7FFF)

/* set QP to map */
#define MCU_SET_QP(m, qp)               (m)=((m)|((qp)&0x7F)<<16)
/* get QP from map */
#define MCU_GET_QP(m)                   (int)(((m)>>16)&0x7F)

/* set skip mode flag */
#define MCU_SET_SF(m)                   (m)=((m)|(1<<23))
/* get skip mode flag */
#define MCU_GET_SF(m)                   (int)(((m)>>23) & 1)
/* clear skip mode flag */
#define MCU_CLR_SF(m)                   (m)=((m) & (~(1<<23)))

/* set cu cbf flag */
#define MCU_SET_CBF(m)                  (m)=((m)|(1<<24))
/* get cu cbf flag */
#define MCU_GET_CBF(m)                  (int)(((m)>>24) & 1)
/* clear cu cbf flag */
#define MCU_CLR_CBF(m)                  (m)=((m) & (~(1<<24)))

/* set encoded/decoded flag of CU to map */
#define MCU_SET_CODED_FLAG(m)           (m)=((m)|(1<<31))
/* get encoded/decoded flag of CU from map */
#define MCU_GET_CODED_FLAG(m)           (int)(((m)>>31) & 1)
/* clear encoded/decoded CU flag to map */
#define MCU_CLR_CODED_FLAG(m)           (m)=((m) & 0x7FFFFFFF)

/* multi bit setting: intra flag, encoded/decoded flag, slice number */
#define MCU_SET_IF_COD_SN_QP(m, i, sn, qp) \
    (m) = (((m)&0xFF807F80)|((sn)&0x7F)|((qp)<<16)|((i)<<15)|(1<<31))

#define MCU_IS_COD_NIF(m)               ((((m)>>15) & 0x10001) == 0x10000)

/*
- [8:9] : affine vertex number, 00: 1(trans); 01: 2(affine); 10: 3(affine); 11: 4(affine)
*/

/* set affine CU mode to map */
#define MCU_SET_AFF(m, v)               (m)=((m & 0xFFFFFCFF)|((v)&0x03)<<8)
/* get affine CU mode from map */
#define MCU_GET_AFF(m)                  (int)(((m)>>8)&0x03)
/* clear affine CU mode to map */
#define MCU_CLR_AFF(m)                  (m)=((m) & 0xFFFFFCFF)

/* set x scu offset & y scu offset relative to top-left scu of CU to map */
#define MCU_SET_X_SCU_OFF(m, v)         (m)=((m & 0xFFFF00FF)|((v)&0xFF)<<8)
#define MCU_SET_Y_SCU_OFF(m, v)         (m)=((m & 0xFF00FFFF)|((v)&0xFF)<<16)
/* get x scu offset & y scu offset relative to top-left scu of CU in map */
#define MCU_GET_X_SCU_OFF(m)            (int)(((m)>>8)&0xFF)
#define MCU_GET_Y_SCU_OFF(m)            (int)(((m)>>16)&0xFF)

/* set cu_width_log2 & cu_height_log2 to map */
#define MCU_SET_LOGW(m, v)              (m)=((m & 0xF0FFFFFF)|((v)&0x0F)<<24)
#define MCU_SET_LOGH(m, v)              (m)=((m & 0x0FFFFFFF)|((v)&0x0F)<<28)
/* get cu_width_log2 & cu_height_log2 to map */
#define MCU_GET_LOGW(m)                 (int)(((m)>>24)&0x0F)
#define MCU_GET_LOGH(m)                 (int)(((m)>>28)&0x0F)

#if TB_SPLIT_EXT
//set
#define MCU_SET_TB_PART_LUMA(m,v)       (m)=((m & 0xFFFFFF00)|((v)&0xFF)<<0 )
//get
#define MCU_GET_TB_PART_LUMA(m)         (int)(((m)>>0 )&0xFF)
#endif

typedef u32 SBAC_CTX_MODEL;
#define NUM_POSITION_CTX                   2
#define NUM_SBAC_CTX_AFFINE_MVD_FLAG       2

#if SEP_CONTEXT
#define NUM_SBAC_CTX_SKIP_FLAG             4
#else
#define NUM_SBAC_CTX_SKIP_FLAG             3
#endif
#define NUM_SBAC_CTX_SKIP_IDX              (MAX_SKIP_NUM - 1)
#if SEP_CONTEXT
#define NUM_SBAC_CTX_DIRECT_FLAG           2
#else
#define NUM_SBAC_CTX_DIRECT_FLAG           1
#endif

#define NUM_SBAC_CTX_UMVE_BASE_IDX         1
#define NUM_SBAC_CTX_UMVE_STEP_IDX         1
#define NUM_SBAC_CTX_UMVE_DIR_IDX          2

#if SEP_CONTEXT
#define NUM_SBAC_CTX_SPLIT_FLAG            4
#else
#define NUM_SBAC_CTX_SPLIT_FLAG            3
#endif
#define NUM_SBAC_CTX_SPLIT_MODE            3
#define NUM_SBAC_CTX_BT_SPLIT_FLAG         9
#if SEP_CONTEXT
#define NUM_SBAC_CTX_SPLIT_DIR             5
#else
#define NUM_SBAC_CTX_SPLIT_DIR             3
#endif
#define NUM_QT_CBF_CTX                     3       /* number of context models for QT CBF */
#if SEP_CONTEXT
#define NUM_CTP_ZERO_FLAG_CTX              2       /* number of context models for ctp_zero_flag */
#define NUM_PRED_MODE_CTX                  6       /* number of context models for prediction mode */
#else
#define NUM_CTP_ZERO_FLAG_CTX              1       /* number of context models for ctp_zero_flag */
#define NUM_PRED_MODE_CTX                  5       /* number of context models for prediction mode */
#endif
#if MODE_CONS
#define NUM_CONS_MODE_CTX                  1       /* number of context models for constrained prediction mode */
#endif
#define NUM_TB_SPLIT_CTX                   1

#define NUM_SBAC_CTX_DELTA_QP              4

#if SEP_CONTEXT
#define NUM_INTER_DIR_CTX                  3       /* number of context models for inter prediction direction */
#else
#define NUM_INTER_DIR_CTX                  2       /* number of context models for inter prediction direction */
#endif
#define NUM_REFI_CTX                       3

#define NUM_MVR_IDX_CTX                   (MAX_NUM_MVR - 1)
#if BD_AFFINE_AMVR
#define NUM_AFFINE_MVR_IDX_CTX            (MAX_NUM_AFFINE_MVR - 1)
#endif

#if EXT_AMVR_HMVP
#define NUM_EXTEND_AMVR_FLAG               1
#endif
#define NUM_MV_RES_CTX                     3       /* number of context models for motion vector difference */

#if TSCPM
#define NUM_INTRA_DIR_CTX                  10
#else
#define NUM_INTRA_DIR_CTX                  9
#endif

#define NUM_SBAC_CTX_AFFINE_FLAG           1
#define NUM_SBAC_CTX_AFFINE_MRG            AFF_MAX_NUM_MRG - 1

#if SMVD
#define NUM_SBAC_CTX_SMVD_FLAG             1
#endif

#if DT_SYNTAX
#define NUM_SBAC_CTX_PART_SIZE             6
#endif

#define NUM_SAO_MERGE_FLAG_CTX             3
#define NUM_SAO_MODE_CTX                   1
#define NUM_SAO_OFFSET_CTX                 1

#define NUM_ALF_COEFF_CTX                  1
#define NUM_ALF_LCU_CTX                    1

#define RANK_NUM 6
#define NUM_SBAC_CTX_LEVEL                 (RANK_NUM * 4)
#define NUM_SBAC_CTX_RUN                   (RANK_NUM * 4)

#define NUM_SBAC_CTX_RUN_RDOQ              (RANK_NUM * 4)


#define NUM_SBAC_CTX_LAST1                 RANK_NUM
#define NUM_SBAC_CTX_LAST2                 12

#define NUM_SBAC_CTX_LAST                  2


/* context models for arithemetic coding */
typedef struct _COM_SBAC_CTX
{
    SBAC_CTX_MODEL   skip_flag       [NUM_SBAC_CTX_SKIP_FLAG];
    SBAC_CTX_MODEL   skip_idx_ctx    [NUM_SBAC_CTX_SKIP_IDX];
    SBAC_CTX_MODEL   direct_flag     [NUM_SBAC_CTX_DIRECT_FLAG];
    SBAC_CTX_MODEL   umve_flag;
    SBAC_CTX_MODEL   umve_base_idx   [NUM_SBAC_CTX_UMVE_BASE_IDX];
    SBAC_CTX_MODEL   umve_step_idx   [NUM_SBAC_CTX_UMVE_STEP_IDX];
    SBAC_CTX_MODEL   umve_dir_idx    [NUM_SBAC_CTX_UMVE_DIR_IDX];

    SBAC_CTX_MODEL   inter_dir       [NUM_INTER_DIR_CTX];
    SBAC_CTX_MODEL   intra_dir       [NUM_INTRA_DIR_CTX];
    SBAC_CTX_MODEL   pred_mode       [NUM_PRED_MODE_CTX];
#if MODE_CONS
    SBAC_CTX_MODEL   cons_mode       [NUM_CONS_MODE_CTX];
#endif
    SBAC_CTX_MODEL   ipf_flag        [NUM_IPF_CTX];
    SBAC_CTX_MODEL   refi            [NUM_REFI_CTX];
    SBAC_CTX_MODEL   mvr_idx         [NUM_MVR_IDX_CTX];
#if BD_AFFINE_AMVR
    SBAC_CTX_MODEL   affine_mvr_idx  [NUM_AFFINE_MVR_IDX_CTX];
#endif
#if EXT_AMVR_HMVP
    SBAC_CTX_MODEL   mvp_from_hmvp_flag[NUM_EXTEND_AMVR_FLAG];
#endif
    SBAC_CTX_MODEL   mvd             [2][NUM_MV_RES_CTX];
    SBAC_CTX_MODEL   ctp_zero_flag   [NUM_CTP_ZERO_FLAG_CTX];
    SBAC_CTX_MODEL   cbf             [NUM_QT_CBF_CTX];
    SBAC_CTX_MODEL   tb_split        [NUM_TB_SPLIT_CTX];
    SBAC_CTX_MODEL   run             [NUM_SBAC_CTX_RUN];
    SBAC_CTX_MODEL   run_rdoq        [NUM_SBAC_CTX_RUN];
    SBAC_CTX_MODEL   last1           [NUM_SBAC_CTX_LAST1 * 2];
    SBAC_CTX_MODEL   last2           [NUM_SBAC_CTX_LAST2 * 2 - 2];
    SBAC_CTX_MODEL   level           [NUM_SBAC_CTX_LEVEL];
    SBAC_CTX_MODEL   split_flag      [NUM_SBAC_CTX_SPLIT_FLAG];
    SBAC_CTX_MODEL   bt_split_flag   [NUM_SBAC_CTX_BT_SPLIT_FLAG];
    SBAC_CTX_MODEL   split_dir       [NUM_SBAC_CTX_SPLIT_DIR];
    SBAC_CTX_MODEL   split_mode      [NUM_SBAC_CTX_SPLIT_MODE];
    SBAC_CTX_MODEL   affine_flag     [NUM_SBAC_CTX_AFFINE_FLAG];
    SBAC_CTX_MODEL   affine_mrg_idx  [NUM_SBAC_CTX_AFFINE_MRG];
#if SMVD
    SBAC_CTX_MODEL   smvd_flag       [NUM_SBAC_CTX_SMVD_FLAG];
#endif
#if DT_SYNTAX
    SBAC_CTX_MODEL   part_size       [NUM_SBAC_CTX_PART_SIZE];
#endif

    SBAC_CTX_MODEL   sao_merge_flag  [NUM_SAO_MERGE_FLAG_CTX];
    SBAC_CTX_MODEL   sao_mode        [NUM_SAO_MODE_CTX];
    SBAC_CTX_MODEL   sao_offset      [NUM_SAO_OFFSET_CTX];
    SBAC_CTX_MODEL   alf_lcu_enable  [NUM_ALF_LCU_CTX];
    SBAC_CTX_MODEL   delta_qp        [NUM_SBAC_CTX_DELTA_QP];
} COM_SBAC_CTX;

#define COEF_SCAN_ZIGZAG                    0
#define COEF_SCAN_TYPE_NUM                  1

/* Maximum transform dynamic range (excluding sign bit) */
#define MAX_TX_DYNAMIC_RANGE               15

#define QUANT_SHIFT                        14

/* neighbor CUs
   neighbor position:

   D     B     C

   A     X,<G>

   E          <F>
*/
#define MAX_NEB                            5
#define NEB_A                              0  /* left */
#define NEB_B                              1  /* up */
#define NEB_C                              2  /* up-right */
#define NEB_D                              3  /* up-left */
#define NEB_E                              4  /* low-left */

#define NEB_F                              5  /* co-located of low-right */
#define NEB_G                              6  /* co-located of X */
#define NEB_X                              7  /* center (current block) */
#define NEB_H                              8  /* right */
#define NEB_I                              9  /* low-right */
#define MAX_NEB2                           10

/* SAO_AVS2(START) */
enum SAOMode   //mode
{
    SAO_MODE_OFF = 0,
    SAO_MODE_MERGE,
    SAO_MODE_NEW,
    NUM_SAO_MODES
};

enum SAOModeMergeTypes
{
    SAO_MERGE_LEFT = 0,
    SAO_MERGE_ABOVE,
    NUM_SAO_MERGE_TYPES
};

enum SAOModeNewTypes   //NEW: types
{
    SAO_TYPE_EO_0,
    SAO_TYPE_EO_90,
    SAO_TYPE_EO_135,
    SAO_TYPE_EO_45,
    SAO_TYPE_BO,
    NUM_SAO_NEW_TYPES
};

enum SAOEOClasses   // EO Groups, the assignments depended on how you implement the edgeType calculation
{
    SAO_CLASS_EO_FULL_VALLEY = 0,
    SAO_CLASS_EO_HALF_VALLEY = 1,
    SAO_CLASS_EO_PLAIN = 2,
    SAO_CLASS_EO_HALF_PEAK = 3,
    SAO_CLASS_EO_FULL_PEAK = 4,
    SAO_CLASS_BO = 5,
    NUM_SAO_EO_CLASSES = SAO_CLASS_BO,
    NUM_SAO_OFFSET
};

struct SAOstatdata
{
    long long int diff[MAX_NUM_SAO_CLASSES];
    int count[MAX_NUM_SAO_CLASSES];
};

typedef struct SAOstatdata SAOStatData;
typedef struct
{
    int modeIdc; //NEW, MERGE, OFF
    int typeIdc; //NEW: EO_0, EO_90, EO_135, EO_45, BO. MERGE: left, above
    int startBand; //BO: starting band index
    int startBand2;
    int deltaband;
    int offset[MAX_NUM_SAO_CLASSES];
} SAOBlkParam;
/* SAO_AVS2(END) */

/* ALF_AVS2(START) */
typedef struct ALFParam
{
    int alf_flag;
    int num_coeff;
    int filters_per_group;
    int componentID;
    int *filterPattern;
    int **coeffmulti;
} ALFParam;

typedef struct ALF_APS
{
    int usedflag;
    int cur_number;
    int max_number;
    ALFParam alf_par[N_C];
} ALF_APS;

typedef struct _AlfCorrData
{
    double ** *ECorr; //!< auto-correlation matrix
    double  **yCorr; //!< cross-correlation
    double   *pixAcc;
    int componentID;
} AlfCorrData;
/* ALF_AVS2(END) */

/* picture store structure */
typedef struct _COM_PIC
{
    /* Start address of Y component (except padding) */
    pel             *y;
    /* Start address of U component (except padding)  */
    pel             *u;
    /* Start address of V component (except padding)  */
    pel             *v;
    /* Stride of luma picture */
    int              stride_luma;
    /* Stride of chroma picture */
    int              stride_chroma;
    /* Width of luma picture */
    int              width_luma;
    /* Height of luma picture */
    int              height_luma;
    /* Width of chroma picture */
    int              width_chroma;
    /* Height of chroma picture */
    int              height_chroma;
    /* padding size of luma */
    int              padsize_luma;
    /* padding size of chroma */
    int              padsize_chroma;
    /* image buffer */
    COM_IMGB        *imgb;
    /* decoding temporal reference of this picture */
    int              dtr;
    /* presentation temporal reference of this picture */
    int              ptr;
    int              picture_output_delay;
    /* 0: not used for reference buffer, reference picture type */
    u8               is_ref;
    /* needed for output? */
    u8               need_for_out;
    /* scalable layer id */
    u8               temporal_id;
    s16            (*map_mv)[REFP_NUM][MV_D];
    s8             (*map_refi)[REFP_NUM];
    u32              list_ptr[MAX_NUM_REF_PICS];
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
    u32              list_is_library_pic[MAX_NUM_REF_PICS];
#endif
} COM_PIC;

#if LIBVC_ON
#define MAX_NUM_COMPONENT                   3
#define MAX_NUM_LIBPIC                      16
#define MAX_CANDIDATE_PIC                   256
#define MAX_DIFFERENCE_OF_RLPIC_AND_LIBPIC  1.7e+308; ///< max. value of Double-type value

typedef struct _PIC_HIST_FEATURE
{
    int num_component;
    int num_of_hist_interval;
    int length_of_interval;
    int * list_hist_feature[MAX_NUM_COMPONENT];
} PIC_HIST_FEATURE;

// ====================================================================================================================
// Information of libpic
// ====================================================================================================================

typedef struct _LibVCData
{
    int num_candidate_pic;
    int list_poc_of_candidate_pic[MAX_CANDIDATE_PIC]; // for now, the candidate set contain only pictures from input sequence.
    COM_IMGB * list_candidate_pic[MAX_CANDIDATE_PIC];
    PIC_HIST_FEATURE list_hist_feature_of_candidate_pic[MAX_CANDIDATE_PIC];

    int num_lib_pic;
    int list_poc_of_libpic[MAX_NUM_LIBPIC];

    COM_PIC * list_libpic_outside[MAX_NUM_LIBPIC];
    int num_libpic_outside;
    int list_library_index_outside[MAX_NUM_LIBPIC];

    int num_RLpic;
    int list_poc_of_RLpic[MAX_CANDIDATE_PIC];
    int list_libidx_for_RLpic[MAX_CANDIDATE_PIC]; //the idx of library picture instead of poc of library picture in origin sequence is set as new poc.

    double bits_dependencyFile;
    double bits_libpic;

    int library_picture_enable_flag;
    int is_libpic_processing;
    int is_libpic_prepared;

} LibVCData;
#endif

/*****************************************************************************
 * picture buffer allocator
 *****************************************************************************/
typedef struct _PICBUF_ALLOCATOR PICBUF_ALLOCATOR;
struct _PICBUF_ALLOCATOR
{
    /* width */
    int              width;
    /* height */
    int              height;
    /* pad size for luma */
    int              pad_l;
    /* pad size for chroma */
    int              pad_c;
    /* arbitrary data, if needs */
    int              ndata[4];
    /* arbitrary address, if needs */
    void            *pdata[4];
};

/*****************************************************************************
 * picture manager for DPB in decoder and RPB in encoder
 *****************************************************************************/
typedef struct _COM_PM
{
    /* picture store (including reference and non-reference) */
    COM_PIC          *pic[MAX_PB_SIZE];
    /* address of reference pictures */
    COM_PIC          *pic_ref[MAX_NUM_REF_PICS];
    /* maximum reference picture count */
    int               max_num_ref_pics;
    /* current count of available reference pictures in PB */
    int               cur_num_ref_pics;
    /* number of reference pictures */
    int               num_refp[REFP_NUM];
    /* next output POC */
    int               ptr_next_output;
    /* POC increment */
    int               ptr_increase;
    /* max number of picture buffer */
    int               max_pb_size;
    /* current picture buffer size */
    int               cur_pb_size;
    /* address of leased picture for current decoding/encoding buffer */
    COM_PIC          *pic_lease;
    /* picture buffer allocator */
    PICBUF_ALLOCATOR  pa;
#if LIBVC_ON
    /* information for LibVC */
    LibVCData        *libvc_data;
    int               is_library_buffer_empty;
    COM_PIC          *pb_libpic; //buffer for libpic
    int               cur_libpb_size;
    int               pb_libpic_library_index;
#endif
} COM_PM;

/* reference picture structure */
typedef struct _COM_REFP
{
    /* address of reference picture */
    COM_PIC         *pic;
    /* PTR of reference picture */
    int              ptr;
    s16            (*map_mv)[REFP_NUM][MV_D];
    s8             (*map_refi)[REFP_NUM];
    u32             *list_ptr;
#if LIBVC_ON
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
    u32             *list_is_library_pic;
#endif
    int              is_library_picture;
#endif
} COM_REFP;

/*****************************************************************************
 * chunk header
 *****************************************************************************/
typedef struct _COM_CNKH
{
    /* version: 3bit */
    int              ver;
    /* chunk type: 4bit */
    int              ctype;
    /* broken link flag: 1bit(should be zero) */
    int              broken;
} COM_CNKH;

/*****************************************************************************
 * sequence header
 *****************************************************************************/
typedef struct _COM_SQH
{
    u8               profile_id;                 /* 8 bits */
    u8               level_id;                   /* 8 bits */
    u8               progressive_sequence;       /* 1 bit  */
    u8               field_coded_sequence;       /* 1 bit  */
#if LIBVC_ON
    u8               library_stream_flag;     /* 1 bit  */
    u8               library_picture_enable_flag;     /* 1 bit  */
    u8               duplicate_sequence_header_flag;     /* 1 bit  */
#endif
    u8               chroma_format;              /* 2 bits */
    u8               encoding_precision;         /* 3 bits */
    u8               output_reorder_delay;       /* 5 bits */
    u8               sample_precision;           /* 3 bits */
    u8               aspect_ratio;               /* 4 bits */
    u8               frame_rate_code;            /* 4 bits */
    u32              bit_rate_lower;             /* 18 bits */
    u32              bit_rate_upper;             /* 18 bits */
    u8               low_delay;                  /* 1 bit */
    u8               temporal_id_enable_flag;    /* 1 bit */
    u32              bbv_buffer_size;            /* 18 bits */
    u8               max_dpb_size;               /* 4 bits */
    int              horizontal_size;            /* 14 bits */
    int              vertical_size;              /* 14 bits */
    u8               log2_max_cu_width_height;   /* 3 bits */
    u8               min_cu_size;
    u8               max_part_ratio;
    u8               max_split_times;
    u8               min_qt_size;
    u8               max_bt_size;
    u8               max_eqt_size;
    u8               max_dt_size;
#if HLS_RPL
    int              rpl1_index_exist_flag;
    int              rpl1_same_as_rpl0_flag;
    COM_RPL          rpls_l0[MAX_NUM_RPLS];
    COM_RPL          rpls_l1[MAX_NUM_RPLS];
    int              rpls_l0_num;
    int              rpls_l1_num;
    int              num_ref_default_active_minus1[2];
#endif
#if IPCM
    int              ipcm_enable_flag;
#endif
    u8               amvr_enable_flag;
    int              umve_enable_flag;
    int              ipf_enable_flag;
#if EXT_AMVR_HMVP
    int              emvr_enable_flag;
#endif
    u8               affine_enable_flag;
#if SMVD
    u8               smvd_enable_flag;
#endif
#if DT_PARTITION
    u8               dt_intra_enable_flag;
#endif
    u8               num_of_hmvp_cand;
#if TSCPM
    u8               tscpm_enable_flag;
#endif

    u8               sample_adaptive_offset_enable_flag;
    u8               adaptive_leveling_filter_enable_flag;
    u8               secondary_transform_enable_flag;
    u8               position_based_transform_enable_flag;

    u8               wq_enable;
    u8               seq_wq_mode;
    u8               wq_4x4_matrix[16];
    u8               wq_8x8_matrix[64];
#if PATCH
    u8               patch_stable;
    u8               cross_patch_loop_filter;
    u8               patch_ref_colocated;
    u8               patch_uniform;
    u8               patch_width_minus1;
    u8               patch_height_minus1;
    u8               patch_columns;
    u8               patch_rows;
    int              column_width[64];
    int              row_height[32];
#endif
} COM_SQH;


/*****************************************************************************
 * picture header
 *****************************************************************************/
typedef struct _COM_PIC_HEADER
{
    /* info from sqh */
    u8  low_delay;
    /* decoding temporal reference */
#if HLS_RPL
    s32 poc;
    int              rpl_l0_idx;         //-1 means this slice does not use RPL candidate in SPS for RPL0
    int              rpl_l1_idx;         //-1 means this slice does not use RPL candidate in SPS for RPL1
    COM_RPL          rpl_l0;
    COM_RPL          rpl_l1;
    u8               num_ref_idx_active_override_flag;
    u8               ref_pic_list_sps_flag[2];
#endif
    u32              dtr;
    u8               slice_type;
    u8               temporal_id;
    u8               loop_filter_disable_flag;
    u32              bbv_delay;
    u16              bbv_check_times;

    u8               loop_filter_parameter_flag;
    int              alpha_c_offset;
    int              beta_offset;

    u8               chroma_quant_param_disable_flag;
    s8               chroma_quant_param_delta_cb;
    s8               chroma_quant_param_delta_cr;

    /*Flag and coeff for ALF*/
    int              tool_alf_on;
    int             *pic_alf_on;
    ALFParam       **m_alfPictureParam;

    int              fixed_picture_qp_flag;
    int              random_access_decodable_flag;
    int              time_code_flag;
    int              time_code;
    int              decode_order_index;
    int              picture_output_delay;
    int              progressive_frame;
    int              picture_structure;
    int              top_field_first;
    int              repeat_first_field;
    int              top_field_picture_flag;
    int              picture_qp;
    int              affine_subblock_size_idx;
#if LIBVC_ON
    int              is_RLpic_flag;  // only reference libpic
    int              library_picture_index;
#endif
    int              pic_wq_enable;
    int              pic_wq_data_idx;
    int              wq_param;
    int              wq_model;
    int              wq_param_vector[6];
    u8               wq_4x4_matrix[16];
    u8               wq_8x8_matrix[64];
} COM_PIC_HEADER;

typedef struct _COM_SH_EXT
{
    u8               slice_sao_enable[N_C];
    u8               fixed_slice_qp_flag;
    u8               slice_qp;
} COM_SH_EXT;

#if TB_SPLIT_EXT
typedef struct _COM_PART_INFO
{
    u8 num_sub_part;
    int sub_x[MAX_NUM_PB]; //sub part x, y, w and h
    int sub_y[MAX_NUM_PB];
    int sub_w[MAX_NUM_PB];
    int sub_h[MAX_NUM_PB];
    int sub_scup[MAX_NUM_PB];
} COM_PART_INFO;
#endif

/*****************************************************************************
 * user data types
 *****************************************************************************/
#define COM_UD_PIC_SIGNATURE              0x10
#define COM_UD_END                        0xFF

/*****************************************************************************
 * for binary and triple tree structure
 *****************************************************************************/
typedef enum _SPLIT_MODE
{
    NO_SPLIT        = 0,
    SPLIT_BI_VER    = 1,
    SPLIT_BI_HOR    = 2,
#if EQT
    SPLIT_EQT_VER   = 3,
    SPLIT_EQT_HOR   = 4,
    SPLIT_QUAD      = 5,
#else
    SPLIT_QUAD      = 3,
#endif
    NUM_SPLIT_MODE
} SPLIT_MODE;

typedef enum _SPLIT_DIR
{
    SPLIT_VER = 0,
    SPLIT_HOR = 1,
    SPLIT_QT = 2,
} SPLIT_DIR;

#if MODE_CONS
typedef enum _CONS_PRED_MODE
{
    NO_MODE_CONS = 0,
    ONLY_INTER = 1,
    ONLY_INTRA = 2,
} CONS_PRED_MODE;
#endif

#if CHROMA_NOT_SPLIT
typedef enum _TREE_STATUS
{
    TREE_LC = 0,
    TREE_L  = 1,
    TREE_C  = 2,
} TREE_STATUS;
#endif

typedef enum _CHANNEL_TYPE
{
    CHANNEL_LC = 0,
    CHANNEL_L  = 1,
    CHANNEL_C  = 2,
} CHANNEL_TYPE;

#if TB_SPLIT_EXT
typedef enum _PART_SIZE
{
    SIZE_2Nx2N,           ///< symmetric partition,  2Nx2N
    SIZE_2NxnU,           ///< asymmetric partition, 2Nx( N/2) + 2Nx(3N/2)
    SIZE_2NxnD,           ///< asymmetric partition, 2Nx(3N/2) + 2Nx( N/2)
    SIZE_nLx2N,           ///< asymmetric partition, ( N/2)x2N + (3N/2)x2N
    SIZE_nRx2N,           ///< asymmetric partition, (3N/2)x2N + ( N/2)x2N
    SIZE_NxN,             ///< symmetric partition, NxN
    SIZE_2NxhN,           ///< symmetric partition, 2N x 0.5N
    SIZE_hNx2N,           ///< symmetric partition, 0.5N x 2N
    NUM_PART_SIZE
} PART_SIZE;
#endif

typedef enum _BLOCK_SHAPE
{
    NON_SQUARE_18,
    NON_SQUARE_14,
    NON_SQUARE_12,
    SQUARE,
    NON_SQUARE_21,
    NON_SQUARE_41,
    NON_SQUARE_81,
    NUM_BLOCK_SHAPE,
} BLOCK_SHAPE;

typedef enum _CTX_NEV_IDX
{
    CNID_SKIP_FLAG,
    CNID_PRED_MODE,
    CNID_AFFN_FLAG,
    NUM_CNID,
} CTX_NEV_IDX;


/*****************************************************************************
* mode decision structure
*****************************************************************************/
typedef struct _COM_MODE
{
    /* CU position X in a frame in SCU unit */
    int            x_scu;
    /* CU position Y in a frame in SCU unit */
    int            y_scu;

    /* depth of current CU */
    int            cud;

    /* width of current CU */
    int            cu_width;
    /* height of current CU */
    int            cu_height;
    /* log2 of cu_width */
    int             cu_width_log2;
    /* log2 of cu_height */
    int             cu_height_log2;
    /* position of CU */
    int            x_pos;
    int            y_pos;
    /* CU position in current frame in SCU unit */
    int            scup;

    int  cu_mode;
#if TB_SPLIT_EXT
    //note: DT can apply to intra CU and only use normal amvp for inter CU (no skip, direct, amvr, affine, hmvp)
    int  pb_part;
    int  tb_part;
    COM_PART_INFO  pb_info;
    COM_PART_INFO  tb_info;
#endif

    pel  rec[N_C][MAX_CU_DIM];
    s16  coef[N_C][MAX_CU_DIM];
    pel  pred[N_C][MAX_CU_DIM];


#if TB_SPLIT_EXT
    int  num_nz[MAX_NUM_TB][N_C];
#else
    int  num_nz[N_C];
#endif

    /* reference indices */
    s8   refi[REFP_NUM];

    /* MVR indices */
    u8   mvr_idx;
    u8   umve_flag;
    u8   umve_idx;
    u8   skip_idx;
#if EXT_AMVR_HMVP
    u8   mvp_from_hmvp_flag;
#endif

    /* mv difference */
    s16  mvd[REFP_NUM][MV_D];
    /* mv */
    s16  mv[REFP_NUM][MV_D];

    u8   affine_flag;
    CPMV affine_mv[REFP_NUM][VER_NUM][MV_D];
    s16  affine_mvd[REFP_NUM][VER_NUM][MV_D];
#if SMVD
    u8   smvd_flag;
#endif

    /* intra prediction mode */
#if TB_SPLIT_EXT
    u8   mpm[MAX_NUM_PB][2];
    s8   ipm[MAX_NUM_PB][2];
#else
    u8   mpm[2];
    s8   ipm[2];
#endif
    u8   ipf_flag;
} COM_MODE;

/*****************************************************************************
* map structure
*****************************************************************************/
typedef struct _COM_MAP
{
    /* SCU map for CU information */
    u32                    *map_scu;
    /* LCU split information */
    s8                     (*map_split)[MAX_CU_DEPTH][NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU];
    /* decoded motion vector for every blocks */
    s16                    (*map_mv)[REFP_NUM][MV_D];
    /* reference frame indices */
    s8                     (*map_refi)[REFP_NUM];
    /* intra prediction modes */
    s8                     *map_ipm;
    u32                    *map_cu_mode;
#if TB_SPLIT_EXT
    u32                    *map_pb_tb_part;
#endif
    s8                     *map_depth;
    s8                     *map_delta_qp;
    s8                     *map_patch_idx;
} COM_MAP;

/*****************************************************************************
* common info
*****************************************************************************/
typedef struct _COM_INFO
{
    /* current chunk header */
    COM_CNKH                cnkh;

    /* current picture header */
    COM_PIC_HEADER          pic_header;

    /* current slice header */
    COM_SH_EXT              shext;

    /* sequence header */
    COM_SQH                 sqh;

    /* decoding picture width */
    int                     pic_width;
    /* decoding picture height */
    int                     pic_height;
    /* maximum CU width and height */
    int                     max_cuwh;
    /* log2 of maximum CU width and height */
    int                     log2_max_cuwh;

    /* picture width in LCU unit */
    int                     pic_width_in_lcu;
    /* picture height in LCU unit */
    int                     pic_height_in_lcu;

    /* picture size in LCU unit (= w_lcu * h_lcu) */
    int                     f_lcu;
    /* picture width in SCU unit */
    int                     pic_width_in_scu;
    /* picture height in SCU unit */
    int                     pic_height_in_scu;
    /* picture size in SCU unit (= pic_width_in_scu * h_scu) */
    int                     f_scu;

    int                     bit_depth_internal;
    int                     bit_depth_input;
    int                     qp_offset_bit_depth;
} COM_INFO;

typedef enum _MSL_IDX
{
    MSL_SKIP,  //skip
    MSL_MERG,  //merge or direct
    MSL_LIS0,  //list 0
    MSL_LIS1,  //list 1
    MSL_BI,    //bi pred
    NUM_MODE_SL,
} MSL_IDX;
/*****************************************************************************
* patch info
*****************************************************************************/
#if PATCH
typedef struct _PATCH_INFO
{
    int                     stable;
    int                     cross_patch_loop_filter;
    int                     ref_colocated;
    int                     uniform;
    int                     height;
    int                     width;
    int                     columns;
    int                     rows;
    int                     idx;
    /*patch's location(up left) in pixel*/
    int                     x_pel;
    int                     y_pel;
    /*all the cu in a patch are skip_mode*/
    int                     skip_patch_flag;
    /*pointer the width of each patch*/
    int                    *width_in_lcu;
    int                    *height_in_lcu;
    /*count as the patch*/
    int                     x_pat;
    int                     y_pat;
    /*define the boundary of a patch*/
    int                     left_pel;
    int                     right_pel;
    int                     up_pel;
    int                     down_pel;
    /*patch_sao_enable_flag info of all patches*/
    s8                     *patch_sao_enable;
} PATCH_INFO;
#endif
enum TRANS_TYPE
{
    DCT2,
    DCT8,
    DST7,
    NUM_TRANS_TYPE
};

#include "com_tbl.h"
#include "com_util.h"
#include "com_recon.h"
#include "com_ipred.h"
#include "com_tbl.h"
#include "com_itdq.h"
#include "com_picman.h"
#include "com_mc.h"
#include "com_img.h"

#endif /* _COM_DEF_H_ */
