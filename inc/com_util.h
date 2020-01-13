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

#ifndef _COM_UTIL_H_
#define _COM_UTIL_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "com_def.h"
#include <stdlib.h>

/*! macro to determine maximum */
#define COM_MAX(a,b)                   (((a) > (b)) ? (a) : (b))

/*! macro to determine minimum */
#define COM_MIN(a,b)                   (((a) < (b)) ? (a) : (b))

/*! macro to absolute a value */
#define COM_ABS(a)                     abs(a)

/*! macro to absolute a 64-bit value */
#define COM_ABS64(a)                   (((a)^((a)>>63)) - ((a)>>63))

/*! macro to absolute a 32-bit value */
#define COM_ABS32(a)                   (((a)^((a)>>31)) - ((a)>>31))

/*! macro to absolute a 16-bit value */
#define COM_ABS16(a)                   (((a)^((a)>>15)) - ((a)>>15))

/*! macro to clipping within min and max */
#define COM_CLIP3(min_x, max_x, value)   COM_MAX((min_x), COM_MIN((max_x), (value)))

/*! macro to clipping within min and max */
#define COM_CLIP(n,min,max)            (((n)>(max))? (max) : (((n)<(min))? (min) : (n)))

#define COM_SIGN(x)                    (((x) < 0) ? -1 : 1)

/*! macro to get a sign from a 16-bit value.\n
operation: if(val < 0) return 1, else return 0 */
#define COM_SIGN_GET(val)              ((val<0)? 1: 0)

/*! macro to set sign into a value.\n
operation: if(sign == 0) return val, else if(sign == 1) return -val */
#define COM_SIGN_SET(val, sign)        ((sign)? -val : val)

/*! macro to get a sign from a 16-bit value.\n
operation: if(val < 0) return 1, else return 0 */
#define COM_SIGN_GET16(val)            (((val)>>15) & 1)

/*! macro to set sign into a 16-bit value.\n
operation: if(sign == 0) return val, else if(sign == 1) return -val */
#define COM_SIGN_SET16(val, sign)      (((val) ^ ((s16)((sign)<<15)>>15)) + (sign))

#define COM_ALIGN(val, align)          ((((val) + (align) - 1) / (align)) * (align))

#define CONV_LOG2(v)                    (com_tbl_log2[v])

typedef struct _COM_MOTION
{
    s16 mv[REFP_NUM][MV_D];
    s8 ref_idx[REFP_NUM];
} COM_MOTION;

#define SAME_MV(MV0, MV1) ((MV0[MV_X] == MV1[MV_X]) && (MV0[MV_Y] == MV1[MV_Y]))
void copy_mv(s16 dst[MV_D], const s16 src[MV_D]);
void copy_motion_table(COM_MOTION *motion_dst, s8 *cnt_cands_dst, const COM_MOTION *motion_src, const s8 cnt_cands_src);
#if EXT_AMVR_HMVP
int same_motion(COM_MOTION motion1, COM_MOTION motion2);
#endif

u16 com_get_avail_intra(int x_scu, int y_scu, int pic_width_in_scu, int scup, u32 *map_scu);

COM_PIC * com_pic_alloc(PICBUF_ALLOCATOR * pa, int * ret);
void com_pic_free(PICBUF_ALLOCATOR *pa, COM_PIC *pic);
COM_PIC* com_picbuf_alloc(int w, int h, int pad_l, int pad_c, int *err);
void com_picbuf_free(COM_PIC *pic);
void com_picbuf_expand(COM_PIC *pic, int exp_l, int exp_c);

void check_mvp_motion_availability(COM_INFO *info, COM_MODE* mod_info_curr, COM_MAP *pic_map, int neb_addr[NUM_AVS2_SPATIAL_MV], int valid_flag[NUM_AVS2_SPATIAL_MV], int lidx);
void check_umve_motion_availability(COM_INFO *info, COM_MODE* mod_info_curr, COM_MAP *pic_map, int neb_addr[NUM_AVS2_SPATIAL_MV], int valid_flag[NUM_AVS2_SPATIAL_MV]);

