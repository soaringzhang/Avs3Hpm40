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

#include "com_def.h"
#include "com_recon.h"


void com_recon(PART_SIZE part, s16 *resi, pel *pred, int (*is_coef)[N_C], int plane, int cu_width, int cu_height, int s_rec, pel *rec, int bit_depth)
{
    int i, j;
    s16 t0;
    int k, part_num = get_part_num(part);
    int tb_height, tb_width;

    get_tb_width_height(cu_width, cu_height, part, &tb_width, &tb_height);

    for (k = 0; k < part_num; k++)
    {
        int tb_x, tb_y;
        pel *p, *r;
        s16 *c;

        get_tb_start_pos(cu_width, cu_height, part, k, &tb_x, &tb_y);

        p = pred + tb_y * cu_width + tb_x;
        r = rec  + tb_y * s_rec + tb_x;

        if (is_coef[k][plane] == 0) /* just copy pred to rec */
        {
            for (i = 0; i < tb_height; i++)
            {
                for (j = 0; j < tb_width; j++)
                {
                    r[i * s_rec + j] = COM_CLIP3(0, (1 << bit_depth) - 1, p[i * cu_width + j]);
                }
            }
        }
        else  /* add b/w pred and coef and copy it into rec */
        {
            c = resi + k * tb_width * tb_height;
            for (i = 0; i < tb_height; i++)
            {
                for (j = 0; j < tb_width; j++)
                {
                    t0 = c[i * tb_width + j] + p[i * cu_width + j];
                    r[i * s_rec + j] = COM_CLIP3(0, (1 << bit_depth) - 1, t0);
                }
            }
        }
    }
}



void com_recon_yuv(PART_SIZE part_size, int x, int y, int cu_width, int cu_height, s16 resi[N_C][MAX_CU_DIM], pel pred[N_C][MAX_CU_DIM], int (*num_nz_coef)[N_C], COM_PIC *pic, CHANNEL_TYPE channel, int bit_depth )
{
    pel * rec;
    int s_rec, off;

    /* Y */
    if (channel == CHANNEL_LC || channel == CHANNEL_L)
    {
        s_rec = pic->stride_luma;
        rec = pic->y + (y * s_rec) + x;
        com_recon(part_size, resi[Y_C], pred[Y_C], num_nz_coef, Y_C, cu_width, cu_height, s_rec, rec, bit_depth);
    }

    /* chroma */
    if (channel == CHANNEL_LC || channel == CHANNEL_C)
    {
        cu_width >>= 1;
        cu_height >>= 1;
        off = (x >> 1) + (y >> 1) * pic->stride_chroma;
        com_recon(SIZE_2Nx2N, resi[U_C], pred[U_C], num_nz_coef, U_C, cu_width, cu_height, pic->stride_chroma, pic->u + off, bit_depth);
        com_recon(SIZE_2Nx2N, resi[V_C], pred[V_C], num_nz_coef, V_C, cu_width, cu_height, pic->stride_chroma, pic->v + off, bit_depth);
    }
}



