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

#include "com_util.h"
#include "com_df.h"
#include "enc_def.h"
#include "enc_util.h"

/******************************************************************************
 * picture buffer alloc/free/expand
 ******************************************************************************/
void enc_picbuf_expand(ENC_CTX *ctx, COM_PIC *pic)
{
    com_picbuf_expand(pic, pic->padsize_luma, pic->padsize_chroma);
}

/******************************************************************************
 * implementation of bitstream writer
 ******************************************************************************/
void enc_bsw_skip_slice_size(COM_BSW *bs)
{
    com_bsw_write(bs, 0, 32);
}

void enc_bsw_write_slice_size(COM_BSW *bs)
{
    u32 size;
    size = COM_BSW_GET_WRITE_BYTE(bs) - 4;
    bs->beg[0] = (u8)((size & 0xff000000) >> 24);
    bs->beg[1] = (u8)((size & 0x00ff0000) >> 16);
    bs->beg[2] = (u8)((size & 0x0000ff00) >>  8);
    bs->beg[3] = (u8)((size & 0x000000ff) >>  0);
}

void enc_diff_pred(int x, int y, int cu_width_log2, int cu_height_log2, COM_PIC *org, pel pred[N_C][MAX_CU_DIM], s16 diff[N_C][MAX_CU_DIM])
{
    pel * buf;
    int cu_width, cu_height, stride;
    cu_width   = 1 << cu_width_log2;
    cu_height   = 1 << cu_height_log2;
    stride = org->stride_luma;
    /* Y */
    buf = org->y  + (y * stride) + x;
    enc_diff_16b(cu_width_log2, cu_height_log2, buf, pred[Y_C], stride, cu_width, cu_width, diff[Y_C]);
    cu_width  >>= 1;
    cu_height  >>= 1;
    x >>= 1;
    y >>= 1;
    cu_width_log2--;
    cu_height_log2--;
    stride = org->stride_chroma;
    /* U */
    buf = org->u  + (y * stride) + x;
    enc_diff_16b(cu_width_log2, cu_height_log2, buf, pred[U_C], stride, cu_width, cu_width, diff[U_C]);
    /* V */
    buf = org->v  + (y * stride) + x;
    enc_diff_16b(cu_width_log2, cu_height_log2, buf, pred[V_C], stride, cu_width, cu_width, diff[V_C]);
}