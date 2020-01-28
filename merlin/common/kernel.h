
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
double analyze_intra_cu(ENC_CTX *ctx, ENC_CORE *core);

#ifdef USE_KERNEL_IPRED
#pragma ACCEL kernel
#else 
#define com_ipred_m com_ipred
#endif 
void KERNEL(com_ipred)(pel *src_le, pel *src_up, pel *dst, int ipm, int w, int h, 
    int bit_depth, u16 avail_cu, u8 ipf_flag);


#endif //__KERNEL_H_INCLUDED__



