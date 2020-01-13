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

/* macros for reference picture flag */
#define IS_REF(pic)          (((pic)->is_ref) != 0)
#define SET_REF_UNMARK(pic)  (((pic)->is_ref) = 0)
#define SET_REF_MARK(pic)    (((pic)->is_ref) = 1)

#define PRINT_DPB(pm)\
    printf("%s: current num_ref = %d, dpb_size = %d\n", __FUNCTION__, \
    pm->cur_num_ref_pics, picman_get_num_allocated_pics(pm));

static int picman_get_num_allocated_pics(COM_PM * pm)
{
    int i, cnt = 0;
    for(i = 0; i < MAX_PB_SIZE; i++) /* this is coding order */
    {
        if(pm->pic[i]) cnt++;
    }
    return cnt;
}

static int picman_move_pic(COM_PM *pm, int from, int to)
{
    int i;
    COM_PIC * pic;
    int temp_cur_num_ref_pics = pm->cur_num_ref_pics;
    BOOL found_empty_pos = 0;

    pic = pm->pic[from];

    for (i = from; i < temp_cur_num_ref_pics - 1; i++)
    {
      pm->pic[i] = pm->pic[i + 1];// move the remaining ref pic to the front
    }
    pm->pic[temp_cur_num_ref_pics - 1] = NULL; // the new end fill with NULL.
    temp_cur_num_ref_pics--;// update, since the ref-pic number decrease 
    for (i = to; i > temp_cur_num_ref_pics; i--)
    {
      if (pm->pic[i] == NULL)
      {
        pm->pic[i] = pic;// find the first NULL pos from end to front, and fill with the un-ref pic
        found_empty_pos = 1;
        break;
      }
    }
    assert(found_empty_pos == 1);
    return 0;
}

static void picman_flush_pb(COM_PM * pm)
{
    int i;
    /* mark all frames unused */
    for(i = 0; i < MAX_PB_SIZE; i++)
    {
        if(pm->pic[i]) SET_REF_UNMARK(pm->pic[i]);
    }
    pm->cur_num_ref_pics = 0;
}

static void picman_update_pic_ref(COM_PM * pm)
{
    COM_PIC ** pic;
    COM_PIC ** pic_ref;
    COM_PIC  * pic_t;
    int i, j, cnt;
    pic = pm->pic;
    pic_ref = pm->pic_ref;
    for(i = 0, j = 0; i < MAX_PB_SIZE; i++)
    {
        if(pic[i] && IS_REF(pic[i]))
        {
            pic_ref[j++] = pic[i];
        }
    }
    cnt = j;
    while(j < MAX_NUM_REF_PICS) pic_ref[j++] = NULL;
    /* descending order sort based on PTR */
    for(i = 0; i < cnt - 1; i++)
    {
        for(j = i + 1; j < cnt; j++)
        {
            if(pic_ref[i]->ptr < pic_ref[j]->ptr)
            {
                pic_t = pic_ref[i];
                pic_ref[i] = pic_ref[j];
                pic_ref[j] = pic_t;
            }
        }
    }
}

static COM_PIC * picman_remove_pic_from_pb(COM_PM * pm, int pos)
{
    int         i;
    COM_PIC  * pic_rem;
    pic_rem = pm->pic[pos];
    pm->pic[pos] = NULL;
    /* fill empty pic buffer */
    for(i = pos; i < MAX_PB_SIZE - 1; i++)
    {
        pm->pic[i] = pm->pic[i + 1];
    }
    pm->pic[MAX_PB_SIZE - 1] = NULL;
    pm->cur_pb_size--;
    return pic_rem;
}

static void picman_set_pic_to_pb(COM_PM * pm, COM_PIC * pic,
                                 COM_REFP(*refp)[REFP_NUM], int pos)
{
    int i;
    for (i = 0; i < pm->num_refp[REFP_0]; i++)
    {
        pic->list_ptr[i] = refp[i][REFP_0].ptr;
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
        pic->list_is_library_pic[i] = refp[i][REFP_0].is_library_picture;
#endif
    }
    if(pos >= 0)
    {
        com_assert(pm->pic[pos] == NULL);
        pm->pic[pos] = pic;
    }
    else /* pos < 0 */
    {
        /* search empty pic buffer position */
        for(i = (MAX_PB_SIZE - 1); i >= 0; i--)
        {
            if(pm->pic[i] == NULL)
            {
                pm->pic[i] = pic;
                break;
            }
        }
        if(i < 0)
        {
            printf("i=%d\n", i);
            com_assert(i >= 0);
        }
    }
    pm->cur_pb_size++;
}

static int picman_get_empty_pic_from_list(COM_PM * pm)
{
    COM_IMGB * imgb;
    COM_PIC  * pic;
    int i;
    for(i = 0; i < MAX_PB_SIZE; i++)
    {
        pic = pm->pic[i];
        if(pic != NULL && !IS_REF(pic) && pic->need_for_out == 0)
        {
            imgb = pic->imgb;
            com_assert(imgb != NULL);
            /* check reference count */
            if(1 == imgb->getref(imgb))
            {
                return i; /* this is empty buffer */
            }
        }
    }
    return -1;
}