void update_skip_candidates(COM_MOTION motion_cands[ALLOWED_HMVP_NUM], s8 *cands_num, const int max_hmvp_num, s16 mv_new[REFP_NUM][MV_D], s8 refi_new[REFP_NUM]);
void fill_skip_candidates(COM_MOTION motion_cands[ALLOWED_HMVP_NUM], s8 *num_cands, const int num_hmvp_cands, s16 mv_new[REFP_NUM][MV_D], s8 refi_new[REFP_NUM], int bRemDuplicate);
void get_hmvp_skip_cands(const COM_MOTION motion_cands[ALLOWED_HMVP_NUM], const u8 num_cands, s16(*skip_mvs)[REFP_NUM][MV_D], s8(*skip_refi)[REFP_NUM]);

void derive_MHBskip_spatial_motions(COM_INFO *info, COM_MODE* mod_info_curr, COM_MAP *pic_map, s16 skip_pmv[PRED_DIR_NUM][REFP_NUM][MV_D], s8 skip_refi[PRED_DIR_NUM][REFP_NUM]);

#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
void derive_umve_base_motions(COM_REFP(*refp)[REFP_NUM], COM_INFO *info, COM_MODE* mod_info_curr, COM_MAP *pic_map, s16 t_mv[REFP_NUM][MV_D], s8 t_refi[REFP_NUM], s16 umve_base_pmv[UMVE_BASE_NUM][REFP_NUM][MV_D], s8 umve_base_refi[UMVE_BASE_NUM][REFP_NUM]);
#else
void derive_umve_base_motions(COM_INFO *info, COM_MODE* mod_info_curr, COM_MAP *pic_map, s16 t_mv[REFP_NUM][MV_D], s8 t_refi[REFP_NUM], s16 umve_base_pmv[UMVE_BASE_NUM][REFP_NUM][MV_D], s8 umve_base_refi[UMVE_BASE_NUM][REFP_NUM]);
#endif
void derive_umve_final_motions(int umve_idx, COM_REFP(*refp)[REFP_NUM], int cur_poc, s16 umve_base_pmv[UMVE_BASE_NUM][REFP_NUM][MV_D], s8 umve_base_refi[UMVE_BASE_NUM][REFP_NUM], s16 umve_final_pmv[UMVE_BASE_NUM*UMVE_MAX_REFINE_NUM][REFP_NUM][MV_D], s8 umve_final_refi[UMVE_BASE_NUM*UMVE_MAX_REFINE_NUM][REFP_NUM]);

void com_get_mvp_default(COM_INFO *info, COM_MODE *mod_info_curr, COM_REFP(*refp)[REFP_NUM], COM_MAP *pic_map, int ptr_cur, int lidx, s8 cur_refi, u8 amvr_idx, s16 mvp[MV_D]);
void com_derive_mvp(COM_INFO info, COM_MODE *mod_info_curr, int ptr, int ref_list, int ref_idx, int cnt_hmvp_cands, COM_MOTION *motion_cands, COM_MAP map, COM_REFP(*refp)[REFP_NUM], int mvr_idx, s16 mvp[MV_D]);
enum
{
    SPLIT_MAX_PART_COUNT = 4
};

typedef struct _COM_SPLIT_STRUCT
{
    int       part_count;
    int       cud;
    int       width[SPLIT_MAX_PART_COUNT];
    int       height[SPLIT_MAX_PART_COUNT];
    int       log_cuw[SPLIT_MAX_PART_COUNT];
    int       log_cuh[SPLIT_MAX_PART_COUNT];
    int       x_pos[SPLIT_MAX_PART_COUNT];
    int       y_pos[SPLIT_MAX_PART_COUNT];
    int       cup[SPLIT_MAX_PART_COUNT];
} COM_SPLIT_STRUCT;

