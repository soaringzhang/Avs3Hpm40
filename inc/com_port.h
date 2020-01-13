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

#ifndef _COM_PORT_H_
#define _COM_PORT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/*****************************************************************************
 * types
 *****************************************************************************/
#if defined(WIN32) || defined(WIN64)
typedef __int8                     s8;
typedef unsigned __int8            u8;
typedef __int16                    s16;
typedef unsigned __int16           u16;
typedef __int32                    s32;
typedef unsigned __int32           u32;
typedef __int64                    s64;
typedef unsigned __int64           u64;
#elif defined(LINUX)
#include <stdint.h>
typedef int8_t                     s8;
typedef uint8_t                    u8;
typedef int16_t                    s16;
typedef uint16_t                   u16;
typedef int32_t                    s32;
typedef uint32_t                   u32;
typedef int64_t                    s64;
typedef uint64_t                   u64;
#else
typedef signed char                s8;
typedef unsigned char              u8;
typedef signed short               s16;
typedef unsigned short             u16;
typedef signed int                 s32;
typedef unsigned int               u32;
#if defined(X86_64) && !defined(_MSC_VER) /* for 64bit-Linux */
typedef signed long                s64;
typedef unsigned long              u64;
#else
typedef signed long long           s64;
typedef unsigned long long         u64;
#endif
#endif

typedef s16                        pel; /* pixel type */
typedef s32                        double_pel; /* pixel type */


#ifndef NULL
#define NULL                      (void*)0
#endif

/*****************************************************************************
 * limit constant
 *****************************************************************************/
#define COM_UINT16_MAX          ((u16)0xFFFF)
#define COM_UINT16_MIN          ((u16)0x0)
#define COM_INT16_MAX           ((s16)0x7FFF)
#define COM_INT16_MIN           ((s16)0x8000)

#define COM_UINT_MAX            ((u32)0xFFFFFFFF)
#define COM_UINT_MIN            ((u32)0x0)
#define COM_INT_MAX             ((int)0x7FFFFFFF)
#define COM_INT_MIN             ((int)0x80000000)

#define COM_UINT32_MAX          ((u32)0xFFFFFFFF)
#define COM_UINT32_MIN          ((u32)0x0)
#define COM_INT32_MAX           ((s32)0x7FFFFFFF)
#define COM_INT32_MIN           ((s32)0x80000000)

#define COM_UINT64_MAX          ((u64)0xFFFFFFFFFFFFFFFFL)
#define COM_UINT64_MIN          ((u64)0x0L)
#define COM_INT64_MAX           ((s64)0x7FFFFFFFFFFFFFFFL)
#define COM_INT64_MIN           ((s64)0x8000000000000000L)

#define COM_INT18_MAX           ((s32)(131071))
#define COM_INT18_MIN           ((s32)(-131072))

/*****************************************************************************
 * memory operations
 *****************************************************************************/
#define com_malloc(size)          malloc((size))
#define com_malloc_fast(size)     com_malloc((size))

#define com_mfree(m)              if(m){free(m);}
#define com_mfree_fast(m)         if(m){com_mfree(m);}

#define com_mcpy(dst,src,size)    memcpy((dst), (src), (size))
#define com_mset(dst,v,size)      memset((dst), (v), (size))
#define com_mset_x64a(dst,v,size) memset((dst), (v), (size))
#define com_mset_x128(dst,v,size) memset((dst), (v), (size))
#define com_mcmp(dst,src,size)    memcmp((dst), (src), (size))
static __inline void com_mset_16b(s16 * dst, s16 v, int cnt)
{
    int i;
    for(i=0; i<cnt; i++)
        dst[i] = v;
}


/*****************************************************************************
 * trace and assert
 *****************************************************************************/
#ifndef COM_TRACE
#define COM_TRACE               0
#endif

/* print function */
#if defined(LINUX)
#define com_print(args...) printf(args)
#else
#define com_print(args,...) printf(args,__VA_ARGS__)
#endif

/* trace function */
#if COM_TRACE
#define com_trace com_print("[%s:%d] ", __FILE__, __LINE__); com_print
#else
#if defined(LINUX)
#define com_trace(args...) {}
#else
#define com_trace(...) {}
#endif
#endif

/* assert function */
#include <assert.h>
#define com_assert(x) \
    {if(!(x)){assert(x);}}
#define com_assert_r(x) \
    {if(!(x)){assert(x); return;}}
#define com_assert_rv(x,r) \
    {if(!(x)){assert(x); return (r);}}
#define com_assert_g(x,g) \
    {if(!(x)){assert(x); goto g;}}
#define com_assert_gv(x,r,v,g) \
    {if(!(x)){assert(x); (r)=(v); goto g;}}

#ifdef X86_SSE
#if defined(WIN32) || defined(WIN64)
#include <emmintrin.h>
#include <xmmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#else
#include <x86intrin.h>
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* _COM_PORT_H_ */