void set_refp(COM_REFP * refp, COM_PIC  * pic_ref)
{
    refp->pic      = pic_ref;
    refp->ptr      = pic_ref->ptr;
    refp->map_mv   = pic_ref->map_mv;
    refp->map_refi = pic_ref->map_refi;
    refp->list_ptr = pic_ref->list_ptr;
#if LIBVC_ON
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
    refp->list_is_library_pic = pic_ref->list_is_library_pic;
#endif
    refp->is_library_picture = 0;
#endif
}

void copy_refp(COM_REFP * refp_dst, COM_REFP * refp_src)
{
    refp_dst->pic      = refp_src->pic;
    refp_dst->ptr      = refp_src->ptr;
    refp_dst->map_mv   = refp_src->map_mv;
    refp_dst->map_refi = refp_src->map_refi;
    refp_dst->list_ptr = refp_src->list_ptr;
#if LIBVC_ON
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
    refp_dst->list_is_library_pic = refp_src->list_is_library_pic;
#endif
    refp_dst->is_library_picture = refp_src->is_library_picture;
#endif
}

int check_copy_refp(COM_REFP(*refp)[REFP_NUM], int cnt, int lidx, COM_REFP  * refp_src)
{
    int i;
    for(i = 0; i < cnt; i++)
    {
#if LIBVC_ON
        if (refp[i][lidx].ptr == refp_src->ptr && refp[i][lidx].is_library_picture == refp_src->is_library_picture)
#else
        if (refp[i][lidx].ptr == refp_src->ptr)
#endif
        {
            return -1;
        }
    }
    copy_refp(&refp[cnt][lidx], refp_src);
    return COM_OK;
}

