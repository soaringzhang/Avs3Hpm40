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

#include "com_typedef.h"
#include "com_img.h"

static void imgb_delete(COM_IMGB * imgb)
{
    int i;
    com_assert_r(imgb);
    for(i=0; i<COM_IMGB_MAX_PLANE; i++)
    {
        if (imgb->buf_addr[i]) com_mfree(imgb->buf_addr[i]);
    }
    com_mfree(imgb);
}

static int imgb_addref(COM_IMGB * imgb)
{
    com_assert_rv(imgb, COM_ERR_INVALID_ARGUMENT);
    return com_atomic_inc(&imgb->refcnt);
}

static int imgb_getref(COM_IMGB * imgb)
{
    com_assert_rv(imgb, COM_ERR_INVALID_ARGUMENT);
    return imgb->refcnt;
}

static int imgb_release(COM_IMGB * imgb)
{
    int refcnt;
    com_assert_rv(imgb, COM_ERR_INVALID_ARGUMENT);
    refcnt = com_atomic_dec(&imgb->refcnt);
    if(refcnt == 0)
    {
        imgb_delete(imgb);
    }
    return refcnt;
}

COM_IMGB * com_imgb_create(int width, int height, int cs, int pad[COM_IMGB_MAX_PLANE], int align[COM_IMGB_MAX_PLANE])
{
    int i, p_size, a_size;
    COM_IMGB * imgb;
    imgb = (COM_IMGB *)com_malloc(sizeof(COM_IMGB));
    com_assert_rv(imgb, NULL);
    com_mset(imgb, 0, sizeof(COM_IMGB));

    if(cs == COM_COLORSPACE_YUV420)
    {
        const int scale = 2; // always allocate 10-bit image buffer
        for(i=0; i<3; i++)
        {
            imgb->width[i] = width;
            imgb->height[i] = height;
            a_size = (align != NULL)? align[i] : 0;
            p_size = (pad != NULL)? pad[i] : 0;
            imgb->width_aligned[i] = COM_ALIGN(width, a_size);
            imgb->height_aligned[i] = COM_ALIGN(height, a_size);
            imgb->pad_left[i] = imgb->pad_right[i]=imgb->pad_up[i]=imgb->pad_down[i]=p_size;
            imgb->stride[i] = (imgb->width_aligned[i] + imgb->pad_left[i] + imgb->pad_right[i]) * scale;
            imgb->buf_size[i] = imgb->stride[i]*(imgb->height_aligned[i] + imgb->pad_up[i] + imgb->pad_down[i]);
            imgb->buf_addr[i] = com_malloc(imgb->buf_size[i]);
            imgb->addr_plane[i] = ((u8*)imgb->buf_addr[i]) + imgb->pad_up[i]*imgb->stride[i] + imgb->pad_left[i]* scale;
            if(i == 0)
            {
                width = (width+1)>>1;
                height = (height+1)>>1;
            }
        }
        imgb->np = 3;
    }
    else
    {
        com_trace("unsupported color space\n");
        com_mfree(imgb);
        return NULL;
    }
    imgb->addref = imgb_addref;
    imgb->getref = imgb_getref;
    imgb->release = imgb_release;
    imgb->cs = cs;
    imgb->addref(imgb);
    return imgb;
}
