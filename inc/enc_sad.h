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

#ifndef _ENC_SAD_H_
#define _ENC_SAD_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "com_port.h"

typedef int  (*ENC_FN_SAD) (int w, int h, void *src1, void *src2, int s_src1, int s_src2, int bit_depth);
typedef int  (*ENC_FN_SATD)(int w, int h, void *src1, void *src2, int s_src1, int s_src2, int bit_depth);
typedef s64  (*ENC_FN_SSD) (int w, int h, void *src1, void *src2, int s_src1, int s_src2, int bit_depth);
typedef void (*ENC_FN_DIFF)(int w, int h, void *src1, void *src2, int s_src1, int s_src2, int s_diff, s16 *diff);

extern const ENC_FN_SAD enc_tbl_sad_16b[8][8];
#define enc_sad_16b(log2w, log2h, src1, src2, s_src1, s_src2, bit_depth)\
    enc_tbl_sad_16b[log2w][log2h](1<<(log2w), 1<<(log2h), src1, src2, s_src1, s_src2, bit_depth)

extern const ENC_FN_SATD enc_tbl_satd_16b[8][8];
#define enc_satd_16b(log2w, log2h, src1, src2, s_src1, s_src2, bit_depth)\
    enc_tbl_satd_16b[log2w][log2h](1<<(log2w), 1<<(log2h), src1, src2, s_src1, s_src2, bit_depth)

extern const ENC_FN_SSD enc_tbl_ssd_16b[8][8];
#define enc_ssd_16b(log2w, log2h, src1, src2, s_src1, s_src2, bit_depth)\
    enc_tbl_ssd_16b[log2w][log2h](1<<(log2w), 1<<(log2h), src1, src2, s_src1, s_src2, bit_depth)

extern const ENC_FN_DIFF enc_tbl_diff_16b[8][8];
#define enc_diff_16b(log2w, log2h, src1, src2, s_src1, s_src2, s_diff, diff) \
    enc_tbl_diff_16b[log2w][log2h](1<<(log2w), 1<<(log2h), src1, src2, s_src1, s_src2, s_diff, diff)


#ifdef __cplusplus
}
#endif

#endif /* _ENC_SAD_H_ */