#if HLS_RPL   //This is implementation of reference picture list construction based on RPL. This is meant to replace function int com_picman_refp_init(COM_PM *pm, int num_ref_pics_act, int slice_type, u32 ptr, u8 layer_id, int last_intra, COM_REFP(*refp)[REFP_NUM])
int com_picman_refp_rpl_based_init(COM_PM *pm, COM_PIC_HEADER *pic_header, COM_REFP(*refp)[REFP_NUM])
{
    //MX: IDR?
    if ((pic_header->slice_type == SLICE_I)&&(pic_header->poc == 0))
    {
        pm->num_refp[REFP_0] = pm->num_refp[REFP_1] = 0;
        return COM_OK;
    }

    picman_update_pic_ref(pm);
#if LIBVC_ON
    if (!pm->libvc_data->library_picture_enable_flag && pic_header->slice_type != SLICE_I)
#endif
    {
        com_assert_rv(pm->cur_num_ref_pics > 0, COM_ERR_UNEXPECTED);
    }
    for (int i = 0; i < MAX_NUM_REF_PICS; i++)
    {
        refp[i][REFP_0].pic = refp[i][REFP_1].pic = NULL;
    }
    pm->num_refp[REFP_0] = pm->num_refp[REFP_1] = 0;

    //Do the L0 first
    for (int i = 0; i < pic_header->rpl_l0.ref_pic_active_num; i++)
    {
#if LIBVC_ON
        if (pm->libvc_data->library_picture_enable_flag && pic_header->rpl_l0.library_index_flag[i])
        {
            int ref_lib_index = pic_header->rpl_l0.ref_pics[i];
            com_assert_rv(ref_lib_index == pm->pb_libpic_library_index, COM_ERR_UNEXPECTED);

            set_refp(&refp[i][REFP_0], pm->pb_libpic);
            refp[i][REFP_0].ptr = pic_header->poc - 1;
            refp[i][REFP_0].is_library_picture = 1;
            pm->num_refp[REFP_0] = pm->num_refp[REFP_0] + 1;
        }
        else
#endif
        {
            int refPicPoc = pic_header->poc - pic_header->rpl_l0.ref_pics[i];
            //Find the ref pic in the DPB
            int j = 0;
            while (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr != refPicPoc) j++;

            //If the ref pic is found, set it to RPL0
            if (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr == refPicPoc)
            {
                set_refp(&refp[i][REFP_0], pm->pic_ref[j]);
                pm->num_refp[REFP_0] = pm->num_refp[REFP_0] + 1;
                //make sure delta doi of RPL0 correct,in case of last incomplete GOP
                int diff = (pic_header->decode_order_index%DOI_CYCLE_LENGTH - pm->pic_ref[j]->dtr);//MX: doi is not increase mono, but in the range of [0,255]; the diff may be minus value.
                if (diff != pic_header->rpl_l0.ref_pics_ddoi[i])
                {
                    pic_header->ref_pic_list_sps_flag[0] = 0;
                    pic_header->rpl_l0.ref_pics_ddoi[i] = diff;
                }
            }
            else
                return COM_ERR;   //The refence picture must be available in the DPB, if not found then there is problem
        }
    }
    //update inactive ref ddoi in L0,in case of last incomplete GOP
    for (int i = 0; i < MAX_NUM_REF_PICS; i++)
    {
#if LIBVC_ON
        if (!pic_header->rpl_l0.library_index_flag[i] && pic_header->rpl_l0.ref_pics[i] > 0)
        {
#else
        if (pic_header->rpl_l0.ref_pics[i] > 0)
        {
#endif
            int refPicPoc = pic_header->poc - pic_header->rpl_l0.ref_pics[i];
            //Find the ref pic in the DPB
            int j = 0;
            while (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr != refPicPoc) j++;

            //If the ref pic is found, set it to RPL0
            if (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr == refPicPoc)
            {
                int diff = (pic_header->decode_order_index%DOI_CYCLE_LENGTH - pm->pic_ref[j]->dtr);//MX: doi is not increase mono, but in the range of [0,255]; the diff may be minus value.
                if (diff != pic_header->rpl_l0.ref_pics_ddoi[i])
                {
                    pic_header->ref_pic_list_sps_flag[0] = 0;
                    pic_header->rpl_l0.ref_pics_ddoi[i] = diff;
                }
            }
        }
    }
    if (pic_header->slice_type == SLICE_P) return COM_OK;

    //Do the L1 first
    for (int i = 0; i < pic_header->rpl_l1.ref_pic_active_num; i++)
    {
#if LIBVC_ON
        if (pm->libvc_data->library_picture_enable_flag && pic_header->rpl_l1.library_index_flag[i])
        {
            int ref_lib_index = pic_header->rpl_l1.ref_pics[i];
            com_assert_rv(ref_lib_index == pm->pb_libpic_library_index, COM_ERR_UNEXPECTED);

            set_refp(&refp[i][REFP_1], pm->pb_libpic);
            refp[i][REFP_1].ptr = pic_header->poc - 1;
            refp[i][REFP_1].is_library_picture = 1;
            pm->num_refp[REFP_1] = pm->num_refp[REFP_1] + 1;
        }
        else
#endif
        {
            int refPicPoc = pic_header->poc - pic_header->rpl_l1.ref_pics[i];
            //Find the ref pic in the DPB
            int j = 0;
            while (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr != refPicPoc) j++;

            //If the ref pic is found, set it to RPL1
            if (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr == refPicPoc)
            {
                set_refp(&refp[i][REFP_1], pm->pic_ref[j]);
                pm->num_refp[REFP_1] = pm->num_refp[REFP_1] + 1;
                //make sure delta doi of RPL0 correct
                int diff = (pic_header->decode_order_index%DOI_CYCLE_LENGTH - pm->pic_ref[j]->dtr);//MX: doi is not increase mono, but in the range of [0,255]; the diff may be minus value.
                if (diff != pic_header->rpl_l1.ref_pics_ddoi[i])
                {
                    pic_header->ref_pic_list_sps_flag[1] = 0;
                    pic_header->rpl_l1.ref_pics_ddoi[i] = diff;
                }
            }
            else
                return COM_ERR;   //The refence picture must be available in the DPB, if not found then there is problem
        }
    }
    //update inactive ref ddoi in L1
    for (int i = 0; i < MAX_NUM_REF_PICS; i++)
    {
#if LIBVC_ON
        if (!pic_header->rpl_l1.library_index_flag[i] && pic_header->rpl_l1.ref_pics[i] > 0)
        {
#else
        if (pic_header->rpl_l1.ref_pics[i] > 0)
        {
#endif
            int refPicPoc = pic_header->poc - pic_header->rpl_l1.ref_pics[i];
            //Find the ref pic in the DPB
            int j = 0;
            while (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr != refPicPoc) j++;

            //If the ref pic is found, set it to RPL0
            if (j < pm->cur_num_ref_pics && pm->pic_ref[j]->ptr == refPicPoc)
            {
                int diff = (pic_header->decode_order_index%DOI_CYCLE_LENGTH - pm->pic_ref[j]->dtr);//MX: doi is not increase mono, but in the range of [0,255]; the diff may be minus value.
                if (diff != pic_header->rpl_l1.ref_pics_ddoi[i])
                {
                    pic_header->ref_pic_list_sps_flag[1] = 0;
                    pic_header->rpl_l1.ref_pics_ddoi[i] = diff;
                }
            }
        }
    }
    return COM_OK;  //RPL construction completed
}

int com_cleanup_useless_pic_buffer_in_pm( COM_PM *pm )
{
    //remove the pic if it is a unref pic, and has been output.
    for( int i = 0; i < MAX_PB_SIZE; i++ )
    {
        if( (pm->pic[i] != NULL) && (pm->pic[i]->need_for_out == 0) && (pm->pic[i]->is_ref == 0) )
        {
            com_pic_free( &pm->pa, pm->pic[i] );
            pm->cur_pb_size--;
            pm->pic[i] = NULL;
        }
    }
    return COM_OK;
}

int com_picman_dpbpic_doi_minus_cycle_length( COM_PM *pm )
{
    COM_PIC * pic;
    for( int i = 0; i < MAX_PB_SIZE; i++ )
    {
        pic = pm->pic[i];
        if( pic != NULL )//MX: no matter whether is ref or unref(for output), need to minus 256.
        {
            pic->dtr = pic->dtr - DOI_CYCLE_LENGTH;
            assert( pic->dtr >= (-256) );//MX:minus once at most.
        }
    }
    return COM_OK;
}

//This is implementation of reference picture list construction based on RPL for decoder
//in decoder, use DOI as pic reference instead of POC
int com_picman_refp_rpl_based_init_decoder(COM_PM *pm, COM_PIC_HEADER *pic_header, COM_REFP(*refp)[REFP_NUM])
{
    picman_update_pic_ref(pm);
#if LIBVC_ON
    if (!pm->libvc_data->library_picture_enable_flag && pic_header->slice_type != SLICE_I)
#endif
    {
        com_assert_rv(pm->cur_num_ref_pics > 0, COM_ERR_UNEXPECTED);
    }

    for (int i = 0; i < MAX_NUM_REF_PICS; i++)
        refp[i][REFP_0].pic = refp[i][REFP_1].pic = NULL;
    pm->num_refp[REFP_0] = pm->num_refp[REFP_1] = 0;

    //Do the L0 first
    for (int i = 0; i < pic_header->rpl_l0.ref_pic_active_num; i++)
    {
#if LIBVC_ON
        if (pm->libvc_data->library_picture_enable_flag && pic_header->rpl_l0.library_index_flag[i])
        {
            int ref_lib_index = pic_header->rpl_l0.ref_pics_ddoi[i];
            com_assert_rv(ref_lib_index == pm->pb_libpic_library_index, COM_ERR_UNEXPECTED);

            set_refp(&refp[i][REFP_0], pm->pb_libpic);
            refp[i][REFP_0].ptr = pic_header->poc - 1;
            refp[i][REFP_0].is_library_picture = 1;
            pm->num_refp[REFP_0] = pm->num_refp[REFP_0] + 1;
        }
        else
#endif
        {
            int refPicDoi = pic_header->rpl_l0.ref_pics_doi[i];//MX: no need to fix the value in the range of 0~255. because DOI in the DPB can be a minus value, after minus 256.
            //Find the ref pic in the DPB
            int j = 0;
            while (j < pm->cur_num_ref_pics && pm->pic_ref[j]->dtr != refPicDoi) j++;

            //If the ref pic is found, set it to RPL0
            if (j < pm->cur_num_ref_pics && pm->pic_ref[j]->dtr == refPicDoi)
            {
                set_refp(&refp[i][REFP_0], pm->pic_ref[j]);
                pm->num_refp[REFP_0] = pm->num_refp[REFP_0] + 1;
            }
            else
                return COM_ERR;   //The refence picture must be available in the DPB, if not found then there is problem
        }
    }

    if (pic_header->slice_type == SLICE_P) return COM_OK;

    //Do the L1 first
    for (int i = 0; i < pic_header->rpl_l1.ref_pic_active_num; i++)
    {
#if LIBVC_ON
        if (pm->libvc_data->library_picture_enable_flag && pic_header->rpl_l1.library_index_flag[i])
        {
            int ref_lib_index = pic_header->rpl_l1.ref_pics_ddoi[i];
            com_assert_rv(ref_lib_index == pm->pb_libpic_library_index, COM_ERR_UNEXPECTED);

            set_refp(&refp[i][REFP_1], pm->pb_libpic);
            refp[i][REFP_1].ptr = pic_header->poc - 1;
            refp[i][REFP_1].is_library_picture = 1;
            pm->num_refp[REFP_1] = pm->num_refp[REFP_1] + 1;
        }
        else
#endif
        {
            int refPicDoi = pic_header->rpl_l1.ref_pics_doi[i];//MX: no need to fix the value in the range of 0~255. because DOI in the DPB can be a minus value, after minus 256.
            //Find the ref pic in the DPB
            int j = 0;
            while (j < pm->cur_num_ref_pics && pm->pic_ref[j]->dtr != refPicDoi) j++;

            //If the ref pic is found, set it to RPL1
            if (j < pm->cur_num_ref_pics && pm->pic_ref[j]->dtr == refPicDoi)
            {
                set_refp(&refp[i][REFP_1], pm->pic_ref[j]);
                pm->num_refp[REFP_1] = pm->num_refp[REFP_1] + 1;
            }
            else
                return COM_ERR;   //The refence picture must be available in the DPB, if not found then there is problem
        }
    }

    return COM_OK;  //RPL construction completed
}
#endif

COM_PIC * com_picman_get_empty_pic(COM_PM * pm, int * err)
{
    int ret;
    COM_PIC * pic = NULL;
#if LIBVC_ON
    if (pm->libvc_data->is_libpic_processing)
    {
        /* no need to find empty picture buffer in list */
        ret = -1;
    }
    else
#endif
    {
        /* try to find empty picture buffer in list */
        ret = picman_get_empty_pic_from_list(pm);
    }
    if(ret >= 0)
    {
        pic = picman_remove_pic_from_pb(pm, ret);
        goto END;
    }
    /* else if available, allocate picture buffer */
    pm->cur_pb_size = picman_get_num_allocated_pics(pm);
#if LIBVC_ON
    if (pm->cur_pb_size + pm->cur_libpb_size < pm->max_pb_size)
#else
    if(pm->cur_pb_size < pm->max_pb_size)
#endif
    {
        /* create picture buffer */
        pic = com_pic_alloc(&pm->pa, &ret);
        com_assert_gv(pic != NULL, ret, COM_ERR_OUT_OF_MEMORY, ERR);
        goto END;
    }
    com_assert_gv(0, ret, COM_ERR_UNKNOWN, ERR);
END:
    pm->pic_lease = pic;
    if(err) *err = COM_OK;
    return pic;
ERR:
    if(err) *err = ret;
    if(pic) com_pic_free(&pm->pa, pic);
    return NULL;
}
#if HLS_RPL
/*This is the implementation of reference picture marking based on RPL*/
int com_picman_refpic_marking(COM_PM *pm, COM_PIC_HEADER *pic_header)
{
    picman_update_pic_ref(pm);
#if LIBVC_ON
    if (!pic_header->rpl_l0.reference_to_library_enable_flag && !pic_header->rpl_l1.reference_to_library_enable_flag && pic_header->slice_type != SLICE_I && pic_header->poc != 0)
#else
    if (pic_header->slice_type != SLICE_I && pic_header->poc != 0)
#endif
        com_assert_rv(pm->cur_num_ref_pics > 0, COM_ERR_UNEXPECTED);

    COM_PIC * pic;
    int numberOfPicsToCheck = pm->cur_num_ref_pics;
    for (int i = 0; i < numberOfPicsToCheck; i++)
    {
        pic = pm->pic[i];
        if (pm->pic[i] && IS_REF(pm->pic[i]))
        {
            //If the pic in the DPB is a reference picture, check if this pic is included in RPL0
            int isIncludedInRPL = 0;
            int j = 0;
            while (!isIncludedInRPL && j < pic_header->rpl_l0.ref_pic_num)
            {
#if LIBVC_ON
                if ((pic->ptr == (pic_header->poc - pic_header->rpl_l0.ref_pics[j])) && !pic_header->rpl_l0.library_index_flag[j])  //NOTE: we need to put POC also in COM_PIC
#else
                if (pic->ptr == (pic_header->poc - pic_header->rpl_l0.ref_pics[j]))  //NOTE: we need to put POC also in COM_PIC
#endif
                {
                    isIncludedInRPL = 1;
                }
                j++;
            }
            //Check if the pic is included in RPL1. This while loop will be executed only if the ref pic is not included in RPL0
            j = 0;
            while (!isIncludedInRPL && j < pic_header->rpl_l1.ref_pic_num)
            {
#if LIBVC_ON
                if ((pic->ptr == (pic_header->poc - pic_header->rpl_l1.ref_pics[j])) && !pic_header->rpl_l1.library_index_flag[j])  //NOTE: we need to put POC also in COM_PIC
#else
                if (pic->ptr == (pic_header->poc - pic_header->rpl_l1.ref_pics[j]))
#endif
                {
                    isIncludedInRPL = 1;
                }
                j++;
            }
            //If the ref pic is not included in either RPL0 nor RPL1, then mark it as not used for reference. move it to the end of DPB.
            if (!isIncludedInRPL)
            {
                SET_REF_UNMARK(pic);
                picman_move_pic(pm, i, MAX_PB_SIZE - 1);
                pm->cur_num_ref_pics--;
                i--;                                           //We need to decrement i here because it will be increment by i++ at for loop. We want to keep the same i here because after the move, the current ref pic at i position is the i+1 position which we still need to check.
                numberOfPicsToCheck--;                         //We also need to decrement this variable to avoid checking the moved ref picture twice.
            }
        }
    }
#if LIBVC_ON
    // count the libpic in rpl
    int count_library_picture = 0;
    int referenced_library_picture_index = -1;
    for (int i = 0; i < pic_header->rpl_l0.ref_pic_num; i++)
    {
        if (pic_header->rpl_l0.library_index_flag[i])
        {
            if (count_library_picture == 0)
            {
                referenced_library_picture_index = pic_header->rpl_l0.ref_pics[i];
                count_library_picture++;
            }

            com_assert_rv(referenced_library_picture_index == pic_header->rpl_l0.ref_pics[i], COM_ERR_UNEXPECTED);
        }
    }
    for (int i = 0; i < pic_header->rpl_l1.ref_pic_num; i++)
    {
        if (pic_header->rpl_l1.library_index_flag[i])
        {
            if (count_library_picture == 0)
            {
                referenced_library_picture_index = pic_header->rpl_l1.ref_pics[i];
                count_library_picture++;
            }

            com_assert_rv(referenced_library_picture_index == pic_header->rpl_l1.ref_pics[i], COM_ERR_UNEXPECTED);
        }
    }

    if (count_library_picture > 0)
    {
        // move out lib pic
        if (!pm->is_library_buffer_empty && (referenced_library_picture_index != pm->pb_libpic_library_index))
        {
            //com_picbuf_free(pm->libvc_data->pb_libpic);
            pm->pb_libpic = NULL;
            pm->cur_libpb_size--;
            pm->pb_libpic_library_index = -1;
            pm->is_library_buffer_empty = 1;
        }
        // move in lib pic from the buffer outside the decoder
        if (pm->is_library_buffer_empty)
        {
            //send out referenced_library_picture_index
            int  libpic_idx = -1;
            for (int ii = 0; ii < pm->libvc_data->num_libpic_outside; ii++)
            {
                if (referenced_library_picture_index == pm->libvc_data->list_library_index_outside[ii])
                {
                    libpic_idx = ii;
                    break;
                }
            }
            com_assert_rv(libpic_idx >= 0, COM_ERR_UNEXPECTED);

            // move in lib pic from the buffer outside the decoder
            pm->pb_libpic = pm->libvc_data->list_libpic_outside[libpic_idx];
            pm->pb_libpic_library_index = referenced_library_picture_index;
            pm->cur_libpb_size++;
            pm->is_library_buffer_empty = 0;
        }
    }
#endif
    return COM_OK;
}

int com_constrcut_ref_list_doi( COM_PIC_HEADER *pic_header )
{
    int i;
    for( i = 0; i < pic_header->rpl_l0.ref_pic_num; i++ )
    {
        pic_header->rpl_l0.ref_pics_doi[i] = pic_header->decode_order_index%DOI_CYCLE_LENGTH - pic_header->rpl_l0.ref_pics_ddoi[i];
    }
    for( i = 0; i < pic_header->rpl_l1.ref_pic_num; i++ )
    {
        pic_header->rpl_l1.ref_pics_doi[i] = pic_header->decode_order_index%DOI_CYCLE_LENGTH - pic_header->rpl_l1.ref_pics_ddoi[i];
    }
    return COM_OK;
}

/*This is the implementation of reference picture marking based on RPL for decoder */
/*In decoder side, use DOI as pic reference instead of POI */
int com_picman_refpic_marking_decoder(COM_PM *pm, COM_PIC_HEADER *pic_header)
{
    picman_update_pic_ref(pm);
#if LIBVC_ON
    if (!pic_header->rpl_l0.reference_to_library_enable_flag && !pic_header->rpl_l1.reference_to_library_enable_flag && pic_header->slice_type != SLICE_I && pic_header->poc != 0)
#else
    if (pic_header->slice_type != SLICE_I && pic_header->poc != 0)
#endif
        com_assert_rv(pm->cur_num_ref_pics > 0, COM_ERR_UNEXPECTED);

    COM_PIC * pic;
    int numberOfPicsToCheck = pm->cur_num_ref_pics;
    for (int i = 0; i < numberOfPicsToCheck; i++)
    {
        pic = pm->pic[i];
        if (pm->pic[i] && IS_REF(pm->pic[i]))
        {
            //If the pic in the DPB is a reference picture, check if this pic is included in RPL0
            int isIncludedInRPL = 0;
            int j = 0;
            while (!isIncludedInRPL && j < pic_header->rpl_l0.ref_pic_num)
            {
#if LIBVC_ON
                if( !pic_header->rpl_l0.library_index_flag[j] && pic->dtr == pic_header->rpl_l0.ref_pics_doi[j] )
#else
                if( pic->dtr == ((pic_header->decode_order_index - pic_header->rpl_l0.ref_pics_ddoi[j] + DOI_CYCLE_LENGTH) % DOI_CYCLE_LENGTH) )  //NOTE: we need to put POC also in COM_PIC
#endif
                {
                    isIncludedInRPL = 1;
                }
                j++;
            }
            //Check if the pic is included in RPL1. This while loop will be executed only if the ref pic is not included in RPL0
            j = 0;
            while (!isIncludedInRPL && j < pic_header->rpl_l1.ref_pic_num)
            {
#if LIBVC_ON
                if( !pic_header->rpl_l1.library_index_flag[j] && pic->dtr == pic_header->rpl_l1.ref_pics_doi[j] )
#else
                if( pic->dtr == ((pic_header->decode_order_index - pic_header->rpl_l1.ref_pics_ddoi[j] + DOI_CYCLE_LENGTH) % DOI_CYCLE_LENGTH) )
#endif
                {
                    isIncludedInRPL = 1;
                }
                j++;
            }
            //If the ref pic is not included in either RPL0 nor RPL1, then mark it as not used for reference. move it to the end of DPB.
            if (!isIncludedInRPL)
            {
                SET_REF_UNMARK(pic);
                picman_move_pic(pm, i, MAX_PB_SIZE - 1);
                pm->cur_num_ref_pics--;
                i--;                                           //We need to decrement i here because it will be increment by i++ at for loop. We want to keep the same i here because after the move, the current ref pic at i position is the i+1 position which we still need to check.
                numberOfPicsToCheck--;                         //We also need to decrement this variable to avoid checking the moved ref picture twice.
            }
        }
    }
#if LIBVC_ON
    // count the libpic in rpl
    int count_library_picture = 0;
    int referenced_library_picture_index = -1;
    for (int i = 0; i < pic_header->rpl_l0.ref_pic_num; i++)
    {
        if (pic_header->rpl_l0.library_index_flag[i])
        {
            if (count_library_picture == 0)
            {
                referenced_library_picture_index = pic_header->rpl_l0.ref_pics_ddoi[i];
                count_library_picture++;
            }

            com_assert_rv(referenced_library_picture_index == pic_header->rpl_l0.ref_pics_ddoi[i], COM_ERR_UNEXPECTED);
        }
    }
    for (int i = 0; i < pic_header->rpl_l1.ref_pic_num; i++)
    {
        if (pic_header->rpl_l1.library_index_flag[i])
        {
            if (count_library_picture == 0)
            {
                referenced_library_picture_index = pic_header->rpl_l1.ref_pics_ddoi[i];
                count_library_picture++;
            }

            com_assert_rv(referenced_library_picture_index == pic_header->rpl_l1.ref_pics_ddoi[i], COM_ERR_UNEXPECTED);
        }
    }

    if (count_library_picture > 0)
    {
        // move out lib pic
        if (!pm->is_library_buffer_empty && (referenced_library_picture_index != pm->pb_libpic_library_index))
        {
            //com_picbuf_free(pm->libvc_data->pb_libpic);
            pm->pb_libpic = NULL;
            pm->cur_libpb_size--;
            pm->pb_libpic_library_index = -1;
            pm->is_library_buffer_empty = 1;
        }
        // move in lib pic from the buffer outside the decoder
        if (pm->is_library_buffer_empty)
        {
            //send out referenced_library_picture_index
            int  libpic_idx = -1;
            for (int ii = 0; ii < pm->libvc_data->num_libpic_outside; ii++)
            {
                if (referenced_library_picture_index == pm->libvc_data->list_library_index_outside[ii])
                {
                    libpic_idx = ii;
                    break;
                }
            }
            com_assert_rv(libpic_idx >= 0, COM_ERR_UNEXPECTED);

            //move in the corresponding referenced library pic
            pm->pb_libpic = pm->libvc_data->list_libpic_outside[libpic_idx];
            pm->pb_libpic_library_index = referenced_library_picture_index;
            pm->cur_libpb_size++;
            pm->is_library_buffer_empty = 0;
        }
    }
#endif
    return COM_OK;
}


#endif

#if LIBVC_ON
int com_picman_put_libpic(COM_PM * pm, COM_PIC * pic, int slice_type, u32 ptr, u32 dtr, u8 temporal_id, int need_for_output, COM_REFP(*refp)[REFP_NUM], COM_PIC_HEADER * pic_header)
{
    SET_REF_MARK(pic);
    pic->temporal_id = temporal_id;
    pic->ptr = ptr;
    pic->dtr = dtr;
    pic->need_for_out = (u8)need_for_output;

    int i;
    for (i = 0; i < pm->num_refp[REFP_0]; i++)
    {
        pic->list_ptr[i] = refp[i][REFP_0].ptr;
#if !LIBVC_BLOCKDISTANCE_BY_LIBPTR
        pic->list_is_library_pic[i] = refp[i][REFP_0].is_library_picture;
#endif
    }

    // move out
    if (!pm->is_library_buffer_empty)
    {
        //com_picbuf_free(pm->pb_libpic);
        pm->pb_libpic = NULL;
        pm->cur_libpb_size--;
        pm->pb_libpic_library_index = -1;
        pm->is_library_buffer_empty = 1;
    }

    // move in
    pm->pb_libpic = pic;
    pm->pb_libpic_library_index = pic_header->library_picture_index;
    pm->cur_libpb_size++;
    pm->is_library_buffer_empty = 0;

    if (pm->pic_lease == pic)
    {
        pm->pic_lease = NULL;
    }

    return COM_OK;
}
#endif

int com_picman_put_pic(COM_PM * pm, COM_PIC * pic, int slice_type, u32 ptr, u32 dtr, 
  u32 picture_output_delay, u8 temporal_id, int need_for_output, COM_REFP(*refp)[REFP_NUM])
{
    /* manage RPB */
    SET_REF_MARK(pic);
    pic->temporal_id = temporal_id;
    pic->ptr = ptr;
    pic->dtr = dtr%DOI_CYCLE_LENGTH;//MX: range of 0~255
    pic->picture_output_delay = picture_output_delay;
    pic->need_for_out = (u8)need_for_output;
    /* put picture into listed RPB */
    if(IS_REF(pic))
    {
        picman_set_pic_to_pb(pm, pic, refp, pm->cur_num_ref_pics);
        pm->cur_num_ref_pics++;
    }
    else
    {
        picman_set_pic_to_pb(pm, pic, refp, -1);
    }
    if(pm->pic_lease == pic)
    {
        pm->pic_lease = NULL;
    }
    /*PRINT_DPB(pm);*/
    return COM_OK;
}
#if LIBVC_ON
int com_picman_out_libpic(COM_PIC * pic, int library_picture_index, COM_PM * pm)
{
    if (pic != NULL && pic->need_for_out)
    {
        //output to the buffer outside the decoder
        int num_libpic_outside = pm->libvc_data->num_libpic_outside;
        pm->libvc_data->list_libpic_outside[num_libpic_outside] = pic;
        pm->libvc_data->list_library_index_outside[num_libpic_outside] = library_picture_index;
        pm->libvc_data->num_libpic_outside++;
        pic->need_for_out = 0;

        return COM_OK;
    }
    else
    {
        return COM_ERR_UNEXPECTED;
    }


}
#endif

COM_PIC * com_picman_out_pic(COM_PM * pm, int * err, int cur_pic_doi, int state)
{
    COM_PIC ** ps;
    int i, ret, any_need_for_out = 0;
    ps = pm->pic;
    BOOL exist_pic = 0;
    int temp_smallest_poc = MAX_INT;
    int temp_idx_for_smallest_poc = 0;

    if (state != 1)
    {
        for (i = 0; i < MAX_PB_SIZE; i++)
        {
            if (ps[i] != NULL && ps[i]->need_for_out)
            {
                any_need_for_out = 1;
                if ((ps[i]->dtr + ps[i]->picture_output_delay <= cur_pic_doi))
                {
                    exist_pic = 1;
                    if (temp_smallest_poc >= ps[i]->ptr)
                    {
                        temp_smallest_poc = ps[i]->ptr;
                        temp_idx_for_smallest_poc = i;
                    }
                }
            }
        }
        if (exist_pic)
        {
            ps[temp_idx_for_smallest_poc]->need_for_out = 0;
            if (err) *err = COM_OK;
            return ps[temp_idx_for_smallest_poc];
        }
    }
    else
    {
        //bumping state, bumping the pic in the DPB according to the POC number, from small to larger.
        for (i = 0; i < MAX_PB_SIZE; i++)
        {
            if (ps[i] != NULL && ps[i]->need_for_out)
            {
                any_need_for_out = 1;
                //Need to output the smallest poc
                if ((ps[i]->ptr <= temp_smallest_poc))
                {
                    exist_pic = 1;
                    temp_smallest_poc = ps[i]->ptr;
                    temp_idx_for_smallest_poc = i;
                }
            }
        }
        if (exist_pic)
        {
            ps[temp_idx_for_smallest_poc]->need_for_out = 0;
            if (err) *err = COM_OK;
            return ps[temp_idx_for_smallest_poc];
        }
    }

    if (any_need_for_out == 0)
    {
        ret = COM_ERR_UNEXPECTED;
    }
    else
    {
        ret = COM_OK_FRM_DELAYED;
    }
    if (err) *err = ret;
    return NULL;
}

int com_picman_deinit(COM_PM * pm)
{
    int i;
    /* remove allocated picture and picture store buffer */
    for (i = 0; i < MAX_PB_SIZE; i++)
    {
        if (pm->pic[i])
        {
            com_pic_free(&pm->pa, pm->pic[i]);
            pm->pic[i] = NULL;
        }
    }
    if (pm->pic_lease)
    {
        com_pic_free(&pm->pa, pm->pic_lease);
        pm->pic_lease = NULL;
    }

    pm->cur_num_ref_pics = 0;

#if LIBVC_ON
    if (pm->pb_libpic)
    {
        pm->pb_libpic = NULL;
    }
    pm->cur_libpb_size = 0;
    pm->pb_libpic_library_index = -1;
    pm->is_library_buffer_empty = 1;
#endif
    return COM_OK;
}

int com_picman_init(COM_PM * pm, int max_pb_size, int max_num_ref_pics, PICBUF_ALLOCATOR * pa)
{
    if(max_num_ref_pics > MAX_NUM_REF_PICS || max_pb_size > MAX_PB_SIZE)
    {
        return COM_ERR_UNSUPPORTED;
    }
    pm->max_num_ref_pics = max_num_ref_pics;
    pm->max_pb_size = max_pb_size;
    pm->ptr_increase = 1;
    pm->pic_lease = NULL;
    com_mcpy(&pm->pa, pa, sizeof(PICBUF_ALLOCATOR));
#if LIBVC_ON
    pm->pb_libpic = NULL;
    pm->cur_libpb_size = 0;
    pm->pb_libpic_library_index = -1;
    pm->is_library_buffer_empty = 1;
#endif
    return COM_OK;
}

int com_picman_check_repeat_doi(COM_PM * pm, COM_PIC_HEADER * pic_header)
{
    COM_PIC * pic;
    for (int i = 0; i < MAX_PB_SIZE; i++)
    {
        pic = pm->pic[i];
        if (pic != NULL)
        {
            assert(pic->dtr != pic_header->decode_order_index);//the DOI of current frame cannot be the same as the DOI of pic in DPB.
        }
    }
    return COM_OK;
}
