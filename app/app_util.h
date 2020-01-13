/* ====================================================================================================================

  The copyright in this software is being made available under the License included below.
  This software may be subject to other third party and contributor rights, including patent rights, and no such
  rights are granted under this license.

  Copyright (c) 2018, HUAWEI TECHNOLOGIES CO., LTD. All rights reserved.
  Copyright (c) 2018, SAMSUNG ELECTRONICS CO., LTD. All rights reserved.

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

#ifndef _IFVCA_UTIL_H_
#define _IFVCA_UTIL_H_
#include <stdlib.h>
#include <assert.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* for support  16bit aligned input source */
#define APP_PEL_16BIT
#ifdef APP_PEL_16BIT
#define CONV8_16BIT
#endif

#ifndef RECON_POC_ORDER
/* 0: let reconstructed pictures order to decoding order*/
/* 1: let reconstructed pictures order to display order*/
#define RECON_POC_ORDER            1
#endif

#define USE_CPU_TIME_CLOCK         1

#include <stdio.h>
#include <string.h>
#include <time.h>

/* print function ************************************************************/
#if defined(LINUX)
#define print(args...) printf(args)
#else
#define print(args, ...) printf(args, __VA_ARGS__)
#endif

#define VERBOSE_0                  0
#define VERBOSE_1                  1
#define VERBOSE_2                  2

static int op_verbose = VERBOSE_1;


#if defined(LINUX)
#define v0print(args...) {if(op_verbose >= VERBOSE_0) {print(args);}}
#define v1print(args...) {if(op_verbose >= VERBOSE_1) {print(args);}}
#define v2print(args...) {if(op_verbose >= VERBOSE_2) {print(args);}}
#define print_flush(stream)  fflush(stream);
#else
#define v0print(args,...) {if(op_verbose >= VERBOSE_0){print(args,__VA_ARGS__);}}
#define v1print(args,...) {if(op_verbose >= VERBOSE_1){print(args,__VA_ARGS__);}}
#define v2print(args,...) {if(op_verbose >= VERBOSE_2){print(args,__VA_ARGS__);}}
#define print_flush(stream)  fflush(stream);
#endif
/****************************************************************************/
#if defined(WIN32) || defined(WIN64)
#include <windows.h>

#define COM_CLK             DWORD
#define COM_CLK_PER_SEC     (1000)
#define COM_CLK_PER_MSEC    (1)
#define COM_CLK_MAX         ((COM_CLK)(-1))
#if USE_CPU_TIME_CLOCK
#define com_clk_get()       clock()
#else
#define com_clk_get()       GetTickCount()
#endif
/****************************************************************************/
#elif defined(LINUX) || defined(ANDROID)
#include <time.h>
#include <sys/time.h>
#define COM_CLK             unsigned long
#define COM_CLK_MAX         ((COM_CLK)(-1))
#define COM_CLK_PER_SEC     (10000)
#define COM_CLK_PER_MSEC    (10)
static COM_CLK com_clk_get(void)
{
    COM_CLK clk;
#if USE_CPU_TIME_CLOCK
    clk = (COM_CLK)(clock()) * 10000L / CLOCKS_PER_SEC;
#else
    struct timeval t;
    gettimeofday(&t, NULL);
    clk = t.tv_sec*10000L + t.tv_usec/100L;
#endif
    return clk;
}
/* The others ***************************************************************/
#else
#error THIS PLATFORM CANNOT SUPPORT CLOCK.
#endif

#define com_clk_diff(t1, t2) \
    (((t2) >= (t1)) ? ((t2) - (t1)) : ((COM_CLK_MAX - (t1)) + (t2)))

static COM_CLK com_clk_from(COM_CLK from) \
{
    COM_CLK now = com_clk_get();
    \
    return com_clk_diff(from, now);
    \
}


#define com_clk_msec(clk) \
    ((int)((clk + (COM_CLK_PER_MSEC/2))/COM_CLK_PER_MSEC))