//! Count of partitions, correspond to split_mode
int com_split_part_count(int split_mode);
//! Get partition size
int com_split_get_part_size(int split_mode, int part_num, int length);
//! Get partition size log
int com_split_get_part_size_idx(int split_mode, int part_num, int length_idx);
//! Get partition split structure
void com_split_get_part_structure(int split_mode, int x0, int y0, int cu_width, int cu_height, int cup, int cud, int log2_culine, COM_SPLIT_STRUCT* split_struct);
//! Get array of split modes tried sequentially in RDO
void com_split_get_split_rdo_order(int cu_width, int cu_height, SPLIT_MODE splits[MAX_SPLIT_NUM]);
//! Get split direction. Quad will return vertical direction.
SPLIT_DIR com_split_get_direction(SPLIT_MODE mode);
#if EQT
//! Is mode triple tree?
int  com_split_is_EQT(SPLIT_MODE mode);
#endif
//! Is mode BT?
int  com_split_is_BT(SPLIT_MODE mode);
//! Check that mode is vertical
int com_split_is_vertical(SPLIT_MODE mode);
//! Check that mode is horizontal
int com_split_is_horizontal(SPLIT_MODE mode);

int get_colocal_scup(int scup, int pic_width_in_scu, int pic_height_in_scu);

void get_col_mv(COM_REFP refp[REFP_NUM], u32 ptr, int scup, s16 mvp[REFP_NUM][MV_D]);
void get_col_mv_from_list0(COM_REFP refp[REFP_NUM], u32 ptr, int scup, s16 mvp[REFP_NUM][MV_D]);

int com_scan_tbl_init();
int com_scan_tbl_delete();
int com_get_split_mode(s8* split_mode, int cud, int cup, int cu_width, int cu_height, int lcu_s
                       , s8(*split_mode_buf)[NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU]);
int com_set_split_mode(s8  split_mode, int cud, int cup, int cu_width, int cu_height, int lcu_s
                       , s8(*split_mode_buf)[NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU]);

#if MODE_CONS
u8   com_get_cons_pred_mode(int cud, int cup, int cu_width, int cu_height, int lcu_s, s8(*split_mode_buf)[NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU]);
void com_set_cons_pred_mode(u8 cons_pred_mode, int cud, int cup, int cu_width, int cu_height, int lcu_s, s8(*split_mode_buf)[NUM_BLOCK_SHAPE][MAX_CU_CNT_IN_LCU]);
#endif

void com_mv_rounding_s32(s32 hor, s32 ver, s32 * rounded_hor, s32 * rounded_ver, int right_shift, int left_shift);
void com_mv_rounding_s16(s32 hor, s32 ver, s16 * rounded_hor, s16 * rounded_ver, int right_shift, int left_shift);

void com_get_affine_mvp_scaling(COM_INFO *info, COM_MODE * mod_info_curr, COM_REFP(*refp)[REFP_NUM], COM_MAP *pic_map, int ptr, int lidx, \
    CPMV mvp[VER_NUM][MV_D], int vertex_num
#if BD_AFFINE_AMVR
    , u8 curr_mvr
#endif
);

int com_get_affine_memory_access(CPMV mv[VER_NUM][MV_D], int cu_width, int cu_height);

void com_set_affine_mvf(COM_INFO *info, COM_MODE *mod_info_curr, COM_REFP(*refp)[REFP_NUM], COM_MAP *pic_map);

int com_get_affine_merge_candidate(COM_INFO *info, COM_MODE *mod_info_curr, COM_REFP(*refp)[REFP_NUM], COM_MAP *pic_map,
    s8 mrg_list_refi[AFF_MAX_NUM_MRG][REFP_NUM], CPMV mrg_list_cpmv[AFF_MAX_NUM_MRG][REFP_NUM][VER_NUM][MV_D], int mrg_list_cp_num[AFF_MAX_NUM_MRG], int ptr);

/* MD5 structure */
typedef struct _COM_MD5
{
    u32     h[4]; /* hash state ABCD */
    u8      msg[64]; /*input buffer (chunk message) */
    u32     bits[2]; /* number of bits, modulo 2^64 (lsb first)*/
} COM_MD5;

/* MD5 Functions */
void com_md5_init(COM_MD5 * md5);
void com_md5_update(COM_MD5 * md5, void * buf, u32 len);
void com_md5_update_16(COM_MD5 * md5, void * buf, u32 len);
void com_md5_finish(COM_MD5 * md5, u8 digest[16]);
int com_md5_imgb(COM_IMGB * imgb, u8 digest[16]);

int com_picbuf_signature(COM_PIC * pic, u8 * md5_out);

