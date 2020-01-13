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

#ifndef _DEC_BSR_H_
#define _DEC_BSR_H_

#include "com_port.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _COM_BSR COM_BSR;

/*! Function pointer for */
typedef int (*COM_BSR_FN_FLUSH)(COM_BSR *bs, int byte);

/*!
 * bitstream structure for decoder.
 *
 * NOTE: Don't change location of variable because this variable is used
 *       for assembly coding!
 */
struct _COM_BSR
{
    /* temporary read code buffer */
    u32                code;
    /* left bits count in code */
    int                leftbits;
    /*! address of current bitstream position */
    u8               * cur;
    /*! address of bitstream end */
    u8               * end;
    /*! address of bitstream begin */
    u8               * beg;
    /*! size of original bitstream in byte */
    int                size;
    /*! Function pointer for bs_flush */
    COM_BSR_FN_FLUSH  fn_flush;
    /*! arbitrary data, if needs */
    int                ndata[4];
    /*! arbitrary address, if needs */
    void             * pdata[4];
};

#if defined(X86F)
/* on X86 machine, 32-bit shift means remaining of original value, so we
should set zero in that case. */
#define COM_BSR_SKIP_CODE(bs, size) \
    com_assert((bs)->leftbits >= (size)); \
    if((size) == 32) {(bs)->code = 0; (bs)->leftbits = 0;} \
    else           {(bs)->code <<= (size); (bs)->leftbits -= (size);}
#else
#define COM_BSR_SKIP_CODE(bs, size) \
    com_assert((bs)->leftbits >= (size)); \
    (bs)->code <<= (size); (bs)->leftbits -= (size);
#endif

/* get number of byte consumed */
#define COM_BSR_GET_READ_BYTE(bs) \
    ((int)((bs)->cur - (bs)->beg) - ((bs)->leftbits >> 3))

void com_bsr_init(COM_BSR * bs, u8 * buf, int size, COM_BSR_FN_FLUSH fn_flush);
int com_bsr_flush(COM_BSR * bs, int byte);
u32 com_bsr_read(COM_BSR * bs, int size);
u32 com_bsr_next(COM_BSR * bs, int size);
int com_bsr_read1(COM_BSR * bs);
int com_bsr_clz_in_code(u32 code);
u32 com_bsr_read_ue(COM_BSR * bs);
int com_bsr_read_se(COM_BSR * bs);

#ifdef __cplusplus
}
#endif

#endif /* _DEC_BSR_H_ */