#define com_clk_sec(clk)  \
    ((int)((clk + (COM_CLK_PER_SEC/2))/COM_CLK_PER_SEC))

#define IFVCA_CLIP(n,min,max) (((n)>(max))? (max) : (((n)<(min))? (min) : (n)))

static int skip_frames(FILE * fp, COM_IMGB * img, int num_skipped_frames, int bit_depth)
{
    int y_size, u_size, v_size, skipped_size;
    int f_w = img->width[0];
    int f_h = img->height[0];
    if (num_skipped_frames <= 0)
    {
        return 0;
    }
    if (img->cs == COM_COLORSPACE_YUV420)
    {
        int scale = (bit_depth == 10 ? 2 : 1);
        y_size = f_w * f_h * scale;
        u_size = v_size = (f_w >> 1) * (f_h >> 1) * scale;
        skipped_size = (y_size + u_size + v_size) * num_skipped_frames;
        fseek(fp, skipped_size, SEEK_CUR);
    }
    else
    {
        print("not supported color space\n");
        return -1;
    }
    return 0;
}

static int imgb_write(char * fname, COM_IMGB * img, int bit_depth)
{
    unsigned char * p8;
    int             i, j;
    FILE          * fp;
    int scale = (bit_depth == 10 ? 2 : 1);
    fp = fopen(fname, "ab");
    if(fp == NULL)
    {
        print("cannot open file = %s\n", fname);
        return -1;
    }

    assert(img->cs == COM_COLORSPACE_YUV420);

    for(i = 0; i < 3; i++)
    {
        p8 = (unsigned char *)img->addr_plane[i];
        for(j = 0; j < img->height[i]; j++)
        {
            fwrite(p8, img->width[i] * scale, 1, fp);
            p8 += img->stride[i];
        }
    }
    fclose(fp);
    return 0;
}

static void __imgb_cpy_plane(void *src, void *dst, int bw, int h, int s_src, int s_dst)
{
    int i;
    unsigned char *s, *d;
    s = (unsigned char*)src;
    d = (unsigned char*)dst;
    for(i = 0; i < h; i++)
    {
        memcpy(d, s, bw);
        s += s_src;
        d += s_dst;
    }
}

static int write_data(char * fname, unsigned char * data, int size, int end_of_video_sequence)
{
    FILE * fp;
    fp = fopen(fname, "ab");
    if(fp == NULL)
    {
        v0print("cannot open an writing file=%s\n", fname);
        return -1;
    }

    fwrite(data, 1, size, fp);
    if (end_of_video_sequence == END_OF_VIDEO_SEQUENCE)
    {
        //0x000001B1
        unsigned char video_sequence_end_code;
        video_sequence_end_code = 0x00;
        fwrite(&video_sequence_end_code, 1, 1, fp);
        video_sequence_end_code = 0x00;
        fwrite(&video_sequence_end_code, 1, 1, fp);
        video_sequence_end_code = 0x01;
        fwrite(&video_sequence_end_code, 1, 1, fp);
        video_sequence_end_code = 0xB1;
        fwrite(&video_sequence_end_code, 1, 1, fp);
    }

    fclose(fp);
    return 0;
}

static void imgb_conv_8b_to_16b(COM_IMGB * imgb_dst, COM_IMGB * imgb_src,
                                int shift)
{
    int i, j, k;
    unsigned char * s;
    short         * d;
    for(i = 0; i < 3; i++)
    {
        s = imgb_src->addr_plane[i];
        d = imgb_dst->addr_plane[i];
        for(j = 0; j < imgb_src->height[i]; j++)
        {
            for(k = 0; k < imgb_src->width[i]; k++)
            {
                d[k] = (short)(s[k] << shift);
            }
            s = s + imgb_src->stride[i];
            d = (short*)(((unsigned char *)d) + imgb_dst->stride[i]);
        }
    }
}

