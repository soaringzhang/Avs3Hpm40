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

#ifndef _ENC_BSW_H_
#define _ENC_BSW_H_

#include "com_port.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _COM_BSW COM_BSW;

/*! Function pointer for */
typedef int (*COM_BSW_FN_FLUSH)(COM_BSW *bs);

/*! Bitstream structure for encoder */
struct _COM_BSW
{
    /* buffer */
    u32                code;
    /* bits left in buffer */
    int                leftbits;
    /*! address of current writing position */
    u8               * cur;
    /*! address of bitstream buffer end */
    u8               * end;
    /*! address of bitstream buffer begin */
    u8               * beg;
    /*! address of temporal buffer for demulation begin */
    u8               * buftmp;
    /*! size of bitstream buffer in byte */
    int                size;
    /*! address of function for flush */
    COM_BSW_FN_FLUSH  fn_flush;
    /*! arbitrary data, if needs */
    int                 ndata[4];
    /*! arbitrary address, if needs */
    void              * pdata[4];
};

/* get number of byte written */
#define COM_BSW_GET_WRITE_BYTE(bs)    (int)((bs)->cur - (bs)->beg)

/* number of bytes to be sunk */
#define COM_BSW_GET_SINK_BYTE(bs)     ((32 - (bs)->leftbits + 7) >> 3)

void com_bsw_init(COM_BSW * bs, u8 * buf, u8 * buftmp, int size, COM_BSW_FN_FLUSH fn_flush);
void com_bsw_deinit(COM_BSW * bs);

int com_bsw_write1(COM_BSW * bs, int val);
int com_bsw_write(COM_BSW * bs, u32 val, int len);

void com_bsw_write_ue(COM_BSW * bs, u32 val);
void com_bsw_write_se(COM_BSW * bs, int val);

#ifdef __cplusplus
}
#endif

#endif /* _ENC_BSW_H_ */