int com_atomic_inc(volatile int * pcnt);
int com_atomic_dec(volatile int * pcnt);

void com_check_split_mode(COM_SQH* sqh, int *split_allow, int cu_width_log2, int cu_height_log2, int boundary, int boundary_b, int boundary_r, int log2_max_cuwh, int id
                          , const int parent_split, int qt_depth, int bet_depth, int slice_type);

#ifdef __cplusplus
}
#endif

void com_sbac_ctx_init(COM_SBAC_CTX *sbac_ctx);

#if DT_SYNTAX
int  com_dt_allow(int cu_w, int cu_h, int pred_mode, int max_dt_size);
#endif


#if TB_SPLIT_EXT
void init_tb_part(COM_MODE *mod_info_curr);
void init_pb_part(COM_MODE *mod_info_curr);
void set_pb_part(COM_MODE *mod_info_curr, int part_size);
void set_tb_part(COM_MODE *mod_info_curr, int part_size);
void get_part_info(int pic_width_in_scu, int x, int y, int w, int h, int part_size, COM_PART_INFO* sub_info);
int  get_part_idx(PART_SIZE part_size, int x, int y, int w, int h);
void update_intra_info_map_scu(u32 *map_scu, s8 *map_ipm, int tb_x, int tb_y, int tb_w, int tb_h, int pic_width_in_scu, int ipm);
#endif

int  com_ctx_tb_split(int pb_part_size);
int  get_part_num(PART_SIZE size);
int  get_part_num_tb_in_pb(PART_SIZE pb_part_size, int pb_part_idx);
int  get_tb_idx_offset(PART_SIZE pb_part_size, int pb_part_idx);
void get_tb_width_height_in_pb(int pb_w, int pb_h, PART_SIZE pb_part_size, int pb_part_idx, int *tb_w, int *tb_h);
void get_tb_pos_in_pb(int pb_x, int pb_y, PART_SIZE pb_part_size, int tb_w, int tb_h, int tb_part_idx, int *tb_x, int *tb_y);
int get_coef_offset_tb(int cu_x, int cu_y, int tb_x, int tb_y, int cu_w, int cu_h, int tb_part_size);
PART_SIZE get_tb_part_size_by_pb(PART_SIZE pb_part, int pred_mode);
void get_tb_width_height_log2(int log2_w, int log2_h, PART_SIZE part, int *log2_tb_w, int *log2_tb_h);
void get_tb_width_height(int w, int h, PART_SIZE part, int *tb_w, int *tb_h);
void get_tb_start_pos(int w, int h, PART_SIZE part, int idx, int *pos_x, int *pos_y);
int  is_tb_avaliable(COM_INFO info, int log2_w, int log2_h, PART_SIZE pb_part_size, int pred_mode);
int  is_cu_nz(int nz[MAX_NUM_TB][N_C]);
int  is_cu_plane_nz(int nz[MAX_NUM_TB][N_C], int plane);
void cu_plane_nz_cpy(int dst[MAX_NUM_TB][N_C], int src[MAX_NUM_TB][N_C], int plane);
void cu_plane_nz_cln(int dst[MAX_NUM_TB][N_C], int plane);
int is_cu_nz_equ(int dst[MAX_NUM_TB][N_C], int src[MAX_NUM_TB][N_C]);
void cu_nz_cln(int dst[MAX_NUM_TB][N_C]);
void check_set_tb_part(COM_MODE *mode);
void check_tb_part(COM_MODE *mode);
void copy_rec_y_to_pic(pel* src, int x, int y, int w, int h, int stride, COM_PIC *pic);

#if MODE_CONS
u8 com_constrain_pred_mode(int w, int h, SPLIT_MODE split, u8 slice_type);
#endif
#if CHROMA_NOT_SPLIT
u8 com_tree_split(int w, int h, SPLIT_MODE split, u8 slice_type);
#endif

#if LIBVC_ON
void init_libvcdata(LibVCData *libvc_data);
void delete_libvcdata(LibVCData *libvc_data);
int  get_libidx(LibVCData *libvc_data, int cur_ptr);
#endif

#endif /* _COM_UTIL_H_ */