static void imgb_conv_16b_to_8b(COM_IMGB * imgb_dst, COM_IMGB * imgb_src, int shift)
{
    int i, j, k, t0, add;
    short         * s;
    unsigned char * d;
    add = (shift > 0) ? (1 << (shift - 1)) : 0;
    for(i = 0; i < 3; i++)
    {
        s = imgb_src->addr_plane[i];
        d = imgb_dst->addr_plane[i];
        for(j = 0; j < imgb_src->height[i]; j++)
        {
            for(k = 0; k < imgb_src->width[i]; k++)
            {
                t0 = ((s[k] + add) >> shift);
                d[k] = (unsigned char)(IFVCA_CLIP(t0, 0, 255));
            }
            s = (short*)(((unsigned char *)s) + imgb_src->stride[i]);
            d = d + imgb_dst->stride[i];
        }
    }
}

static void imgb_cpy_internal(COM_IMGB * dst, COM_IMGB * src)
{
    int i;
    if (src->cs == dst->cs)
    {
        int scale = 2; // internal storage is always 16-bit short type
        for (i = 0; i < src->np; i++)
        {
            __imgb_cpy_plane(src->addr_plane[i], dst->addr_plane[i], scale*src->width[i], src->height[i],
                             src->stride[i], dst->stride[i]);
        }
    }
    else
    {
        assert(0);
    }

    for(i = 0; i < 4; i++)
    {
        dst->ts[i] = src->ts[i];
    }
}

static void imgb_cpy_conv_rec(COM_IMGB * dst, COM_IMGB * src, int bit_depth)
{
    int i;

    if (bit_depth == 10)
    {
        int scale = (bit_depth == 10 ? 2 : 1);
        for (i = 0; i < src->np; i++)
        {
            __imgb_cpy_plane(src->addr_plane[i], dst->addr_plane[i], scale*src->width[i], src->height[i],
                             src->stride[i], dst->stride[i]);
        }
    }
    else // the output reconstructed file is 8-bit storage for 8-bit depth
    {
        imgb_conv_16b_to_8b(dst, src, 0);
    }

    for (i = 0; i < 4; i++)
    {
        dst->ts[i] = src->ts[i];
    }
}

static void imgb_free(COM_IMGB * imgb)
{
    int i;
    for(i = 0; i < COM_IMGB_MAX_PLANE; i++)
    {
        if(imgb->buf_addr[i]) free(imgb->buf_addr[i]);
    }
    free(imgb);
}

COM_IMGB * imgb_alloc(int width, int height, int cs, int bitDepth)
{
    int i;
    COM_IMGB * imgb;
    imgb = (COM_IMGB *)malloc(sizeof(COM_IMGB));
    if(imgb == NULL)
    {
        v0print("cannot create image buffer\n");
        return NULL;
    }
    memset(imgb, 0, sizeof(COM_IMGB));
    if(cs == COM_COLORSPACE_YUV420)
    {
        for(i = 0; i < 3; i++)
        {
            imgb->width[i] = imgb->width_aligned[i] = width;
            imgb->stride[i] = width * (bitDepth == 8 ? 1: 2);
            imgb->height[i] = imgb->height_aligned[i] = height;
            imgb->buf_size[i] = imgb->stride[i] * height;
            imgb->addr_plane[i] = imgb->buf_addr[i] = malloc(imgb->buf_size[i]);
            if(imgb->addr_plane[i] == NULL)
            {
                v0print("cannot allocate picture buffer\n");
                return NULL;
            }
            if(i == 0)
            {
                width = (width + 1) >> 1;
                height = (height + 1) >> 1;
            }
        }
        imgb->np = 3;
    }
    else
    {
        v0print("unsupported color space\n");
        if(imgb)free(imgb);
        return NULL;
    }
    imgb->cs = cs;
    return imgb;
}

