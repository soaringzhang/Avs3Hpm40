
#ifndef __KERNEL_H_INCLUDED__
#define __KERNEL_H_INCLUDED__


#ifdef MCC_ACC
#define KERNEL(x) x##_m
#else
#define KERNEL(x) x
#endif

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef USE_KERNEL_INTRA_TOP
#pragma ACCEL kernel
#else 
#define analyze_intra_cu_m analyze_intra_cu
#endif 
double KERNEL(analyze_intra_cu)(ENC_CTX *ctx, ENC_CORE *core);

#ifdef USE_KERNEL_IPRED
#pragma ACCEL kernel
#else 
#define com_ipred_m com_ipred
#endif 
void KERNEL(com_ipred)(pel *src_le, pel *src_up, pel *dst, int ipm, int w, int h, 
    int bit_depth, u16 avail_cu, u8 ipf_flag);


#ifdef USE_KERNEL_IPRED_UV
#pragma ACCEL kernel
#else 
#define com_ipred_uv_m com_ipred_uv
#endif 
void KERNEL(com_ipred_uv)(pel *src_le, pel *src_up, pel *dst, int ipm_c, int ipm, int w, int h, int bit_depth, u16 avail_cu
#if TSCPM
                  , int chType, pel *piRecoY, int uiStrideY, pel nb[N_C][N_REF][MAX_CU_SIZE * 3]
#endif
                 );

#ifdef USE_KERNEL_IPRED_RDO
#pragma ACCEL kernel
#else 
#define pintra_residue_rdo_m pintra_residue_rdo
#endif 
double KERNEL(pintra_residue_rdo)(ENC_CTX *ctx, ENC_CORE *core, pel *org_luma, pel *org_cb, pel *org_cr, int s_org, int s_org_c, int cu_width_log2, int cu_height_log2,
                                 s32 *dist, int bChroma, int pb_idx, int x, int y);




#endif //__KERNEL_H_INCLUDED__