static int imgb_read_conv(FILE * fp, COM_IMGB * img, int bit_depth_input, int bit_depth_internal)
{
    int width_pic = img->width[0];
    int height_pic = img->height[0];
    int size_yuv[3];
    short *dst;
    int scale = (bit_depth_input == 10) ? 2 : 1;

    int bit_shift = bit_depth_internal - bit_depth_input;
    const short minval = 0;
    const short maxval = (1 << bit_depth_internal) - 1;

    if (img->cs == COM_COLORSPACE_YUV420)
    {
        int num_comp = 3;
        size_yuv[0] = width_pic * height_pic;
        size_yuv[1] = size_yuv[2] = (width_pic >> 1) * (height_pic >> 1);
        for (int comp = 0; comp < num_comp; comp++)
        {
            int padding_w = (img->width[0] - img->horizontal_size) >> (comp > 0 ? 1 : 0);
            int padding_h = (img->height[0] - img->vertical_size) >> (comp > 0 ? 1 : 0);
            int size_byte = ((img->horizontal_size * img->vertical_size) >> (comp > 0 ? 2 : 0)) * scale;
            unsigned char * buf = malloc(size_byte);
            if (fread(buf, 1, size_byte, fp) != (unsigned)size_byte)
            {
                return -1;
            }

            dst = (short*)img->addr_plane[comp];

            if (bit_depth_input == 10)
            {
                for( int y = 0; y < img->height[comp] - padding_h; y++ )
                {
                    int ind_src = y * (img->width[comp] - padding_w);
                    int ind_dst = y * img->width[comp];
                    for( int x = 0; x < img->width[comp] - padding_w; x++ )
                    {
                        int i_dst = ind_dst + x;
                        int i_src = ind_src + x;
                        dst[i_dst] = (buf[i_src * 2] | (buf[i_src * 2 + 1] << 8));

                        if( dst[i_dst] > 1023 || dst[i_dst] < 0 )
                        {
                            printf( "\nError: input pixel value %d beyond [0, 1023], please check bit-depth %d of the input yuv file.\n", dst[i_dst], bit_depth_input );
                            return -1;
                        }
                    }
                    //padding right
                    for( int x = img->width[comp] - padding_w; x < img->width[comp]; x++ )
                    {
                        int i_dst = ind_dst + x;
                        dst[i_dst] = dst[i_dst - 1];
                    }
                }
                //padding bottom
                for( int y = img->height[comp] - padding_h; y < img->height[comp]; y++ )
                {
                    int ind_dst = y * img->width[comp];
                    for( int x = 0; x < img->width[comp]; x++ )
                    {
                        int i_dst = ind_dst + x;
                        dst[i_dst] = dst[i_dst - img->width[comp]];
                    }
                }
            }
            else
            {
                for( int y = 0; y < img->height[comp] - padding_h; y++ )
                {
                    int ind_src = y * (img->width[comp] - padding_w);
                    int ind_dst = y * img->width[comp];
                    for( int x = 0; x < img->width[comp] - padding_w; x++ )
                    {
                        int i_dst = ind_dst + x;
                        int i_src = ind_src + x;
                        dst[i_dst] = buf[i_src];
                    }
                    //padding right
                    for( int x = img->width[comp] - padding_w; x < img->width[comp]; x++ )
                    {
                        int i_dst = ind_dst + x;
                        dst[i_dst] = dst[i_dst - 1];
                    }
                }
                //padding bottom
                for( int y = img->height[comp] - padding_h; y < img->height[comp]; y++ )
                {
                    int ind_dst = y * img->width[comp];
                    for( int x = 0; x < img->width[comp]; x++ )
                    {
                        int i_dst = ind_dst + x;
                        dst[i_dst] = dst[i_dst - img->width[comp]];
                    }
                }
            }

            if (bit_shift > 0)
            {
                for (int i = 0; i < size_yuv[comp]; i++)
                {
                    dst[i] = dst[i] << bit_shift;
                }
            }
            else if (bit_shift < 0)
            {
                int rounding = 1 << ((-bit_shift) - 1);
                for (int i = 0; i < size_yuv[comp]; i++)
                {
                    dst[i] = IFVCA_CLIP((dst[i] + rounding) >> (-bit_shift), minval, maxval);
                }
            }

            free(buf);
        }
    }
    else
    {
        print("not supported color space\n");
        return -1;
    }
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _IFVCA_UTIL_H_ */
