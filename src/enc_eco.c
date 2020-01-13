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

#include "enc_def.h"
#include <limits.h>

int enc_eco_cnkh(COM_BSW * bs, COM_CNKH * cnkh)
{
    com_bsw_write(bs, cnkh->ver, 3);
    com_bsw_write(bs, cnkh->ctype, 4);
    com_bsw_write1(bs, cnkh->broken);
    return COM_OK;
}

#if HLS_RPL
#if LIBVC_ON
int appe_eco_rlp(COM_BSW * bs, COM_RPL * rpl, COM_SQH * sqh)
#else
int appe_eco_rlp(COM_BSW * bs, COM_RPL * rpl)
#endif
{
    //Hendry -- we don't write these into bitstream in the RPL
    //ifvc_bsw_write_ue(bs, (u32)rpl->poc);
    //ifvc_bsw_write_ue(bs, (u32)rpl->tid);

    //Yuqun Fan, notice RPL struct dismatch, check again
#if LIBVC_ON
    int ddoi_base = 0;

    if (sqh->library_picture_enable_flag)
    {
        com_bsw_write1(bs, rpl->reference_to_library_enable_flag);
    }
#endif

    com_bsw_write_ue(bs, (u32)rpl->ref_pic_num);
    if (rpl->ref_pic_num > 0)
    {
#if LIBVC_ON
        if (sqh->library_picture_enable_flag && rpl->reference_to_library_enable_flag)
        {
            com_bsw_write1(bs, rpl->library_index_flag[0]);
        }
        if (sqh->library_picture_enable_flag && rpl->library_index_flag[0])
        {
            com_bsw_write_ue(bs, (u32)abs(rpl->ref_pics_ddoi[0]));
        }
        else
#endif
        {
            com_bsw_write_ue(bs, (u32)abs(rpl->ref_pics_ddoi[0]));
            if (rpl->ref_pics_ddoi[0] != 0) com_bsw_write1(bs, rpl->ref_pics_ddoi[0] < 0);
#if LIBVC_ON
            ddoi_base = rpl->ref_pics_ddoi[0];
#endif
        }
    }
    for (int i = 1; i < rpl->ref_pic_num; ++i)
    {
#if LIBVC_ON
        if (sqh->library_picture_enable_flag && rpl->reference_to_library_enable_flag)
        {
            com_bsw_write1(bs, rpl->library_index_flag[i]);
        }
        if (sqh->library_picture_enable_flag && rpl->library_index_flag[i])
        {
            com_bsw_write_ue(bs, (u32)abs(rpl->ref_pics_ddoi[i]));
        }
        else
#endif
        {
#if LIBVC_ON
            int deltaRefPic = rpl->ref_pics_ddoi[i] - ddoi_base;
            com_bsw_write_ue(bs, (u32)abs(deltaRefPic));
            if (deltaRefPic != 0) com_bsw_write1(bs, deltaRefPic < 0);
            ddoi_base = rpl->ref_pics_ddoi[i];
#else
            int deltaRefPic = rpl->ref_pics_ddoi[i] - rpl->ref_pics_ddoi[i - 1];
            com_bsw_write_ue(bs, (u32)abs(deltaRefPic));
            if (deltaRefPic != 0) com_bsw_write1(bs, deltaRefPic < 0);
#endif
        }
    }
    return COM_OK;
}
#endif

void write_wq_matrix(COM_BSW * bs, u8 *m4x4, u8 *m8x8)
{
    int i;
    for (i = 0; i < 16; i++)
    {
        com_bsw_write_ue(bs, m4x4[i]);
    }
    for (i = 0; i < 64; i++)
    {
        com_bsw_write_ue(bs, m8x8[i]);
    }
}

int enc_eco_sqh(ENC_CTX *ctx, COM_BSW * bs, COM_SQH * sqh)
{
    //video_sequence_start_code
    com_bsw_write(bs, 0x000001, 24);
    com_bsw_write(bs, 0xB0, 8);

    com_bsw_write(bs, sqh->profile_id, 8);
    com_bsw_write(bs, sqh->level_id, 8);
    com_bsw_write1(bs, sqh->progressive_sequence);
    com_bsw_write1(bs, sqh->field_coded_sequence);
#if LIBVC_ON
    com_bsw_write1(bs, sqh->library_stream_flag);
    if (!sqh->library_stream_flag)
    {
        com_bsw_write1(bs, sqh->library_picture_enable_flag);
        if (sqh->library_picture_enable_flag)
        {
            com_bsw_write1(bs, sqh->duplicate_sequence_header_flag);
        }
    }
#endif
    com_bsw_write1(bs, 1); // marker_bit
#if DEUBG_TEST_CHANGE_HORI_VERT_SIZE
    com_bsw_write(bs, (sqh->horizontal_size % 8 == 0) ? sqh->horizontal_size - 1 : sqh->horizontal_size, 14);
#else
    com_bsw_write(bs, sqh->horizontal_size, 14);
#endif
    com_bsw_write1(bs, 1); // marker_bit
#if DEUBG_TEST_CHANGE_HORI_VERT_SIZE
    com_bsw_write(bs, (sqh->vertical_size % 8 == 0) ? sqh->vertical_size - 1 : sqh->vertical_size, 14);
#else
    com_bsw_write(bs, sqh->vertical_size, 14);
#endif
    com_bsw_write(bs, sqh->chroma_format, 2);
    com_bsw_write(bs, sqh->sample_precision, 3);
    if (sqh->profile_id == 0x22)
    {
        com_bsw_write(bs, sqh->encoding_precision, 3);
    }
    com_bsw_write1(bs, 1); // marker_bit
    com_bsw_write(bs, sqh->aspect_ratio, 4);
    com_bsw_write(bs, sqh->frame_rate_code, 4);
    com_bsw_write1(bs, 1); // marker_bit
    com_bsw_write(bs, sqh->bit_rate_lower, 18);
    com_bsw_write1(bs, 1); // marker_bit
    com_bsw_write(bs, sqh->bit_rate_upper, 12);
    com_bsw_write1(bs, sqh->low_delay);

    com_bsw_write1(bs, sqh->temporal_id_enable_flag);
    com_bsw_write1(bs, 1); // marker_bit
    com_bsw_write(bs, sqh->bbv_buffer_size, 18);
    com_bsw_write1(bs, 1); // marker_bit
    com_bsw_write(bs, sqh->max_dpb_size - 1, 4);

#if HLS_RPL
    com_bsw_write1(bs, (u32)sqh->rpl1_index_exist_flag);
    com_bsw_write1(bs, (u32)sqh->rpl1_same_as_rpl0_flag);
    com_bsw_write1(bs, 1); // marker_bit
    com_bsw_write_ue(bs, (u32)sqh->rpls_l0_num);
    for (int i = 0; i < sqh->rpls_l0_num; ++i)
    {
#if LIBVC_ON
        appe_eco_rlp(bs, &sqh->rpls_l0[i], sqh);
#else
        appe_eco_rlp(bs, &sqh->rpls_l0[i]);
#endif
    }

    if (!sqh->rpl1_same_as_rpl0_flag)
    {
        com_bsw_write_ue(bs, (u32)sqh->rpls_l1_num);
        for (int i = 0; i < sqh->rpls_l1_num; ++i)
        {
#if LIBVC_ON
            appe_eco_rlp(bs, &sqh->rpls_l1[i], sqh);
#else
            appe_eco_rlp(bs, &sqh->rpls_l1[i]);
#endif
        }
    }
    //write default number of active ref
    sqh->num_ref_default_active_minus1[0] = 1;
    sqh->num_ref_default_active_minus1[1] = 1;
    com_bsw_write_ue(bs, sqh->num_ref_default_active_minus1[0]); //num_ref_default_active_minus1[0]
    com_bsw_write_ue(bs, sqh->num_ref_default_active_minus1[1]); //num_ref_default_active_minus1[1]
#endif

    com_bsw_write(bs, sqh->log2_max_cu_width_height - 2,      3);
    com_bsw_write(bs, com_tbl_log2[sqh->min_cu_size] - 2,     2);

    com_bsw_write(bs, com_tbl_log2[sqh->max_part_ratio] - 2,  2);
    com_bsw_write(bs, sqh->max_split_times - 6,               3);
    com_bsw_write(bs, com_tbl_log2[sqh->min_qt_size] - 2,     3);
    com_bsw_write(bs, com_tbl_log2[sqh->max_bt_size] - 2,     3);

    com_bsw_write(bs, com_tbl_log2[sqh->max_eqt_size]- 3,     2);
    com_bsw_write1(bs, 1); // marker_bit

    com_bsw_write1(bs, sqh->wq_enable);
    if (sqh->wq_enable)
    {
        com_bsw_write1(bs, sqh->seq_wq_mode);
        if (sqh->seq_wq_mode)
        {
            write_wq_matrix(bs, sqh->wq_4x4_matrix, sqh->wq_8x8_matrix);
        }
    }

    com_bsw_write1(bs, sqh->secondary_transform_enable_flag);
    com_bsw_write1(bs, sqh->sample_adaptive_offset_enable_flag);
    com_bsw_write1(bs, sqh->adaptive_leveling_filter_enable_flag);
    com_bsw_write1(bs, sqh->affine_enable_flag);
#if SMVD
    com_bsw_write1(bs, sqh->smvd_enable_flag);
#endif
#if IPCM
    com_bsw_write1(bs, sqh->ipcm_enable_flag);
#endif
    com_bsw_write1(bs, sqh->amvr_enable_flag);
    com_bsw_write(bs, sqh->num_of_hmvp_cand, 4); // 1~ 15
    com_bsw_write1(bs, sqh->umve_enable_flag);
#if EXT_AMVR_HMVP
    if (sqh->amvr_enable_flag && sqh->num_of_hmvp_cand)
    {
        com_bsw_write1(bs, sqh->emvr_enable_flag);
    }
#endif
    com_bsw_write1(bs, sqh->ipf_enable_flag);
#if TSCPM
    com_bsw_write1(bs, sqh->tscpm_enable_flag);
#endif
    com_bsw_write1(bs, 1); // marker_bit
#if DT_PARTITION
    com_bsw_write1(bs, sqh->dt_intra_enable_flag);
    if (sqh->dt_intra_enable_flag)
    {
        com_bsw_write(bs, com_tbl_log2[sqh->max_dt_size] - 4, 2);
    }
#endif
    com_bsw_write1(bs, sqh->position_based_transform_enable_flag);
    if (sqh->low_delay == 0)
    {
        com_bsw_write(bs, sqh->output_reorder_delay, 5);
    }

#if PATCH
    com_bsw_write1(bs, sqh->cross_patch_loop_filter);
    com_bsw_write1(bs, sqh->patch_ref_colocated);
    com_bsw_write1(bs, sqh->patch_stable);
    if (sqh->patch_stable)
    {
        com_bsw_write1(bs, sqh->patch_uniform);
        if (sqh->patch_uniform)
        {
            com_bsw_write1(bs, 1); // marker_bit
            com_bsw_write_ue(bs, sqh->patch_width_minus1);
            com_bsw_write_ue(bs, sqh->patch_height_minus1);
        }
    }
#endif
    com_bsw_write(bs, 0, 2); // reserved_bits r(2)

    com_bsw_write1(bs, 1);   // stuffing_bit '1'
    while(!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        com_bsw_write1(bs, 0);  // stuffing_bit '0'
    }
    return COM_OK;
}

void Demulate(COM_BSW * bs)
{
    unsigned int uiZeroCount = 0;
    unsigned char ucHeaderType;
    unsigned int uiStartPos = 0;
    bs->fn_flush(bs);
    bs->leftbits = bs->leftbits % 8;
    unsigned int current_bytes_size = COM_BSW_GET_WRITE_BYTE(bs);

    //stuffing bit '1'
    int stuffbitnum = bs->leftbits;
    if (stuffbitnum > 0)
    {
        bs->beg[current_bytes_size - 1] = bs->beg[current_bytes_size - 1] & (~(1 << (8 - stuffbitnum))) << stuffbitnum;
        bs->beg[current_bytes_size - 1] = bs->beg[current_bytes_size - 1] | (1 << (stuffbitnum - 1));
    }
    else
    {
        bs->beg[current_bytes_size++] = 0x80;
    }

    if (bs->beg[uiStartPos] == 0x00 && bs->beg[uiStartPos + 1] == 0x00 && bs->beg[uiStartPos + 2] == 0x01)
    {
        uiStartPos += 3;
        ucHeaderType = bs->beg[uiStartPos++]; //HeaderType
        if (ucHeaderType == 0x00)
        {
            uiZeroCount++;
        }
    }
    else
    {
        printf("Wrong start code!");
        exit(1);
    }

    unsigned int uiWriteOffset = uiStartPos;
    unsigned int uiBitsWriteOffset = 0;

    switch (ucHeaderType)
    {
    case 0xb5:
        if ((bs->beg[uiStartPos] >> 4) == 0x0d)
        {
            break;
        }
        else
        {
        }
    case 0xb0: //
    case 0xb2: //
        return;
    default:
        break;
    }
    /*write head info*/
    bs->buftmp[0] = 0x00;
    bs->buftmp[1] = 0x00;
    bs->buftmp[2] = 0x01;
    bs->buftmp[3] = ucHeaderType;
    /*demulate*/
    for (unsigned int uiReadOffset = uiStartPos; uiReadOffset < current_bytes_size; uiReadOffset++)
    {
        unsigned char ucCurByte = (bs->beg[uiReadOffset - 1] << (8 - uiBitsWriteOffset)) | (bs->beg[uiReadOffset] >> uiBitsWriteOffset);
        if (2 <= uiZeroCount && 0 == (ucCurByte & 0xfc))
        {
            bs->buftmp[uiWriteOffset++] = 0x02;
            uiBitsWriteOffset += 2;
            uiZeroCount = 0;
            if (uiBitsWriteOffset >= 8)
            {
                uiBitsWriteOffset = 0;
                uiReadOffset--;
            }
            continue;
        }
        bs->buftmp[uiWriteOffset++] = ucCurByte;

        if (0 == ucCurByte)
        {
            uiZeroCount++;
        }
        else
        {
            uiZeroCount = 0;
        }
    }

    if (uiBitsWriteOffset != 0)
    {
        /*get the last several bits*/
        unsigned char  ucCurByte = bs->beg[current_bytes_size - 1] << (8 - uiBitsWriteOffset);
        bs->buftmp[uiWriteOffset++] = ucCurByte;
    }

    for (unsigned int i = 0; i < uiWriteOffset; i++)
    {
        bs->beg[i] = bs->buftmp[i];
    }

    bs->cur = bs->beg + uiWriteOffset;
    bs->code = 0;
    bs->leftbits = 32;
}

int enc_eco_pic_header(COM_BSW * bs, COM_PIC_HEADER * pic_header, COM_SQH * sqh)
{
    //intra_picture_start_code or inter_picture_start_code
    com_bsw_write(bs, 0x000001, 24);
    com_bsw_write(bs, pic_header->slice_type == SLICE_I ? 0xB3 : 0xB6, 8);
    if (pic_header->slice_type != SLICE_I)
    {
      com_bsw_write1(bs, pic_header->random_access_decodable_flag);
    }
    //bbv_delay
    com_bsw_write(bs, pic_header->bbv_delay >> 24, 8);
    com_bsw_write(bs, (pic_header->bbv_delay & 0x00ff0000) >> 16, 8);
    com_bsw_write(bs, (pic_header->bbv_delay & 0x0000ff00) >> 8, 8);
    com_bsw_write(bs, pic_header->bbv_delay & 0xff, 8);

    if (pic_header->slice_type != SLICE_I)
    {
        //picture_coding_type
        assert(pic_header->slice_type == SLICE_P || pic_header->slice_type == SLICE_B);
        com_bsw_write(bs, pic_header->slice_type == SLICE_B ? 2 : 1, 2);
    }
    else
    {
        //time_code_flag & time_code
        com_bsw_write1(bs, pic_header->time_code_flag);
        if (pic_header->time_code_flag == 1)
        {
            com_bsw_write(bs, pic_header->time_code, 24);
        }
    }

    com_bsw_write(bs, pic_header->decode_order_index%DOI_CYCLE_LENGTH, 8);//MX: algin with the spec, should no affect the results 
#if LIBVC_ON
    if (pic_header->slice_type == SLICE_I)
    {
        if (sqh->library_stream_flag)
        {
            com_bsw_write_ue(bs, pic_header->library_picture_index);
        }
    }
#endif
    if (sqh->temporal_id_enable_flag == 1)
    {
        com_bsw_write(bs, pic_header->temporal_id, 3);
    }
    if (sqh->low_delay == 0)
    {
        com_bsw_write_ue(bs, pic_header->picture_output_delay);
    }

    //ref_pic_list_struct_flag...missing

    if (sqh->low_delay == 1)
    {
        com_bsw_write_ue(bs, pic_header->bbv_check_times);
    }

    //the field information below is not used by decoder -- start
    com_bsw_write1(bs, pic_header->progressive_frame);
    assert(pic_header->progressive_frame == 1);
    if (pic_header->progressive_frame == 0)
    {
        com_bsw_write1(bs, pic_header->picture_structure);
    }
    else
    {
        assert(pic_header->picture_structure == 1);
    }
    com_bsw_write1(bs, pic_header->top_field_first);
    com_bsw_write1(bs, pic_header->repeat_first_field);
    if (sqh->field_coded_sequence == 1)
    {
        com_bsw_write1(bs, pic_header->top_field_picture_flag);
        com_bsw_write1(bs, 0); // reserved_bits r(1)
    }
    // -- end
#if HLS_RPL
    //TBD(@Chernyak) if(!IDR) condition to be added here

    //com_bsw_write_ue(bs, sh->poc); //TBD(@Chernyak) align with spec

    // L0 candidates signaling

    com_bsw_write1(bs, pic_header->ref_pic_list_sps_flag[0]);

    if (pic_header->ref_pic_list_sps_flag[0])
    {
        if (sqh->rpls_l0_num > 1)
        {
            com_bsw_write_ue(bs, pic_header->rpl_l0_idx);
        }
        else if (sqh->rpls_l0_num == 1)
        {
            assert(pic_header->rpl_l0_idx == 0);
        }
        else
            return COM_ERR;   //This case shall not happen!
    }
    else
    {
#if LIBVC_ON
        appe_eco_rlp(bs, &pic_header->rpl_l0, sqh);
#else
        appe_eco_rlp(bs, &pic_header->rpl_l0);
#endif
    }

    // L1 candidates signaling
    {
        if (sqh->rpl1_index_exist_flag)
        {
            com_bsw_write1(bs, pic_header->ref_pic_list_sps_flag[1]);
        }
        if (pic_header->ref_pic_list_sps_flag[1])
        {
            if (sqh->rpls_l1_num > 1 && sqh->rpl1_index_exist_flag)
            {
                com_bsw_write_ue(bs, pic_header->rpl_l1_idx);
            }
            else if (!sqh->rpl1_index_exist_flag)
            {
                assert(pic_header->rpl_l1_idx == pic_header->rpl_l0_idx);
            }
            else if (sqh->rpls_l1_num == 1)
            {
                assert(pic_header->rpl_l1_idx == 0);//OK
            }
            else
                return COM_ERR;   //This case shall not happen!
        }
        else
        {
#if LIBVC_ON
            appe_eco_rlp(bs, &pic_header->rpl_l1, sqh);
#else
            appe_eco_rlp(bs, &pic_header->rpl_l1);
#endif
        }
    }


    if (pic_header->slice_type != SLICE_I)
    {
        com_bsw_write1(bs, pic_header->num_ref_idx_active_override_flag);
        if (pic_header->num_ref_idx_active_override_flag)
        {
            com_bsw_write_ue(bs, (u32)(pic_header->rpl_l0).ref_pic_active_num - 1);
            if (pic_header->slice_type == SLICE_B)
            {
                com_bsw_write_ue(bs, (u32)(pic_header->rpl_l1).ref_pic_active_num - 1);
            }
        }
    }
#endif

    com_bsw_write1(bs, pic_header->fixed_picture_qp_flag);
    com_bsw_write(bs, pic_header->picture_qp, 7);
    if( pic_header->slice_type != SLICE_I && !(pic_header->slice_type == SLICE_B && pic_header->picture_structure == 1) )
    {
        com_bsw_write1( bs, 0 ); // reserved_bits r(1)
    }
    enc_eco_DB_param(bs, pic_header);

    com_bsw_write1(bs, pic_header->chroma_quant_param_disable_flag);

    if (pic_header->chroma_quant_param_disable_flag == 0)
    {
        com_bsw_write_se(bs, pic_header->chroma_quant_param_delta_cb);
        com_bsw_write_se(bs, pic_header->chroma_quant_param_delta_cr);
    }

    if (sqh->wq_enable)
    {
        com_bsw_write1(bs, pic_header->pic_wq_enable);
        if (pic_header->pic_wq_enable)
        {
            com_bsw_write(bs, pic_header->pic_wq_data_idx, 2);
            if (pic_header->pic_wq_data_idx == 2)
            {
                write_wq_matrix(bs, pic_header->wq_4x4_matrix, pic_header->wq_8x8_matrix);
            }
            else if (pic_header->pic_wq_data_idx == 1)
            {
                int i;
                com_bsw_write1(bs, 0); //reserved_bits r(1)
                com_bsw_write(bs, pic_header->wq_param, 2); //weight_quant_param_index
                com_bsw_write(bs, pic_header->wq_model, 2); //weight_quant_model

                if (pic_header->wq_param == 1)
                {
                    for (i = 0; i < 6; i++)
                    {
                        com_bsw_write_se(bs, pic_header->wq_param_vector[i] - tab_wq_param_default[0][i]);
                    }
                }
                else if (pic_header->wq_param == 2)
                {
                    for (i = 0; i < 6; i++)
                    {
                        com_bsw_write_se(bs, pic_header->wq_param_vector[i] - tab_wq_param_default[1][i]);
                    }
                }
            }
        }
    }

    if (pic_header->tool_alf_on)
    {
        /* Encode ALF flag and ALF coeff */
        enc_eco_ALF_param(bs, pic_header);
    }

    if (pic_header->slice_type != SLICE_I && sqh->affine_enable_flag)
    {
        com_bsw_write(bs, pic_header->affine_subblock_size_idx, 1);
    }

    Demulate(bs);
    return COM_OK;
}

int enc_eco_patch_header(COM_BSW * bs, COM_SQH *sqh, COM_PIC_HEADER *ph, COM_SH_EXT * sh,u8 patch_idx, PATCH_INFO* patch)
{
    //patch_start_code_prefix
    com_bsw_write(bs, 0x000001, 24);
    //patch_index
    com_bsw_write(bs, patch_idx, 8);

    if (!ph->fixed_picture_qp_flag)
    {
        com_bsw_write1(bs, sh->fixed_slice_qp_flag);
        com_bsw_write(bs, sh->slice_qp, 7);
    }
    s8* sao_enable_patch = patch->patch_sao_enable + patch_idx*N_C;
    if (sqh->sample_adaptive_offset_enable_flag)
    {
        com_bsw_write1(bs, sao_enable_patch[Y_C]);
        com_bsw_write1(bs, sao_enable_patch[U_C]);
        com_bsw_write1(bs, sao_enable_patch[V_C]);
    }

    /* byte align */
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        com_bsw_write1(bs, 1);
    }

    return COM_OK;
}
#if PATCH
int enc_eco_send(COM_BSW * bs)
{
    com_bsw_write(bs, 0x000001, 24);
    com_bsw_write(bs, 0x8F, 8);
    return COM_OK;
}
#endif

int enc_eco_udata(ENC_CTX * ctx, COM_BSW * bs)
{
    int ret, i;
    /* should be aligned before adding user data */
    com_assert_rv(COM_BSR_IS_BYTE_ALIGN(bs), COM_ERR_UNKNOWN);
    /* picture signature */
    if(ctx->param.use_pic_sign)
    {
        u8 pic_sign[16];
        /* should be aligned before adding user data */
        com_assert_rv(COM_BSR_IS_BYTE_ALIGN(bs), COM_ERR_UNKNOWN);
        /* get picture signature */
        ret = com_picbuf_signature(PIC_REC(ctx), pic_sign);
        com_assert_rv(ret == COM_OK, ret);
        /* write user data type */
        com_bsw_write(bs, COM_UD_PIC_SIGNATURE, 8);
        /* embed signature (HASH) to bitstream */
        for(i = 0; i < 16; i++)
        {
            com_bsw_write(bs, pic_sign[i], 8);
            if (i % 2 == 1)
            {
                com_bsw_write1(bs, 1);
            }
        }
    }
    /* write end of user data syntax */
    com_bsw_write(bs, COM_UD_END, 8);
    return COM_OK;
}

static void com_bsw_write_est(ENC_SBAC *sbac, int len)
{
    sbac->bitcounter += len;
}

static void sbac_put_byte (u8 writing_byte, ENC_SBAC *sbac, COM_BSW *bs)
{
    if(sbac->is_pending_byte)
    {
        if (sbac->is_bitcount)
            com_bsw_write_est(sbac, 8);
        else
            com_bsw_write(bs, sbac->pending_byte, 8);
    }
    sbac->pending_byte = writing_byte;
    sbac->is_pending_byte = 1;
}

static void sbac_carry_propagate (ENC_SBAC *sbac, COM_BSW *bs)
{
    u32 leadByte = (sbac->code) >> (24 - sbac->left_bits);
    sbac->left_bits += 8;
    (sbac->code) &= (0xffffffffu >> sbac->left_bits);
    if(leadByte < 0xFF)
    {
        while(sbac->stacked_ff != 0)
        {
            sbac_put_byte(0xFF, sbac, bs);
            sbac->stacked_ff--;
        }
        sbac_put_byte((u8)leadByte, sbac, bs);
    }
    else if(leadByte > 0xFF)
    {
        sbac->pending_byte++; //! add carry bit to pending_byte
        while(sbac->stacked_ff != 0)
        {
            sbac_put_byte(0x00, sbac, bs); //! write pending_tyte
            sbac->stacked_ff--;
        }
        sbac_put_byte((u8)leadByte & 0xFF, sbac, bs);
    }
    else //! leadByte == 0xff
    {
        sbac->stacked_ff++;
    }
}

static void sbac_encode_bin_ep(u32 bin, ENC_SBAC *sbac, COM_BSW *bs)
{
#if TRACE_BIN
    COM_TRACE_COUNTER;
    COM_TRACE_STR("range ");
    COM_TRACE_INT(sbac->range);
    COM_TRACE_STR("\n");
#endif
    (sbac->code) <<= 1;
    if (bin != 0)
    {
        (sbac->code) += (sbac->range);
    }
    if(--(sbac->left_bits) < 12)
    {
        sbac_carry_propagate(sbac, bs);
    }
}

static void sbac_write_unary_sym_ep(u32 sym, ENC_SBAC *sbac, COM_BSW *bs)
{
    u32 ctx_idx = 0;

    do
    {
        sbac_encode_bin_ep(sym ? 0 : 1, sbac, bs);
        ctx_idx++;
    }
    while (sym--);
}

static void sbac_write_unary_sym(u32 sym, u32 num_ctx, ENC_SBAC *sbac, SBAC_CTX_MODEL *model, COM_BSW *bs)
{
    u32 ctx_idx = 0;

    do
    {
        enc_sbac_encode_bin(sym ? 0 : 1, sbac, model + min(ctx_idx, num_ctx - 1), bs);
        ctx_idx++;
    }
    while (sym--);
}

static void sbac_write_truncate_unary_sym(u32 sym, u32 num_ctx, u32 max_num, ENC_SBAC *sbac, SBAC_CTX_MODEL *model, COM_BSW *bs)
{
    u32 ctx_idx = 0;

    do
    {
        enc_sbac_encode_bin(sym ? 0 : 1, sbac, model + min(ctx_idx, num_ctx - 1), bs);
        ctx_idx++;
    }
    while (ctx_idx < max_num - 1 && sym--);
}

static void sbac_encode_bins_ep_msb(u32 value, int num_bin, ENC_SBAC *sbac, COM_BSW *bs)
{
    int bin = 0;
    for(bin = num_bin - 1; bin >= 0; bin--)
    {
        sbac_encode_bin_ep(value & (1 << (u32)bin), sbac, bs);
    }
}

static __inline int ace_get_shift(int v)
{
#ifdef _WIN32
    unsigned long index;
    _BitScanReverse(&index, v);
    return 8 - index;
#else
    return __builtin_clz(v) - 23;
#endif
}

static __inline int ace_get_log2(int v)
{
#ifdef _WIN32
    unsigned long index;
    _BitScanReverse(&index, v);
    return index;
#else
    return 31 - __builtin_clz(v);
#endif
}

void enc_sbac_encode_bin(u32 bin, ENC_SBAC *sbac, SBAC_CTX_MODEL *model, COM_BSW *bs)
{
#if TRACE_BIN
    SBAC_CTX_MODEL prev_model = *model;
#endif
    u16 prob_lps = ((*model) & PROB_MASK) >> 1;
    u16 cmps = (*model) & 1;
    u32 rLPS = prob_lps >> LG_PMPS_SHIFTNO;
    u32 rMPS = sbac->range - rLPS;
    int s_flag = rMPS < QUAR_HALF_PROB;
    rMPS |= 0x100;
    assert(sbac->range >= rLPS); //! this maybe triggered, so it can be removed
    if (bin != cmps)
    {
        rLPS = (sbac->range << s_flag) - rMPS;
        int shift = ace_get_shift(rLPS);
        sbac->range = rLPS << shift;
        sbac->code = ((sbac->code << s_flag) + rMPS) << shift;
        sbac->left_bits -= (shift + s_flag);
        if (sbac->left_bits < 12)
        {
            sbac_carry_propagate(sbac, bs);
        }
        *model = tab_cycno_lgpmps_mps[(*model) | (1 << 13)];
    }
    else //! MPS
    {
        if (s_flag)
        {
            sbac->code <<= 1;
            if (--sbac->left_bits < 12)
            {
                sbac_carry_propagate(sbac, bs);
            }
        }
        sbac->range = rMPS;
        *model = tab_cycno_lgpmps_mps[*model];
    }
#if TRACE_BIN
    COM_TRACE_COUNTER;
    COM_TRACE_STR("model ");
    COM_TRACE_INT(prev_model);
    COM_TRACE_STR("-->");
    COM_TRACE_INT(*model);
    COM_TRACE_STR("MPS Range ");
    COM_TRACE_INT(sbac->range);
    COM_TRACE_STR("LPS Range ");
    COM_TRACE_INT(rLPS);
    COM_TRACE_STR("\n");
#endif
}

void enc_sbac_encode_binW(u32 bin, ENC_SBAC *sbac, SBAC_CTX_MODEL *model1, SBAC_CTX_MODEL *model2, COM_BSW *bs)
{
#if 0//TRACE_BIN
    SBAC_CTX_MODEL prev_model = *model;
#endif
    u16 prob_lps;
    u16 prob_lps1 = ((*model1) & PROB_MASK) >> 1;
    u16 prob_lps2 = ((*model2) & PROB_MASK) >> 1;
    u16 cmps;
    u16 cmps1 = (*model1) & 1;
    u16 cmps2 = (*model2) & 1;
    u32 rLPS;
    u32 rMPS;
    int s_flag;

    if (cmps1 == cmps2)
    {
        cmps = cmps1;
        prob_lps = (prob_lps1 + prob_lps2) >> 1;
    }
    else
    {
        if (prob_lps1 < prob_lps2)
        {
            cmps = cmps1;
            prob_lps = (256 << LG_PMPS_SHIFTNO) - 1 - ((prob_lps2 - prob_lps1) >> 1);
        }
        else
        {
            cmps = cmps2;
            prob_lps = (256 << LG_PMPS_SHIFTNO) - 1 - ((prob_lps1 - prob_lps2) >> 1);
        }
    }

    rLPS = prob_lps >> LG_PMPS_SHIFTNO;

    assert(sbac->range >= rLPS); //! this maybe triggered, so it can be removed

    rMPS = sbac->range - rLPS;
    s_flag = rMPS < QUAR_HALF_PROB;
    rMPS |= 0x100;

    if (bin != cmps)
    {
        rLPS = (sbac->range << s_flag) - rMPS;
        int shift = ace_get_shift(rLPS);
        sbac->range = rLPS << shift;
        sbac->code = ((sbac->code << s_flag) + rMPS) << shift;
        sbac->left_bits -= (shift + s_flag);
        if (sbac->left_bits < 12)
        {
            sbac_carry_propagate(sbac, bs);
        }

    }
    else //! MPS
    {
        if (s_flag)
        {
            sbac->code <<= 1;
            if (--sbac->left_bits < 12)
            {
                sbac_carry_propagate(sbac, bs);
            }
        }
        sbac->range = rMPS;

    }

    if (bin != cmps1)
    {
        *model1 = tab_cycno_lgpmps_mps[(*model1) | (1 << 13)];
    }
    else
    {
        *model1 = tab_cycno_lgpmps_mps[*model1];
    }
    if (bin != cmps2)
    {
        *model2 = tab_cycno_lgpmps_mps[(*model2) | (1 << 13)];
    }
    else
    {
        *model2 = tab_cycno_lgpmps_mps[*model2];
    }

#if 0//TRACE_BIN
    COM_TRACE_COUNTER;
    COM_TRACE_STR("model ");
    COM_TRACE_INT(prev_model);
    COM_TRACE_STR("-->");
    COM_TRACE_INT(*model);
    COM_TRACE_STR("MPS Range ");
    COM_TRACE_INT(sbac->range);
    COM_TRACE_STR("LPS Range ");
    COM_TRACE_INT(rLPS);
    COM_TRACE_STR("\n");
#endif
}


void enc_sbac_encode_bin_trm(u32 bin, ENC_SBAC *sbac, COM_BSW *bs)
{
    int s_flag = (sbac->range == QUAR_HALF_PROB);
    u32 rMPS = (sbac->range - 1) | 0x100;
    (sbac->range) -= 2;
    if(bin)
    {
        sbac->range = QUAR_HALF_PROB;
        sbac->code = ((sbac->code << s_flag) + rMPS) << 8;
        sbac->left_bits -= (8 + s_flag);
        if (sbac->left_bits < 12)
        {
            sbac_carry_propagate(sbac, bs);
        }
    }
    else
    {
        if (s_flag)
        {
            sbac->code <<= 1;
            if (--sbac->left_bits < 12)
            {
                sbac_carry_propagate(sbac, bs);
            }
        }
        sbac->range = rMPS;
    }
}

void enc_sbac_init(COM_BSW * bs)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    sbac->range = 0x1FF;
    sbac->code = 0;
    sbac->left_bits = 23;
    sbac->pending_byte = 0;
    sbac->is_pending_byte = 0;
    sbac->stacked_ff = 0;
}

void enc_sbac_finish(COM_BSW *bs, int is_ipcm)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    if(sbac->code >> (32-sbac->left_bits))
    {
        assert(sbac->pending_byte != 0xff);
        com_bsw_write(bs, sbac->pending_byte + 1, 8);
        while (sbac->stacked_ff != 0)
        {
            com_bsw_write(bs, 0x00, 8);
            sbac->stacked_ff--;
        }
        sbac->code -= 1 << (32 - sbac->left_bits);
    }
    else
    {
#if IPCM
        if (sbac->is_pending_byte)
        {
#endif
            com_bsw_write(bs, sbac->pending_byte, 8);
#if IPCM
        }
#endif
        while (sbac->stacked_ff != 0)
        {
            com_bsw_write(bs, 0xFF, 8);
            sbac->stacked_ff--;
        }
    }
    sbac->code |= (1 << 7);
    com_bsw_write(bs, sbac->code >> 8, 24 - sbac->left_bits);

    //if ((23 - sbac->left_bits) % 8)
    if (is_ipcm || (24 - sbac->left_bits) % 8) // write the last byte of low in the end of CABAC, if the number of used bits (23 - left_bits) + 1 is not exactly bytes (Nx8), corresponding to bits_Needed != 0
    {
        com_bsw_write(bs, sbac->code, 8);
    }

    if (!is_ipcm)
    {
        //add termination slice padding bits
        com_bsw_write(bs, 1, 1);
    }
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        com_bsw_write(bs, 0, 1);
    }
}

void encode_affine_flag(COM_BSW * bs, int flag, ENC_CTX * ctx)
{
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    if (mod_info_curr->cu_width >= AFF_SIZE && mod_info_curr->cu_height >= AFF_SIZE && ctx->info.sqh.affine_enable_flag)
    {
        enc_sbac_encode_bin(flag, sbac, sbac->ctx.affine_flag, bs);
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("affine flag ");
    COM_TRACE_INT(flag);
    COM_TRACE_STR("\n");
}

void encode_affine_mrg_idx( COM_BSW * bs, s16 affine_mrg_idx, ENC_CTX * ctx )
{
    ENC_SBAC * sbac = GET_SBAC_ENC(bs);
    sbac_write_truncate_unary_sym( affine_mrg_idx, NUM_SBAC_CTX_AFFINE_MRG, AFF_MAX_NUM_MRG, sbac, sbac->ctx.affine_mrg_idx, bs );

    COM_TRACE_COUNTER;
    COM_TRACE_STR("affine merge index ");
    COM_TRACE_INT(affine_mrg_idx);
    COM_TRACE_STR("\n");
}

#if SMVD
void encode_smvd_flag( COM_BSW * bs, int flag )
{
    ENC_SBAC *sbac;
    sbac = GET_SBAC_ENC( bs );
    enc_sbac_encode_bin( flag, sbac, sbac->ctx.smvd_flag, bs );

    COM_TRACE_COUNTER;
    COM_TRACE_STR("smvd flag ");
    COM_TRACE_INT(flag);
    COM_TRACE_STR("\n");
}
#endif

#if DT_SYNTAX
void encode_part_size(ENC_CTX *ctx, COM_BSW * bs, int part_size, int cu_w, int cu_h, int pred_mode)
{
    ENC_SBAC *sbac;
    sbac = GET_SBAC_ENC(bs);
    int allowDT = com_dt_allow(cu_w, cu_h, pred_mode, ctx->info.sqh.max_dt_size);
    int sym, dir, eq;

    if (!ctx->info.sqh.dt_intra_enable_flag && pred_mode == MODE_INTRA)
        return;
    if (!allowDT)
        return;

    sym = part_size != SIZE_2Nx2N;
    enc_sbac_encode_bin(sym, sbac, sbac->ctx.part_size + 0, bs);
    if (sym == 1)
    {
        int hori_allow = (allowDT >> 0) & 0x01;
        int vert_allow = (allowDT >> 1) & 0x01;
        dir = part_size == SIZE_2NxhN || part_size == SIZE_2NxnD || part_size == SIZE_2NxnU;
        if (hori_allow && vert_allow)
        {
            enc_sbac_encode_bin(dir, sbac, sbac->ctx.part_size + 1, bs);
        }
        else
            assert( dir == hori_allow);

        if (dir)
        {
            //hori
            eq = part_size == SIZE_2NxhN;
            enc_sbac_encode_bin(eq, sbac, sbac->ctx.part_size + 2, bs);
            if (eq)
            {
                assert(part_size == SIZE_2NxhN);
            }
            else
            {
                assert(part_size == SIZE_2NxnD || part_size == SIZE_2NxnU);
                sym = part_size == SIZE_2NxnD;
                enc_sbac_encode_bin(sym, sbac, sbac->ctx.part_size + 3, bs);
            }
        }
        else
        {
            //vert
            eq = part_size == SIZE_hNx2N;
            enc_sbac_encode_bin(eq, sbac, sbac->ctx.part_size + 4, bs);

            if (eq)
            {
                assert(part_size == SIZE_hNx2N);
            }
            else
            {
                assert(part_size == SIZE_nRx2N || part_size == SIZE_nLx2N);
                sym = part_size == SIZE_nRx2N;
                enc_sbac_encode_bin(sym, sbac, sbac->ctx.part_size + 5, bs);
            }
        }
    }
    else
    {
        assert(part_size == SIZE_2Nx2N);
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("part_size ");
    COM_TRACE_INT(part_size);
    COM_TRACE_STR("\n");
}
#endif

void enc_eco_split_flag(ENC_CTX *c, int cu_width, int cu_height, int x, int y, COM_BSW * bs, ENC_SBAC *sbac, int flag)
{
    //split flag
    int ctx = 0;
    int x_scu = x >> MIN_CU_LOG2;
    int y_scu = y >> MIN_CU_LOG2;
    int pic_width_in_scu = c->info.pic_width >> MIN_CU_LOG2;
    u8  avail[2] = { 0, 0};
    int scun[2];
    int scup = x_scu + y_scu * pic_width_in_scu;

    scun[0] = scup - pic_width_in_scu;
    scun[1] = scup - 1;
    if (y_scu > 0)
        avail[0] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[0]]); //up
    if (x_scu > 0)
        avail[1] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[1]]); //left

    if (avail[0])
        ctx += (1 << MCU_GET_LOGW(c->map.map_cu_mode[scun[0]])) < cu_width;
    if (avail[1])
        ctx += (1 << MCU_GET_LOGH(c->map.map_cu_mode[scun[1]])) < cu_height;

#if SEP_CONTEXT
    if (c->info.pic_header.slice_type == SLICE_I && cu_width == 128 && cu_height == 128)
    {
        ctx = 3;
        assert(flag == 1);
    }
#endif

    enc_sbac_encode_bin(flag, sbac, sbac->ctx.split_flag + ctx, bs);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("split flag ");
    COM_TRACE_INT(flag);
    COM_TRACE_STR("\n");
}


#if MODE_CONS
void enc_eco_cons_pred_mode_child(COM_BSW * bs, u8 cons_pred_mode_child)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    assert(cons_pred_mode_child == ONLY_INTER || cons_pred_mode_child == ONLY_INTRA);
    u8 flag = cons_pred_mode_child == ONLY_INTRA;
    enc_sbac_encode_bin(flag, sbac, sbac->ctx.cons_mode, bs);

    COM_TRACE_COUNTER;
    COM_TRACE_STR("cons mode ");
    COM_TRACE_INT(cons_pred_mode_child);
    COM_TRACE_STR("\n");
}
#endif

void encode_skip_flag(COM_BSW * bs, ENC_SBAC *sbac, int flag, ENC_CTX * ctx)
{
#if NUM_SBAC_CTX_SKIP_FLAG > 1
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    u32 * map_scu = ctx->map.map_scu;
    u8  avail[2] = { 0, 0 };
    int scun[2];
    int ctx_inc = 0;
    scun[0] = mod_info_curr->scup - ctx->info.pic_width_in_scu;
    scun[1] = mod_info_curr->scup - 1;

    if (mod_info_curr->y_scu > 0)
        avail[0] = MCU_GET_CODED_FLAG(map_scu[scun[0]]); // up
    if (mod_info_curr->x_scu > 0)
        avail[1] = MCU_GET_CODED_FLAG(map_scu[scun[1]]); // left
    if (avail[0])
        ctx_inc += MCU_GET_SF(map_scu[scun[0]]);
    if (avail[1])
        ctx_inc += MCU_GET_SF(map_scu[scun[1]]);

#if SEP_CONTEXT
    if (mod_info_curr->cu_width_log2 + mod_info_curr->cu_height_log2 < 6)
    {
        assert(flag == 0);
        ctx_inc = 3;
    }
#endif

    enc_sbac_encode_bin(flag, sbac, sbac->ctx.skip_flag + ctx_inc, bs);
#else
    enc_sbac_encode_bin(flag, sbac, sbac->ctx.skip_flag, bs);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("skip flag ");
    COM_TRACE_INT(flag);
#if NUM_SBAC_CTX_SKIP_FLAG > 1
    COM_TRACE_STR(" skip ctx ");
    COM_TRACE_INT(ctx_inc);
#endif
    COM_TRACE_STR("\n");
}

void encode_umve_flag(COM_BSW * bs, int flag)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    enc_sbac_encode_bin(flag, sbac, &sbac->ctx.umve_flag, bs);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("umve flag ");
    COM_TRACE_INT(flag);
    COM_TRACE_STR("\n");
}

void encode_umve_idx(COM_BSW * bs, int umve_idx)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    int idx;
    int base_idx = umve_idx / UMVE_MAX_REFINE_NUM;
    int ref_step = (umve_idx - (base_idx * UMVE_MAX_REFINE_NUM)) / 4;
    int direction = umve_idx - base_idx * UMVE_MAX_REFINE_NUM - ref_step * 4;

    int num_cand_minus1_base = UMVE_BASE_NUM - 1;
    if (num_cand_minus1_base > 0)
    {

        if (base_idx == 0)
            enc_sbac_encode_bin(1, sbac, sbac->ctx.umve_base_idx, bs);
        else
        {
            enc_sbac_encode_bin(0, sbac, sbac->ctx.umve_base_idx, bs);
            for (idx = 1; idx < num_cand_minus1_base; idx++)
            {
                {
                    sbac_encode_bin_ep(base_idx == idx ? 1 : 0, sbac, bs);
                }
                if (base_idx == idx)
                {
                    break;
                }
            }
        }
    }

    int num_cand_minus1_step = UMVE_REFINE_STEP - 1;
    if (num_cand_minus1_step > 0)
    {
        if (ref_step == 0)
            enc_sbac_encode_bin(1, sbac, sbac->ctx.umve_step_idx, bs);
        else
        {
            enc_sbac_encode_bin(0, sbac, sbac->ctx.umve_step_idx, bs);
            for (idx = 1; idx < num_cand_minus1_step; idx++)
            {
                {
                    sbac_encode_bin_ep(ref_step == idx ? 1 : 0, sbac, bs);
                }
                if (ref_step == idx)
                {
                    break;
                }
            }
        }
    }

    if (direction == 0)
    {
        enc_sbac_encode_bin(0, sbac, &sbac->ctx.umve_dir_idx[0], bs);
        enc_sbac_encode_bin(0, sbac, &sbac->ctx.umve_dir_idx[1], bs);
    }
    else if (direction == 1)
    {
        enc_sbac_encode_bin(0, sbac, &sbac->ctx.umve_dir_idx[0], bs);
        enc_sbac_encode_bin(1, sbac, &sbac->ctx.umve_dir_idx[1], bs);
    }
    else if (direction == 2)
    {
        enc_sbac_encode_bin(1, sbac, &sbac->ctx.umve_dir_idx[0], bs);
        enc_sbac_encode_bin(0, sbac, &sbac->ctx.umve_dir_idx[1], bs);
    }
    else if (direction == 3)
    {
        enc_sbac_encode_bin(1, sbac, &sbac->ctx.umve_dir_idx[0], bs);
        enc_sbac_encode_bin(1, sbac, &sbac->ctx.umve_dir_idx[1], bs);
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("umve idx ");
    COM_TRACE_INT(umve_idx);
    COM_TRACE_STR("\n");
}

void encode_skip_idx(COM_BSW *bs, int skip_idx, int num_hmvp_cands, ENC_CTX * ctx)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    int ctx_idx = 0;

    // for P slice, change 3, 4, ..., 13 to 1, 2, ..., 11
    if (ctx->info.pic_header.slice_type == SLICE_P && skip_idx > 0)
    {
        assert(skip_idx >= 3);
        skip_idx -= 2;
    }

    int val = skip_idx;
    int max_skip_num = (ctx->info.pic_header.slice_type == SLICE_P ? 2 : TRADITIONAL_SKIP_NUM) + num_hmvp_cands;
    assert(skip_idx < max_skip_num);
    while (val-- > 0)
    {
        ctx_idx = min(ctx_idx, NUM_SBAC_CTX_SKIP_IDX - 1);
        enc_sbac_encode_bin(0, sbac, &sbac->ctx.skip_idx_ctx[ctx_idx], bs);
        ctx_idx++;
    }
    if (skip_idx != max_skip_num - 1)
    {
        ctx_idx = min(ctx_idx, NUM_SBAC_CTX_SKIP_IDX - 1);
        enc_sbac_encode_bin(1, sbac, &sbac->ctx.skip_idx_ctx[ctx_idx], bs);
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("skip idx ");
    COM_TRACE_INT(skip_idx);
    COM_TRACE_STR(" HmvpSpsNum ");
    COM_TRACE_INT(num_hmvp_cands);
    COM_TRACE_STR("\n");
}

void encode_direct_flag(COM_BSW *bs, int direct_flag, ENC_CTX * ctx)
{
    ENC_SBAC *sbac;
    sbac = GET_SBAC_ENC(bs);
#if SEP_CONTEXT
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    int ctx_inc = 0;
    if ((mod_info_curr->cu_width_log2 + mod_info_curr->cu_height_log2 < 6) || mod_info_curr->cu_width_log2 > 6 || mod_info_curr->cu_height_log2 > 6)
    {
        assert(direct_flag == 0);
        ctx_inc = 1;
    }

    enc_sbac_encode_bin(direct_flag, sbac, sbac->ctx.direct_flag + ctx_inc, bs);
#else
    enc_sbac_encode_bin(direct_flag, sbac, sbac->ctx.direct_flag, bs);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("direct flag ");
    COM_TRACE_INT(direct_flag);
#if SEP_CONTEXT
    COM_TRACE_STR(" direct flag ctx ");
    COM_TRACE_INT(ctx_inc);
#endif
    COM_TRACE_STR("\n");
}

void enc_eco_slice_end_flag(COM_BSW * bs, int flag)
{
    ENC_SBAC *sbac;
    sbac = GET_SBAC_ENC(bs);
    enc_sbac_encode_bin_trm(flag, sbac, bs);
}

static int enc_eco_run(u32 sym, ENC_SBAC *sbac, SBAC_CTX_MODEL *model, COM_BSW *bs)
{
    int exp_golomb_order = 0;

    COM_TRACE_COUNTER;
    COM_TRACE_STR("run:");
    COM_TRACE_INT(sym);
    COM_TRACE_STR("\n");

    if (sym < 16)
    {
        sbac_write_truncate_unary_sym(sym, 2, 17, sbac, model, bs);
    }
    else
    {
        sym -= 16;

        sbac_write_truncate_unary_sym(16, 2, 17, sbac, model, bs);

        // exp_golomb part
        while ((int)sym >= (1 << exp_golomb_order))
        {
            sym = sym - (1 << exp_golomb_order);
            exp_golomb_order++;
        }

        sbac_write_unary_sym_ep(exp_golomb_order, sbac, bs);
        sbac_encode_bins_ep_msb(sym, exp_golomb_order, sbac, bs);

    }

    return COM_OK;
}

void enc_sbac_encode_bin_for_rdoq(u32 bin, SBAC_CTX_MODEL *model)
{
    u16 cmps = (*model) & 1;

    if (bin != cmps)
    {
        *model = tab_cycno_lgpmps_mps[(*model) | (1 << 13)];
    }
    else
    {
        *model = tab_cycno_lgpmps_mps[*model];
    }
}


static void enc_eco_run_for_rdoq(u32 sym, u32 num_ctx, SBAC_CTX_MODEL *model)
{
    u32 ctx_idx = 0;

    do
    {
        enc_sbac_encode_bin_for_rdoq(sym ? 0 : 1, model + min(ctx_idx, num_ctx - 1));
        ctx_idx++;
    }
    while (sym--);
}

static int enc_eco_level(u32 sym, ENC_SBAC *sbac, SBAC_CTX_MODEL *model, COM_BSW *bs)
{
    int exp_golomb_order = 0;

    COM_TRACE_COUNTER;
    COM_TRACE_STR("level:");
    COM_TRACE_INT(sym);
    COM_TRACE_STR("\n");

    if (sym < 8)
    {
        sbac_write_truncate_unary_sym(sym, 2, 9, sbac, model, bs);
    }
    else
    {
        sym -= 8;

        sbac_write_truncate_unary_sym(8, 2, 9, sbac, model, bs);

        // exp_golomb part
        while ((int)sym >= (1 << exp_golomb_order))
        {
            sym = sym - (1 << exp_golomb_order);
            exp_golomb_order++;
        }

        sbac_write_unary_sym_ep(exp_golomb_order, sbac, bs);
        sbac_encode_bins_ep_msb(sym, exp_golomb_order, sbac, bs);

    }

    return COM_OK;
}

static void enc_eco_level_for_rdoq(u32 sym, u32 num_ctx, SBAC_CTX_MODEL *model)
{
    u32 ctx_idx = 0;

    do
    {
        enc_sbac_encode_bin_for_rdoq(sym ? 0 : 1, model + min(ctx_idx, num_ctx - 1));
        ctx_idx++;
    } while (sym--);
}

void enc_eco_run_length_cc(COM_BSW *bs, s16 *coef, int log2_w, int log2_h, int num_sig, int ch_type)
{
    ENC_SBAC    *sbac;
    COM_SBAC_CTX *sbac_ctx;
    u32            num_coeff, scan_pos;
    u32            sign, level, prev_level, run, last_flag;
    s32            t0;
    const u16     *scanp;
    s16            coef_cur;
    sbac = GET_SBAC_ENC(bs);
    sbac_ctx = &sbac->ctx;
    scanp = com_scan_tbl[COEF_SCAN_ZIGZAG][log2_w - 1][log2_h - 1];
    num_coeff = 1 << (log2_w + log2_h);
    run = 0;
    prev_level = 6;
    for (scan_pos = 0; scan_pos < num_coeff; scan_pos++)
    {
        coef_cur = coef[scanp[scan_pos]];
        if (coef_cur)
        {
            level = COM_ABS16(coef_cur);
            sign = (coef_cur > 0) ? 0 : 1;
            t0 = ((COM_MIN(prev_level - 1, 5)) * 2) + (ch_type == Y_C ? 0 : 12);
            /* Run coding */
            enc_eco_run(run, sbac, &sbac_ctx->run[t0], bs);
            enc_eco_run_for_rdoq(run, 2, &sbac_ctx->run_rdoq[t0]);

            /* Level coding */
            enc_eco_level(level - 1, sbac, &sbac_ctx->level[t0], bs);

            /* Sign coding */
            sbac_encode_bin_ep(sign, sbac, bs);
            if (scan_pos == num_coeff - 1)
            {
                break;
            }
            run = 0;
            num_sig--;
            /* Last flag coding */
            last_flag = (num_sig == 0) ? 1 : 0;
            enc_sbac_encode_binW(last_flag, sbac,
                                 &sbac_ctx->last1[COM_MIN(prev_level - 1, 5) + (ch_type == Y_C ? 0 : NUM_SBAC_CTX_LAST1)],
                                 &sbac_ctx->last2[ace_get_log2(scan_pos + 1) + (ch_type == Y_C ? 0 : NUM_SBAC_CTX_LAST2)], bs);
            prev_level = level;

            if (last_flag)
            {
                break;
            }
        }
        else
        {
            run++;
        }
    }
#if ENC_DEC_TRACE
    COM_TRACE_STR("coef");
    COM_TRACE_INT(ch_type);
    for (scan_pos = 0; scan_pos < num_coeff; scan_pos++)
    {
        COM_TRACE_INT(coef[scan_pos]);
    }
    COM_TRACE_STR("\n");
#endif
}

void enc_eco_xcoef(COM_BSW *bs, s16 *coef, int log2_w, int log2_h, int num_sig, int ch_type)
{
    if( (log2_w > MAX_TR_LOG2) || (log2_h > MAX_TR_LOG2) )
    {
        printf( "transform size > 64x64" );
        assert( 0 );
    }

    enc_eco_run_length_cc(bs, coef, log2_w, log2_h, num_sig, (ch_type == Y_C ? 0 : 1));
}

#if CHROMA_NOT_SPLIT
int enc_eco_cbf_uv(COM_BSW * bs, int num_nz[MAX_NUM_TB][N_C])
{
    ENC_SBAC     *sbac     = GET_SBAC_ENC(bs);
    COM_SBAC_CTX *sbac_ctx = &sbac->ctx;

    assert(num_nz[TBUV0][Y_C] == 0);
    enc_sbac_encode_bin(!!num_nz[TBUV0][U_C], sbac, sbac_ctx->cbf + 1, bs);
    enc_sbac_encode_bin(!!num_nz[TBUV0][V_C], sbac, sbac_ctx->cbf + 2, bs);

    COM_TRACE_STR("cbf U ");
    COM_TRACE_INT(!!num_nz[TBUV0][U_C]);
    COM_TRACE_STR("cbf V ");
    COM_TRACE_INT(!!num_nz[TBUV0][V_C]);
    COM_TRACE_STR("\n");
    return COM_OK;
}
#endif

#if IPCM
int enc_eco_cbf(COM_BSW * bs, int tb_avaliable, int pb_part_size, int tb_part_size, int num_nz[MAX_NUM_TB][N_C], u8 pred_mode, s8 ipm[MAX_NUM_PB][2], u8 tree_status, ENC_CTX * ctx)
#else
int enc_eco_cbf(COM_BSW * bs, int tb_avaliable, int pb_part_size, int tb_part_size, int num_nz[MAX_NUM_TB][N_C], u8 pred_mode, u8 tree_status, ENC_CTX * ctx)
#endif
{
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    ENC_SBAC    *sbac;
    COM_SBAC_CTX *sbac_ctx;
    sbac     = GET_SBAC_ENC(bs);
    sbac_ctx = &sbac->ctx;
    int ctp_zero_flag = !is_cu_nz(num_nz);

    /* code allcbf */
    if(pred_mode != MODE_INTRA)
    {
        if (pred_mode == MODE_DIR)
        {
            assert(ctp_zero_flag == 0);
        }
        else
        {
#if CHROMA_NOT_SPLIT
            if (tree_status == TREE_LC)
            {
#endif
#if SEP_CONTEXT
                if (mod_info_curr->cu_width_log2 > 6 || mod_info_curr->cu_height_log2 > 6)
                {
                    enc_sbac_encode_bin(ctp_zero_flag, sbac, sbac_ctx->ctp_zero_flag + 1, bs);
                    assert(ctp_zero_flag == 1);
                }
                else
#endif
                    enc_sbac_encode_bin(ctp_zero_flag, sbac, sbac_ctx->ctp_zero_flag, bs);
                COM_TRACE_COUNTER;
                COM_TRACE_STR("ctp zero flag ");
                COM_TRACE_INT(ctp_zero_flag);
                COM_TRACE_STR("\n");

                if (ctp_zero_flag)
                {
                    for (int i = 0; i < MAX_NUM_TB; i++)
                    {
                        assert(num_nz[i][Y_C] == 0 && num_nz[i][U_C] == 0 && num_nz[i][V_C] == 0);
                    }
                    assert(tb_part_size == SIZE_2Nx2N);
                    return COM_OK;
                }
#if CHROMA_NOT_SPLIT
            }
#endif
        }

        if (tb_avaliable)
        {
            enc_sbac_encode_bin(tb_part_size != SIZE_2Nx2N, sbac, sbac_ctx->tb_split, bs);
        }
        else
        {
            assert(tb_part_size == SIZE_2Nx2N);
        }
        COM_TRACE_COUNTER;
        COM_TRACE_STR("tb_split ");
        COM_TRACE_INT(tb_part_size != SIZE_2Nx2N);
        COM_TRACE_STR("\n");

        if (tree_status == TREE_LC)
        {
            enc_sbac_encode_bin(!!num_nz[TBUV0][U_C], sbac, sbac_ctx->cbf + 1, bs);
            enc_sbac_encode_bin(!!num_nz[TBUV0][V_C], sbac, sbac_ctx->cbf + 2, bs);
            COM_TRACE_STR("cbf U ");
            COM_TRACE_INT(!!num_nz[TBUV0][U_C]);
            COM_TRACE_STR("cbf V ");
            COM_TRACE_INT(!!num_nz[TBUV0][V_C]);
            COM_TRACE_STR("\n");
        }
        else
        {
            assert(tree_status == TREE_L);
            COM_TRACE_STR("[cbf uv at tree L]\n");
        }

        COM_TRACE_STR("cbf Y ");
#if CHROMA_NOT_SPLIT
        if (num_nz[TBUV0][U_C] + num_nz[TBUV0][V_C] == 0 && tb_part_size == SIZE_2Nx2N && tree_status == TREE_LC)
#else
        if (num_nz[TBUV0][U_C] + num_nz[TBUV0][V_C] == 0 && tb_part_size == SIZE_2Nx2N)
#endif
        {
            assert(num_nz[TB0][Y_C] > 0);
            COM_TRACE_INT(1);
        }
        else
        {
            int i, part_num = get_part_num(tb_part_size);
            for (i = 0; i < part_num; i++)
            {
                enc_sbac_encode_bin(!!num_nz[i][Y_C], sbac, sbac_ctx->cbf, bs);
                COM_TRACE_INT(!!num_nz[i][Y_C]);
            }
        }
        COM_TRACE_STR("\n");
    }
    else
    {
#if IPCM
        if (!(ipm[PB0][0] == IPD_IPCM))
        {
#endif
            int i, part_num = get_part_num(tb_part_size);
            assert(tb_part_size == get_tb_part_size_by_pb(pb_part_size, pred_mode));

            COM_TRACE_STR("cbf Y ");
            for (i = 0; i < part_num; i++)
            {
                enc_sbac_encode_bin(!!num_nz[i][Y_C], sbac, sbac_ctx->cbf, bs);
                COM_TRACE_INT(!!num_nz[i][Y_C]);
            }
            COM_TRACE_STR("\n");
#if IPCM
        }
#endif
        if (tree_status == TREE_LC)
        {
#if IPCM
            if (!(ipm[PB0][0] == IPD_IPCM && ipm[PB0][1] == IPD_DM_C))
            {
#endif
                enc_sbac_encode_bin(!!num_nz[TBUV0][U_C], sbac, sbac_ctx->cbf + 1, bs);
                enc_sbac_encode_bin(!!num_nz[TBUV0][V_C], sbac, sbac_ctx->cbf + 2, bs);
                COM_TRACE_STR("cbf U ");
                COM_TRACE_INT(!!num_nz[TBUV0][U_C]);
                COM_TRACE_STR("cbf V ");
                COM_TRACE_INT(!!num_nz[TBUV0][V_C]);
                COM_TRACE_STR("\n");
#if IPCM
            }
#endif
        }
        else
        {
            assert(tree_status == TREE_L);
            COM_TRACE_STR("[cbf uv at tree L]\n");
        }
    }

    return COM_OK;
}

int encode_coef(COM_BSW * bs, s16 coef[N_C][MAX_CU_DIM], int cu_width_log2, int cu_height_log2, u8 pred_mode, COM_MODE *mi, u8 tree_status, ENC_CTX * ctx)
{
#if ENC_DEC_TRACE
    int num_coeff = 1 << (cu_height_log2 + cu_width_log2);
#endif
    int tb_part_size = mi->tb_part;
    int pb_part_size = mi->pb_part;
    int i, j;

    int tb_avaliable = is_tb_avaliable(ctx->info, cu_width_log2, cu_height_log2, pb_part_size, pred_mode);
    int start_comp, num_comp;
    start_comp = (tree_status == TREE_L || tree_status == TREE_LC) ? Y_C : U_C;
    num_comp = (tree_status == TREE_LC) ? 3 : (tree_status == TREE_L ? 1 : 2);


    if (tree_status != TREE_C)
#if IPCM
        enc_eco_cbf(bs, tb_avaliable, pb_part_size, tb_part_size, mi->num_nz, pred_mode, mi->ipm, tree_status, ctx);
#else
        enc_eco_cbf(bs, tb_avaliable, pb_part_size, tb_part_size, mi->num_nz, pred_mode, tree_status, ctx);
#endif
    else
#if IPCM
        if (!(pred_mode == MODE_INTRA && mi->ipm[0][0] == IPD_IPCM && mi->ipm[0][1] == IPD_DM_C))
        {
#endif
            enc_eco_cbf_uv(bs, mi->num_nz);
#if IPCM
        }
#endif

    for (i = start_comp; i < start_comp + num_comp; i++)
    {
        int log2_tb_w, log2_tb_h, tb_size, part_num;
        int plane_width_log2  = cu_width_log2  - (i != Y_C);
        int plane_height_log2 = cu_height_log2 - (i != Y_C);

#if IPCM
        if (pred_mode == MODE_INTRA && ((i == Y_C && mi->ipm[0][0] == IPD_IPCM) || (i > Y_C && mi->ipm[0][0] == IPD_IPCM && mi->ipm[0][1] == IPD_DM_C)))
        {
            if (i == start_comp)
            {
                enc_sbac_encode_bin_trm(1, GET_SBAC_ENC(bs), bs);
                enc_sbac_finish(bs, 1);
            }
            int tb_w = plane_width_log2 > 5 ? 32 : (1 << plane_width_log2);
            int tb_h = plane_height_log2 > 5 ? 32 : (1 << plane_height_log2);
            int num_tb_w = plane_width_log2 > 5 ? 1 << (plane_width_log2 - 5) : 1;
            int num_tb_h = plane_height_log2 > 5 ? 1 << (plane_height_log2 - 5) : 1;
            for (int h = 0; h < num_tb_h; h++)
            {
                for (int w = 0; w < num_tb_w; w++)
                {
                    s16* coef_tb = coef[i] + (1 << plane_width_log2) * h * tb_h + w * tb_w;
                    encode_ipcm(GET_SBAC_ENC(bs), bs, coef_tb, tb_w, tb_h, 1 << plane_width_log2, ctx->info.bit_depth_input, i);
                }
            }
            if ((i == Y_C && (tree_status == TREE_L || (tree_status == TREE_LC && mi->ipm[0][1] != IPD_DM_C))) || i == V_C)
            {
                assert(COM_BSR_IS_BYTE_ALIGN(bs));
                enc_sbac_init(bs);
            }
        }
        else
        {
#endif
            part_num = get_part_num(i == 0 ? tb_part_size : SIZE_2Nx2N);
            get_tb_width_height_log2(plane_width_log2, plane_height_log2, i == 0 ? tb_part_size : SIZE_2Nx2N, &log2_tb_w, &log2_tb_h);
            tb_size = 1 << (log2_tb_w + log2_tb_h);

            for (j = 0; j < part_num; j++)
            {
                if (mi->num_nz[j][i])
                {
                    enc_eco_xcoef(bs, coef[i] + j * tb_size, log2_tb_w, log2_tb_h, mi->num_nz[j][i], i);
                }
            }
#if IPCM
        }
#endif
    }

    return COM_OK;
}

int encode_pred_mode(COM_BSW * bs, u8 pred_mode, ENC_CTX * ctx)
{
    ENC_SBAC * sbac = GET_SBAC_ENC(bs);
#if NUM_PRED_MODE_CTX > 1
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    u32 * map_scu = ctx->map.map_scu;
    u8  avail[2] = { 0, 0 };
    int scun[2];
    int ctx_inc = 0;
    scun[0] = mod_info_curr->scup - ctx->info.pic_width_in_scu;
    scun[1] = mod_info_curr->scup - 1;

    if (mod_info_curr->y_scu > 0)
        avail[0] = MCU_GET_CODED_FLAG(map_scu[scun[0]]); // up
    if (mod_info_curr->x_scu > 0)
        avail[1] = MCU_GET_CODED_FLAG(map_scu[scun[1]]); // left
    if (avail[0])
        ctx_inc += MCU_GET_INTRA_FLAG(map_scu[scun[0]]);
    if (avail[1])
        ctx_inc += MCU_GET_INTRA_FLAG(map_scu[scun[1]]);
    if (ctx_inc == 0)
    {
        int sample = (1 << mod_info_curr->cu_width_log2) * (1 << mod_info_curr->cu_height_log2);
        ctx_inc = (sample > 256) ? 0 : (sample > 64 ? 3 : 4);
    }

#if SEP_CONTEXT
    if (mod_info_curr->cu_width_log2 > 6 || mod_info_curr->cu_height_log2 > 6)
    {
#if IPCM
        assert(pred_mode == MODE_INTER || pred_mode == MODE_INTRA);
#else
        assert(pred_mode == MODE_INTER);
#endif
        ctx_inc = 5;
    }
#endif

    enc_sbac_encode_bin(pred_mode == MODE_INTRA, sbac, sbac->ctx.pred_mode + ctx_inc, bs);
#else
    enc_sbac_encode_bin(pred_mode == MODE_INTRA, sbac, sbac->ctx.pred_mode, bs);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("pred mode ");
    COM_TRACE_INT(!!pred_mode);
#if NUM_PRED_MODE_CTX > 1
    COM_TRACE_STR(" pred mode ctx ");
    COM_TRACE_INT(ctx_inc);
#endif
    COM_TRACE_STR("\n");
    return COM_OK;
}


int encode_intra_dir(COM_BSW *bs, u8 ipm,
                     u8 mpm[2]
                    )
{
    ENC_SBAC *sbac;
    int ipm_code = (ipm == mpm[0]) ? -2 : ((mpm[1] == ipm) ? -1 : ((ipm < mpm[0]) ? ipm : ((ipm < mpm[1]) ? (ipm - 1) : (ipm - 2))));
    sbac = GET_SBAC_ENC(bs);
    if (ipm_code < 0)
    {
        enc_sbac_encode_bin(1, sbac, sbac->ctx.intra_dir, bs);
        enc_sbac_encode_bin(ipm_code + 2, sbac, sbac->ctx.intra_dir + 6, bs);
    }
    else
    {
        enc_sbac_encode_bin(0, sbac, sbac->ctx.intra_dir, bs);
        enc_sbac_encode_bin((ipm_code & 0x10) >> 4, sbac, sbac->ctx.intra_dir + 1, bs);
        enc_sbac_encode_bin((ipm_code & 0x08) >> 3, sbac, sbac->ctx.intra_dir + 2, bs);
        enc_sbac_encode_bin((ipm_code & 0x04) >> 2, sbac, sbac->ctx.intra_dir + 3, bs);
        enc_sbac_encode_bin((ipm_code & 0x02) >> 1, sbac, sbac->ctx.intra_dir + 4, bs);
        enc_sbac_encode_bin((ipm_code & 0x01), sbac, sbac->ctx.intra_dir + 5, bs);
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ipm Y ");
    COM_TRACE_INT(ipm);
    COM_TRACE_STR(" mpm_0 ");
    COM_TRACE_INT(mpm[0]);
    COM_TRACE_STR(" mpm_1 ");
    COM_TRACE_INT(mpm[1]);
    COM_TRACE_STR("\n");
    return COM_OK;
}

#if TSCPM
int encode_intra_dir_c(COM_BSW *bs, u8 ipm, u8 ipm_l, u8 tscpm_enable_flag)
#else
int encode_intra_dir_c(COM_BSW *bs, u8 ipm, u8 ipm_l)
#endif
{
    ENC_SBAC *sbac;
    u8 chk_bypass;
    sbac = GET_SBAC_ENC(bs);
    COM_IPRED_CONV_L2C_CHK(ipm_l, chk_bypass);

    enc_sbac_encode_bin(!ipm, sbac, sbac->ctx.intra_dir + 7, bs);

    if (ipm)
    {
#if TSCPM
        if (tscpm_enable_flag)
        {
            if (ipm == IPD_TSCPM_C)
            {
                enc_sbac_encode_bin(1, sbac, sbac->ctx.intra_dir + 9, bs);
                COM_TRACE_COUNTER;
                COM_TRACE_STR("ipm UV ");
                COM_TRACE_INT(ipm);
                COM_TRACE_STR("\n");
                return COM_OK;
            }
            else
            {
                enc_sbac_encode_bin(0, sbac, sbac->ctx.intra_dir + 9, bs);
            }
        }
#endif
        u8 symbol = (chk_bypass && ipm > ipm_l) ? ipm - 2 : ipm - 1;

        sbac_write_truncate_unary_sym(symbol, 1, IPD_CHROMA_CNT - 1, sbac, sbac->ctx.intra_dir + 8, bs);

    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ipm UV ");
    COM_TRACE_INT(ipm);
    COM_TRACE_STR("\n");
    return COM_OK;
}

#if IPCM
void encode_ipcm(ENC_SBAC *sbac, COM_BSW *bs, s16 pcm[MAX_CU_DIM], int tb_width, int tb_height, int cu_width, int bit_depth, int ch_type)
{
    int i, j;
#if ENC_DEC_TRACE
    COM_TRACE_STR("pcm_");
    COM_TRACE_INT(ch_type);
    COM_TRACE_STR(":\n");
#endif
    for (i = 0; i < tb_height; i++)
    {
        for (j = 0; j < tb_width; j++)
        {
            if (sbac->is_bitcount)
            {
                com_bsw_write_est(sbac, bit_depth);
            }
            else
            {
                com_bsw_write(bs, pcm[i * cu_width + j], bit_depth);
            }
#if ENC_DEC_TRACE
            COM_TRACE_INT(pcm[i * cu_width + j]);
#endif
        }
#if ENC_DEC_TRACE
        COM_TRACE_STR("\n");
#endif
    }
}
#endif

void encode_inter_dir(COM_BSW *bs, s8 refi[REFP_NUM], int part_size, ENC_CTX * ctx)
{
    assert(ctx->info.pic_header.slice_type == SLICE_B);

    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    u8 predDir;
    if (REFI_IS_VALID(refi[REFP_0]) && REFI_IS_VALID(refi[REFP_1])) /* PRED_BI */
    {
        enc_sbac_encode_bin(1, sbac, sbac->ctx.inter_dir, bs);
        predDir = PRED_BI;
    }
    else
    {
#if SEP_CONTEXT
        COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
        if (mod_info_curr->cu_width_log2 + mod_info_curr->cu_height_log2 < 6)
            enc_sbac_encode_bin(0, sbac, sbac->ctx.inter_dir + 2, bs);
        else
#endif
            enc_sbac_encode_bin(0, sbac, sbac->ctx.inter_dir, bs);
        if (REFI_IS_VALID(refi[REFP_0])) /* PRED_L0 */
        {
            enc_sbac_encode_bin(0, sbac, sbac->ctx.inter_dir + 1, bs);
            predDir = PRED_L0;
        }
        else /* PRED_L1 */
        {
            enc_sbac_encode_bin(1, sbac, sbac->ctx.inter_dir + 1, bs);
            predDir = PRED_L1;
        }
    }

    assert(predDir >= PRED_L0);
    assert(predDir <= PRED_BI);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("pred dir ");
    COM_TRACE_INT(predDir);
    COM_TRACE_STR("\n");
    return;
}

int encode_refidx(COM_BSW *bs, int num_refp, int refi)
{
    if(num_refp > 1)
    {
        ENC_SBAC *sbac = GET_SBAC_ENC(bs);
        sbac_write_truncate_unary_sym(refi, 3, num_refp, sbac, sbac->ctx.refi, bs);
    }

    COM_TRACE_COUNTER;
    COM_TRACE_STR("refi ");
    COM_TRACE_INT(refi);
    COM_TRACE_STR(" ctx ");
    COM_TRACE_INT( 0 );
    COM_TRACE_STR("\n");
    return COM_OK;
}

int encode_mvr_idx(COM_BSW * bs, u8 mvr_idx, BOOL is_affine_mode)
{
    ENC_SBAC * sbac = GET_SBAC_ENC(bs);
    COM_SBAC_CTX * sbac_ctx = &sbac->ctx;

#if !BD_AFFINE_AMVR
    if (is_affine_mode)
    {
        assert(mvr_idx == 0);
        return COM_OK;
    }
#endif

#if BD_AFFINE_AMVR
    if (is_affine_mode)
    {
        sbac_write_truncate_unary_sym(mvr_idx, NUM_AFFINE_MVR_IDX_CTX, MAX_NUM_AFFINE_MVR, sbac, sbac_ctx->affine_mvr_idx, bs);
    }
    else
    {
        sbac_write_truncate_unary_sym(mvr_idx, NUM_MVR_IDX_CTX, MAX_NUM_MVR, sbac, sbac_ctx->mvr_idx, bs);
    }
#else
    sbac_write_truncate_unary_sym(mvr_idx, NUM_MVR_IDX_CTX, MAX_NUM_MVR, sbac, sbac_ctx->mvr_idx, bs);
#endif
    COM_TRACE_COUNTER;
    COM_TRACE_STR("mvr idx ");
    COM_TRACE_INT(mvr_idx);
    COM_TRACE_STR("\n");
    return COM_OK;
}

#if EXT_AMVR_HMVP
void encode_extend_amvr_flag(COM_BSW *bs, u8 mvp_from_hmvp_flag)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    enc_sbac_encode_bin(mvp_from_hmvp_flag, sbac, sbac->ctx.mvp_from_hmvp_flag, bs);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("extended amvr flag ");
    COM_TRACE_INT(mvp_from_hmvp_flag);
    COM_TRACE_STR("\n");
}
#endif

int encode_ipf_flag(COM_BSW * bs, u8 ipf_flag)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    enc_sbac_encode_bin(ipf_flag, sbac, sbac->ctx.ipf_flag, bs);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ipf flag ");
    COM_TRACE_INT(ipf_flag);
    COM_TRACE_STR("\n");
    return COM_OK;
}

static int enc_eco_abs_mvd(u32 sym, ENC_SBAC *sbac, SBAC_CTX_MODEL *model, COM_BSW *bs)
{
    int exp_golomb_order = 0;

    if (sym < 3)   // 0, 1, 2
    {
        if (sym == 0)
        {
            enc_sbac_encode_bin(0, sbac,model, bs);
        }
        else if (sym == 1)
        {
            enc_sbac_encode_bin(1, sbac, model, bs);
            enc_sbac_encode_bin(0, sbac, model + 1, bs);
        }
        else if (sym == 2)
        {
            enc_sbac_encode_bin(1, sbac, model, bs);
            enc_sbac_encode_bin(1, sbac, model + 1, bs);
            enc_sbac_encode_bin(0, sbac, model + 2, bs);
        }
    }
    else
    {
        int offset;

        sym -= 3;
        offset = sym & 1;
        enc_sbac_encode_bin(1, sbac, model, bs);
        enc_sbac_encode_bin(1, sbac, model + 1, bs);
        enc_sbac_encode_bin(1, sbac, model + 2, bs);

        sbac_encode_bin_ep(offset, sbac, bs);
        sym = (sym - offset) >> 1;

        // exp_golomb part
        while ((int)sym >= (1 << exp_golomb_order))
        {
            sym = sym - (1 << exp_golomb_order);
            exp_golomb_order++;
        }

        sbac_write_unary_sym_ep(exp_golomb_order, sbac, bs);
        sbac_encode_bins_ep_msb(sym, exp_golomb_order, sbac, bs);

    }

    return COM_OK;
}

int encode_mvd(COM_BSW *bs, s16 mvd[MV_D])
{
    ENC_SBAC    *sbac;
    COM_SBAC_CTX *sbac_ctx;
    int            t0;
    u32            mv;
    sbac     = GET_SBAC_ENC(bs);
    sbac_ctx = &sbac->ctx;
    t0 = 0;
    mv = mvd[MV_X];
    if(mvd[MV_X] < 0)
    {
        t0 = 1;
        mv = -mvd[MV_X];
    }
    enc_eco_abs_mvd(mv, sbac, sbac_ctx->mvd[0], bs);
    if(mv)
    {
        sbac_encode_bin_ep(t0, sbac, bs);
    }
    t0 = 0;
    mv = mvd[MV_Y];
    if(mvd[MV_Y] < 0)
    {
        t0 = 1;
        mv = -mvd[MV_Y];
    }
    enc_eco_abs_mvd(mv, sbac, sbac_ctx->mvd[1], bs);
    if(mv)
    {
        sbac_encode_bin_ep(t0, sbac, bs);
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("mvd x ");
    COM_TRACE_INT(mvd[MV_X]);
    COM_TRACE_STR("mvd y ");
    COM_TRACE_INT(mvd[MV_Y]);
    COM_TRACE_STR("\n");
    return COM_OK;
}

static int cu_init(ENC_CTX *ctx, ENC_CORE *core, int x, int y, int cup, int cu_width, int cu_height)
{
    ENC_CU_DATA *cu_data = &ctx->map_cu_data[core->lcu_num];
    COM_MODE *mod_info_curr = &ctx->core->mod_info_curr;
    mod_info_curr->cu_width       = cu_width;
    mod_info_curr->cu_height       = cu_height;
    mod_info_curr->cu_width_log2  = CONV_LOG2(cu_width);
    mod_info_curr->cu_height_log2  = CONV_LOG2(cu_height);
    mod_info_curr->x_scu     = PEL2SCU(x);
    mod_info_curr->y_scu     = PEL2SCU(y);
    mod_info_curr->scup      = (mod_info_curr->y_scu * ctx->info.pic_width_in_scu) + mod_info_curr->x_scu;
    core->skip_flag = 0;
    core->mod_info_curr.affine_flag = cu_data->affine_flag[cup];
#if SMVD
    core->mod_info_curr.smvd_flag = cu_data->smvd_flag[cup];
#endif
#if TB_SPLIT_EXT
    core->mod_info_curr.pb_part = cu_data->pb_part[cup];
    core->mod_info_curr.tb_part = cu_data->tb_part[cup];
#endif

    cu_nz_cln(core->mod_info_curr.num_nz);

#if CHROMA_NOT_SPLIT //wrong for TREE_C
    if (ctx->tree_status != TREE_C)
    {
#endif
        if (cu_data->pred_mode[cup] == MODE_SKIP)
        {
            core->skip_flag = 1;
        }
#if CHROMA_NOT_SPLIT
    }
#endif
    return COM_OK;
}

static void coef_rect_to_series(ENC_CTX * ctx,
                                s16 *coef_src[N_C],
                                int x, int y, int cu_width, int cu_height, s16 coef_dst[N_C][MAX_CU_DIM]

                               )
{
    int i, j, sidx, didx;
    sidx = (x&(ctx->info.max_cuwh-1)) + ((y&(ctx->info.max_cuwh-1)) << ctx->info.log2_max_cuwh);
    didx = 0;
    for(j = 0; j < cu_height; j++)
    {
        for(i = 0; i < cu_width; i++)
        {
            coef_dst[Y_C][didx++] = coef_src[Y_C][sidx + i];
        }
        sidx += ctx->info.max_cuwh;
    }
    x >>= 1;
    y >>= 1;
    cu_width >>= 1;
    cu_height >>= 1;
    sidx = (x&((ctx->info.max_cuwh>>1)-1)) + ((y&((ctx->info.max_cuwh>>1)-1)) << (ctx->info.log2_max_cuwh-1));
    didx = 0;
    for(j = 0; j < cu_height; j++)
    {
        for(i = 0; i < cu_width; i++)
        {
            coef_dst[U_C][didx] = coef_src[U_C][sidx + i];
            coef_dst[V_C][didx] = coef_src[V_C][sidx + i];
            didx++;
        }
        sidx += (ctx->info.max_cuwh >> 1);
    }
}

int enc_eco_split_mode(COM_BSW *bs, ENC_CTX *c, ENC_CORE *core, int cud, int cup, int cu_width, int cu_height, int lcu_s
                       , const int parent_split, int qt_depth, int bet_depth, int x, int y)
{
    ENC_SBAC *sbac;
    int ret = COM_OK;
    s8 split_mode;
    int ctx = 0;
    int split_allow[SPLIT_CHECK_NUM];
    int i, non_QT_split_mode_num;
    int boundary = 0, boundary_b = 0, boundary_r = 0;

    if (cu_width == MIN_CU_SIZE && cu_height == MIN_CU_SIZE)
    {
        return ret;
    }

    sbac = GET_SBAC_ENC(bs);
    if(sbac->is_bitcount)
    {
        com_get_split_mode(&split_mode, cud, cup, cu_width, cu_height, lcu_s, core->cu_data_temp[CONV_LOG2(cu_width) - 2][CONV_LOG2(cu_height) - 2].split_mode);
    }
    else
    {
        com_get_split_mode(&split_mode, cud, cup, cu_width, cu_height, lcu_s, c->map_cu_data[core->lcu_num].split_mode);
    }

    boundary = !(x + cu_width <= c->info.pic_width && y + cu_height <= c->info.pic_height);
    boundary_b = boundary && (y + cu_height > c->info.pic_height) && !(x + cu_width > c->info.pic_width);
    boundary_r = boundary && (x + cu_width > c->info.pic_width) && !(y + cu_height > c->info.pic_height);

    com_check_split_mode(&c->info.sqh, split_allow, CONV_LOG2(cu_width), CONV_LOG2(cu_height), boundary, boundary_b, boundary_r, c->info.log2_max_cuwh, c->temporal_id
                         , parent_split, qt_depth, bet_depth, c->info.pic_header.slice_type);
    non_QT_split_mode_num = 0;
    for(i = 1; i < SPLIT_QUAD; i++)
    {
        non_QT_split_mode_num += split_allow[i];
    }

    if (split_allow[SPLIT_QUAD] && !(non_QT_split_mode_num || split_allow[NO_SPLIT])) //only QT is allowed
    {
        assert(split_mode == SPLIT_QUAD);
        return ret;
    }
    else if (split_allow[SPLIT_QUAD])
    {
        enc_eco_split_flag(c, cu_width, cu_height, x, y, bs, sbac, split_mode == SPLIT_QUAD);
        if (split_mode == SPLIT_QUAD)
        {
            return ret;
        }
    }

    if (non_QT_split_mode_num)
    {
        int cu_width_log2 = CONV_LOG2(cu_width);
        int cu_height_log2 = CONV_LOG2(cu_height);
        //split flag
        int x_scu = x >> MIN_CU_LOG2;
        int y_scu = y >> MIN_CU_LOG2;
        int pic_width_in_scu = c->info.pic_width >> MIN_CU_LOG2;
        u8  avail[2] = {0, 0};
        int scun[2];
        int scup = x_scu + y_scu * pic_width_in_scu;

        scun[0] = scup - pic_width_in_scu;
        scun[1] = scup - 1;
        if (y_scu > 0)
            avail[0] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[0]]);  //up
        if (x_scu > 0)
            avail[1] = MCU_GET_CODED_FLAG(c->map.map_scu[scun[1]]); //left

        if (avail[0])
            ctx += (1 << MCU_GET_LOGW(c->map.map_cu_mode[scun[0]])) < cu_width;
        if (avail[1])
            ctx += (1 << MCU_GET_LOGH(c->map.map_cu_mode[scun[1]])) < cu_height;

#if NUM_SBAC_CTX_BT_SPLIT_FLAG == 9
        int sample = cu_width * cu_height;
        int ctx_set = (sample > 1024) ? 0 : (sample > 256 ? 1 : 2);
        int ctx_save = ctx;
        ctx += ctx_set * 3;
#endif

        if (split_allow[NO_SPLIT])
            enc_sbac_encode_bin(split_mode != NO_SPLIT, sbac, sbac->ctx.bt_split_flag + ctx, bs);
        else
            assert(split_mode != NO_SPLIT);

#if NUM_SBAC_CTX_BT_SPLIT_FLAG == 9
        ctx = ctx_save;
#endif

        if(split_mode != NO_SPLIT)
        {
            int HBT = split_allow[SPLIT_BI_HOR];
            int VBT = split_allow[SPLIT_BI_VER];
            int EnableBT = HBT || VBT;
#if EQT
            int HEQT = split_allow[SPLIT_EQT_HOR];
            int VEQT = split_allow[SPLIT_EQT_VER];
            int EnableEQT = HEQT || VEQT;
#endif
            u8 ctx_dir = cu_width_log2 == cu_height_log2 ? 0 : (cu_width_log2 > cu_height_log2 ? 1 : 2);

#if EQT
            u8 split_dir = (split_mode == SPLIT_BI_VER) || (split_mode == SPLIT_EQT_VER);
            u8 split_typ = (split_mode == SPLIT_EQT_HOR) || (split_mode == SPLIT_EQT_VER);
#else
            u8 split_dir = (split_mode == SPLIT_BI_VER) ;
            u8 split_typ = 0;
#endif

#if EQT
            if (EnableEQT && EnableBT)
            {
                enc_sbac_encode_bin(split_typ, sbac, sbac->ctx.split_mode + ctx, bs);
            }
#endif
            if (split_typ == 0)
            {
                if (HBT && VBT)
                {
#if SEP_CONTEXT
                    if (cu_width == 64 && cu_height == 128)
                        ctx_dir = 3;
                    if (cu_width == 128 && cu_height == 64)
                        ctx_dir = 4;
#endif
                    enc_sbac_encode_bin(split_dir, sbac, sbac->ctx.split_dir + ctx_dir, bs);
                    if (cu_width == 64 && cu_height == 128)
                        assert(split_dir == 0);
                    if (cu_width == 128 && cu_height == 64)
                        assert(split_dir == 1);
                }
            }
#if EQT
            if (split_typ == 1)
            {
                if (HEQT && VEQT)
                {
                    enc_sbac_encode_bin(split_dir, sbac, sbac->ctx.split_dir + ctx_dir, bs);
                }
            }
#endif
        }
    }
    COM_TRACE_COUNTER;
    COM_TRACE_STR("split mode ");
    COM_TRACE_INT(split_mode);
    COM_TRACE_STR("\n");
    return ret;
}

#if CHROMA_NOT_SPLIT
int enc_eco_unit_chroma(ENC_CTX * ctx, ENC_CORE * core, int x, int y, int cup, int cu_width, int cu_height)
{
    s16(*coef)[MAX_CU_DIM] = core->ctmp;
    COM_BSW *bs = &ctx->bs;
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    ENC_CU_DATA *cu_data = &ctx->map_cu_data[core->lcu_num];
    COM_MODE *mi = &core->mod_info_curr;

    int i, j;
    cu_init(ctx, core, x, y, cup, cu_width, cu_height);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ptr: ");
    COM_TRACE_INT(ctx->ptr);
    COM_TRACE_STR("x pos ");
    COM_TRACE_INT(core->x_pel + ((cup % (ctx->info.max_cuwh >> MIN_CU_LOG2)) << MIN_CU_LOG2));
    COM_TRACE_STR("y pos ");
    COM_TRACE_INT(core->y_pel + ((cup / (ctx->info.max_cuwh >> MIN_CU_LOG2)) << MIN_CU_LOG2));
    COM_TRACE_STR("width ");
    COM_TRACE_INT(cu_width);
    COM_TRACE_STR("height ");
    COM_TRACE_INT(cu_height);
    COM_TRACE_STR("cons mode ");
    COM_TRACE_INT(ctx->cons_pred_mode);
    COM_TRACE_STR("tree status ");
    COM_TRACE_INT(ctx->tree_status);
    COM_TRACE_STR("\n");

    int scu_stride   = PEL2SCU(ctx->info.max_cuwh);
    int x_scu_in_LCU = PEL2SCU(x % ctx->info.max_cuwh);
    int y_scu_in_LCU = PEL2SCU(y % ctx->info.max_cuwh);
    int cu_w_scu = PEL2SCU(cu_width);
    int cu_h_scu = PEL2SCU(cu_height);
    int luma_cup = (y_scu_in_LCU + (cu_h_scu - 1)) * scu_stride + (x_scu_in_LCU + (cu_w_scu - 1));
    u8 luma_pred_mode = cu_data->pred_mode[luma_cup];
    if (luma_pred_mode != MODE_INTRA)
    {
        mi->cu_mode = MODE_INTER;
        for (int lidx = 0; lidx < REFP_NUM; lidx++)
        {
            mi->refi[lidx] = cu_data->refi[luma_cup][lidx];
            mi->mv[lidx][MV_X] = cu_data->mv[luma_cup][lidx][MV_X];
            mi->mv[lidx][MV_Y] = cu_data->mv[luma_cup][lidx][MV_Y];
            if (!REFI_IS_VALID(mi->refi[lidx]))
            {
                assert(cu_data->mv[luma_cup][lidx][MV_X] == 0);
                assert(cu_data->mv[luma_cup][lidx][MV_Y] == 0);
            }
        }

        COM_TRACE_STR("luma pred mode INTER");
        COM_TRACE_STR("\n");
        COM_TRACE_STR("L0: Ref ");
        COM_TRACE_INT(mi->refi[0]);
        COM_TRACE_STR("MVX ");
        COM_TRACE_INT(mi->mv[0][MV_X]);
        COM_TRACE_STR("MVY ");
        COM_TRACE_INT(mi->mv[0][MV_Y]);
        COM_TRACE_STR("\n");
        COM_TRACE_STR("L1: Ref ");
        COM_TRACE_INT(mi->refi[1]);
        COM_TRACE_STR("MVX ");
        COM_TRACE_INT(mi->mv[1][MV_X]);
        COM_TRACE_STR("MVY ");
        COM_TRACE_INT(mi->mv[1][MV_Y]);
        COM_TRACE_STR("\n");
    }
    else
    {
        COM_TRACE_STR("luma pred mode INTRA");
        COM_TRACE_STR("\n");

        mi->cu_mode = MODE_INTRA;
#if IPCM
        core->mod_info_curr.ipm[PB0][0] = cu_data->ipm[0][luma_cup];
        core->mod_info_curr.ipm[PB0][1] = cu_data->ipm[1][cup];
#endif
#if TSCPM
        encode_intra_dir_c(bs, cu_data->ipm[1][cup], cu_data->ipm[0][luma_cup], ctx->info.sqh.tscpm_enable_flag);
#else
        encode_intra_dir_c(bs, cu_data->ipm[1][cup], cu_data->ipm[0][luma_cup]);
#endif
        assert(cu_data->ipm[1][cup] == cu_data->ipm[1][luma_cup]);
    }

    /* get coefficients and tq */
    coef_rect_to_series(ctx, cu_data->coef, x, y, cu_width, cu_height, coef);
    for (i = U_C; i < N_C; i++)
    {
        int part_num = get_part_num(SIZE_2Nx2N);
        for (j = 0; j < part_num; j++)
        {
            int pos_x, pos_y, tbp;
            get_tb_start_pos(cu_width, cu_height, SIZE_2Nx2N, j, &pos_x, &pos_y);
            pos_x >>= MIN_CU_LOG2;
            pos_y >>= MIN_CU_LOG2;
            tbp = cup + pos_y * (ctx->info.max_cuwh >> MIN_CU_LOG2) + pos_x;
            mi->num_nz[j][i] = cu_data->num_nz_coef[i][tbp];
        }
    }
#if IPCM
    encode_coef(bs, coef, mi->cu_width_log2, mi->cu_height_log2, mi->cu_mode, &core->mod_info_curr, ctx->tree_status, ctx);
#else
    encode_coef(bs, coef, mi->cu_width_log2, mi->cu_height_log2, cu_data->pred_mode[cup], &core->mod_info_curr, ctx->tree_status, ctx);
#endif

#if TRACE_REC
    {
        int s_rec;
        pel* rec;
        COM_PIC* pic = PIC_REC(ctx);

        cu_width = cu_width >> 1;
        cu_height = cu_height >> 1;
        s_rec = pic->stride_chroma;
        rec = pic->u + ((y >> 1) * s_rec) + (x >> 1);
        COM_TRACE_STR("rec u\n");
        for (int j = 0; j < cu_height; j++)
        {
            for (int i = 0; i < cu_width; i++)
            {
                COM_TRACE_INT(rec[i]);
                COM_TRACE_STR(" ");
            }
            COM_TRACE_STR("\n");
            rec += s_rec;
        }

        rec = pic->v + ((y >> 1) * s_rec) + (x >> 1);
        COM_TRACE_STR("rec v\n");
        for (int j = 0; j < cu_height; j++)
        {
            for (int i = 0; i < cu_width; i++)
            {
                COM_TRACE_INT(rec[i]);
                COM_TRACE_STR(" ");
            }
            COM_TRACE_STR("\n");
            rec += s_rec;
        }
    }
#endif
    return COM_OK;
}
#endif

int enc_eco_unit(ENC_CTX * ctx, ENC_CORE * core, int x, int y, int cup, int cu_width, int cu_height)
{
    COM_MODE *mod_info_curr = &core->mod_info_curr;
    s16(*coef)[MAX_CU_DIM] = core->ctmp;
    COM_BSW *bs = &ctx->bs;
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    u32 *map_scu;
    int slice_type, refi0, refi1;
    int i, j, w, h;
    ENC_CU_DATA *cu_data = &ctx->map_cu_data[core->lcu_num];
    u32 *map_cu_mode;
    int cu_cbf_flag;
#if TB_SPLIT_EXT
    u32 *map_pb_tb_part;
#endif
    slice_type = ctx->slice_type;
    cu_init(ctx, core, x, y, cup, cu_width, cu_height);
    COM_TRACE_COUNTER;
    COM_TRACE_STR("ptr: ");
    COM_TRACE_INT(ctx->ptr);
    COM_TRACE_STR("x pos ");
    COM_TRACE_INT(core->x_pel + ((cup % (ctx->info.max_cuwh >> MIN_CU_LOG2)) << MIN_CU_LOG2));
    COM_TRACE_STR("y pos ");
    COM_TRACE_INT(core->y_pel + ((cup / (ctx->info.max_cuwh >> MIN_CU_LOG2)) << MIN_CU_LOG2));
    COM_TRACE_STR("width ");
    COM_TRACE_INT(cu_width);
    COM_TRACE_STR("height ");
    COM_TRACE_INT(cu_height);
    COM_TRACE_STR("cons mode ");
    COM_TRACE_INT(ctx->cons_pred_mode);
    COM_TRACE_STR("tree status ");
    COM_TRACE_INT(ctx->tree_status);
    COM_TRACE_STR("\n");

    if (!core->skip_flag)
    {
        /* get coefficients and tq */
        coef_rect_to_series(ctx, cu_data->coef, x, y, cu_width, cu_height, coef);
        for(i = 0; i < N_C; i++)
        {
            COM_MODE *mi = &core->mod_info_curr;
            int part_num = get_part_num(i == 0 ? mi->tb_part : SIZE_2Nx2N);

            for (j = 0; j < part_num; j++)
            {
                int pos_x, pos_y, tbp;
                get_tb_start_pos(cu_width, cu_height, i == 0 ? mi->tb_part : SIZE_2Nx2N, j, &pos_x, &pos_y);
                pos_x >>= MIN_CU_LOG2;
                pos_y >>= MIN_CU_LOG2;
                tbp = cup + pos_y * (ctx->info.max_cuwh >> MIN_CU_LOG2) + pos_x;
                mi->num_nz[j][i] = cu_data->num_nz_coef[i][tbp];
            }
        }
    }
    /* entropy coding a CU */
    if(slice_type != SLICE_I)
    {
#if MODE_CONS
        if (ctx->cons_pred_mode == ONLY_INTRA)
            assert(core->skip_flag == 0 && cu_data->pred_mode[cup] == MODE_INTRA);
#endif

        if (ctx->cons_pred_mode != ONLY_INTRA)
        {
            encode_skip_flag(bs, sbac, core->skip_flag, ctx);
        }
        if (core->skip_flag)
        {
            if (ctx->info.sqh.umve_enable_flag)
                encode_umve_flag(bs, cu_data->umve_flag[cup]);

            if (cu_data->umve_flag[cup])
            {
                encode_umve_idx(bs, cu_data->umve_idx[cup]);
            }
            else
            {
                encode_affine_flag(bs, core->mod_info_curr.affine_flag != 0, ctx);
                if (core->mod_info_curr.affine_flag)
                {
                    encode_affine_mrg_idx(bs, cu_data->skip_idx[cup], ctx);

                    COM_TRACE_COUNTER;
                    COM_TRACE_STR("merge affine flag after constructed candidate ");
                    COM_TRACE_INT(core->mod_info_curr.affine_flag);
                    COM_TRACE_STR("\n");
                }
                else
                {
                    encode_skip_idx(bs, cu_data->skip_idx[cup], ctx->info.sqh.num_of_hmvp_cand, ctx);
                }
            }
        }
        else
        {
            if (ctx->cons_pred_mode != ONLY_INTRA)
            {
                encode_direct_flag(bs, cu_data->pred_mode[cup] == MODE_DIR, ctx);
            }
            if (cu_data->pred_mode[cup] == MODE_DIR)
            {
                if (ctx->info.sqh.umve_enable_flag)
                    encode_umve_flag(bs, cu_data->umve_flag[cup]);
                if (cu_data->umve_flag[cup])
                {
                    encode_umve_idx(bs, cu_data->umve_idx[cup]);
                }
                else
                {
                    encode_affine_flag(bs, core->mod_info_curr.affine_flag != 0, ctx);
                    if (core->mod_info_curr.affine_flag)
                    {
                        encode_affine_mrg_idx(bs, cu_data->skip_idx[cup], ctx);

                        COM_TRACE_COUNTER;
                        COM_TRACE_STR("merge affine flag after constructed candidate ");
                        COM_TRACE_INT(core->mod_info_curr.affine_flag);
                        COM_TRACE_STR("\n");
                    }
                    else
                    {
                        encode_skip_idx(bs, cu_data->skip_idx[cup], ctx->info.sqh.num_of_hmvp_cand, ctx);
                    }
                }
            }
            else
            {
                if (ctx->cons_pred_mode == NO_MODE_CONS)
                {
                    encode_pred_mode(bs, cu_data->pred_mode[cup], ctx);
                }
                else if (ctx->cons_pred_mode == ONLY_INTER)
                    assert(cu_data->pred_mode[cup] == MODE_INTER);
                else
                    assert(cu_data->pred_mode[cup] == MODE_INTRA);

                if (cu_data->pred_mode[cup] != MODE_INTRA)
                {
                    assert(cu_data->pred_mode[cup] == MODE_INTER);
                    encode_affine_flag(bs, core->mod_info_curr.affine_flag != 0, ctx); /* inter affine_flag */

                    if (ctx->info.sqh.amvr_enable_flag)
                    {
#if EXT_AMVR_HMVP
                        if (ctx->info.sqh.emvr_enable_flag && !core->mod_info_curr.affine_flag) // also imply ctx->info.sqh.num_of_hmvp_cand is not zero
                        {
                            encode_extend_amvr_flag(bs, cu_data->mvp_from_hmvp_flag[cup]);
                        }
#endif
                        encode_mvr_idx(bs, cu_data->mvr_idx[cup], core->mod_info_curr.affine_flag);
                    }

                    if (cu_data->pred_mode[cup] != MODE_DIR)
                    {
                        if (slice_type == SLICE_B)
                        {
                            encode_inter_dir(bs, cu_data->refi[cup], core->mod_info_curr.pb_part, ctx);
                        }

                        if (core->mod_info_curr.affine_flag) // affine inter mode
                        {
                            int vertex;
                            int vertex_num = core->mod_info_curr.affine_flag + 1;
                            int aff_scup[VER_NUM];
                            aff_scup[0] = cup;
                            aff_scup[1] = cup + ((cu_width >> MIN_CU_LOG2) - 1);
                            aff_scup[2] = cup + (((cu_height >> MIN_CU_LOG2) - 1) << ctx->log2_culine);
                            refi0 = cu_data->refi[cup][REFP_0];
                            refi1 = cu_data->refi[cup][REFP_1];
                            if (IS_INTER_SLICE(slice_type) && REFI_IS_VALID(refi0))
                            {
                                encode_refidx(bs, ctx->rpm.num_refp[REFP_0], refi0);
                                for (vertex = 0; vertex < vertex_num; vertex++)
                                {
#if BD_AFFINE_AMVR
                                    s16 mvd_tmp[MV_D];
                                    u8 amvr_shift = Tab_Affine_AMVR(cu_data->mvr_idx[cup]);
                                    mvd_tmp[MV_X] = cu_data->mvd[aff_scup[vertex]][REFP_0][MV_X] >> amvr_shift;
                                    mvd_tmp[MV_Y] = cu_data->mvd[aff_scup[vertex]][REFP_0][MV_Y] >> amvr_shift;
                                    encode_mvd(bs, mvd_tmp);
#else
                                    encode_mvd(bs, cu_data->mvd[aff_scup[vertex]][REFP_0]);
#endif
                                }
                            }
                            if (slice_type == SLICE_B && REFI_IS_VALID(refi1))
                            {
                                encode_refidx(bs, ctx->rpm.num_refp[REFP_1], refi1);
                                for (vertex = 0; vertex < vertex_num; vertex++)
                                {
#if BD_AFFINE_AMVR
                                    s16 mvd_tmp[MV_D];
                                    u8 amvr_Shift = Tab_Affine_AMVR(cu_data->mvr_idx[cup]);
                                    mvd_tmp[MV_X] = cu_data->mvd[aff_scup[vertex]][REFP_1][MV_X] >> amvr_Shift;
                                    mvd_tmp[MV_Y] = cu_data->mvd[aff_scup[vertex]][REFP_1][MV_Y] >> amvr_Shift;
                                    encode_mvd(bs, mvd_tmp);
#else
                                    encode_mvd(bs, cu_data->mvd[aff_scup[vertex]][REFP_1]);
#endif
                                }
                            }
                        }
                        else
                        {
                            refi0 = cu_data->refi[cup][REFP_0];
                            refi1 = cu_data->refi[cup][REFP_1];
#if SMVD
                            if ( ctx->info.sqh.smvd_enable_flag && REFI_IS_VALID( refi0 ) && REFI_IS_VALID( refi1 )
                                    && ( ctx->ptr - ctx->refp[0][REFP_0].ptr == ctx->refp[0][REFP_1].ptr - ctx->ptr )
#if SKIP_SMVD
                                    && !cu_data->mvp_from_hmvp_flag[cup]
#endif
                               )
                            {
                                encode_smvd_flag( bs, cu_data->smvd_flag[cup] );
                            }
#endif

                            if (IS_INTER_SLICE(slice_type) && REFI_IS_VALID(refi0))
                            {
#if SMVD
                                if ( cu_data->smvd_flag[cup] == 0 )
#endif
                                    encode_refidx(bs, ctx->rpm.num_refp[REFP_0], refi0);

                                cu_data->mvd[cup][REFP_0][MV_Y] >>= cu_data->mvr_idx[cup];
                                cu_data->mvd[cup][REFP_0][MV_X] >>= cu_data->mvr_idx[cup];
                                encode_mvd(bs, cu_data->mvd[cup][REFP_0]);
                                cu_data->mvd[cup][REFP_0][MV_Y] <<= cu_data->mvr_idx[cup];
                                cu_data->mvd[cup][REFP_0][MV_X] <<= cu_data->mvr_idx[cup];
                            }
                            if (slice_type == SLICE_B && REFI_IS_VALID(refi1))
                            {
#if SMVD
                                if ( cu_data->smvd_flag[cup] == 0 )
                                {
#endif
                                    encode_refidx(bs, ctx->rpm.num_refp[REFP_1], refi1);

                                    cu_data->mvd[cup][REFP_1][MV_Y] >>= cu_data->mvr_idx[cup];
                                    cu_data->mvd[cup][REFP_1][MV_X] >>= cu_data->mvr_idx[cup];
                                    encode_mvd(bs, cu_data->mvd[cup][REFP_1]);
                                    cu_data->mvd[cup][REFP_1][MV_Y] <<= cu_data->mvr_idx[cup];
                                    cu_data->mvd[cup][REFP_1][MV_X] <<= cu_data->mvr_idx[cup];
#if SMVD
                                }
#endif
                            }
                        }
                    }
                }
            }
        }
    }
    if (cu_data->pred_mode[cup] == MODE_INTRA)
    {
        if (ctx->tree_status != TREE_C)
        {
            com_assert(cu_data->ipm[0][cup] != IPD_INVALID);
        }
        if (ctx->tree_status != TREE_L)
        {
            com_assert(cu_data->ipm[1][cup] != IPD_INVALID);
        }
#if DT_SYNTAX //core
        encode_part_size(ctx, bs, cu_data->pb_part[cup], cu_width, cu_height, cu_data->pred_mode[cup]);
        //here is special, we need to calculate scup inside one CTU
        get_part_info(ctx->info.max_cuwh >> 2, x % ctx->info.max_cuwh, y % ctx->info.max_cuwh, cu_width, cu_height, cu_data->pb_part[cup], &core->mod_info_curr.pb_info);
        assert(core->mod_info_curr.pb_info.sub_scup[0] == cup);
        for (int part_idx = 0; part_idx < core->mod_info_curr.pb_info.num_sub_part; part_idx++)
        {
            int pb_x = core->mod_info_curr.pb_info.sub_x[part_idx];
            int pb_y = core->mod_info_curr.pb_info.sub_y[part_idx];
            int pb_width = core->mod_info_curr.pb_info.sub_w[part_idx];
            int pb_height = core->mod_info_curr.pb_info.sub_h[part_idx];
            int pb_scup = core->mod_info_curr.pb_info.sub_scup[part_idx];
#if IPCM
            core->mod_info_curr.ipm[part_idx][0] = cu_data->ipm[0][pb_scup];
#endif
#endif
            u8 mpm[2];
            mpm[0] = cu_data->mpm[0][pb_scup];
            mpm[1] = cu_data->mpm[1][pb_scup];
            encode_intra_dir(bs, cu_data->ipm[0][pb_scup], mpm);
#if DT_SYNTAX
        }
#endif
        if (ctx->tree_status != TREE_L)
        {
#if IPCM
            core->mod_info_curr.ipm[PB0][1] = cu_data->ipm[1][cup];
#endif
#if TSCPM
            encode_intra_dir_c(bs, cu_data->ipm[1][cup], cu_data->ipm[0][cup], ctx->info.sqh.tscpm_enable_flag);
#else
            encode_intra_dir_c(bs, cu_data->ipm[1][cup], cu_data->ipm[0][cup]);
#endif
        }

#if IPCM
        if (!((ctx->tree_status == TREE_C && cu_data->ipm[0][cup] == IPD_IPCM && cu_data->ipm[1][cup] == IPD_DM_C)
                || (ctx->tree_status != TREE_C && cu_data->ipm[0][cup] == IPD_IPCM)))
        {
#endif

#if DT_INTRA_BOUNDARY_FILTER_OFF
            if( ctx->info.sqh.ipf_enable_flag && (cu_width < MAX_CU_SIZE) && (cu_height < MAX_CU_SIZE) && core->mod_info_curr.pb_part == SIZE_2Nx2N )
#else
            if( ctx->info.sqh.ipf_enable_flag && (cu_width < MAX_CU_SIZE) && (cu_height < MAX_CU_SIZE) )
#endif
                encode_ipf_flag(bs, cu_data->ipf_flag[cup]);
#if IPCM
        }
#endif
    }

    if (!core->skip_flag)
    {
        encode_coef(bs, coef, mod_info_curr->cu_width_log2, mod_info_curr->cu_height_log2, cu_data->pred_mode[cup], &core->mod_info_curr, ctx->tree_status, ctx);
    }
    map_scu = ctx->map.map_scu + mod_info_curr->scup;
    w = (mod_info_curr->cu_width >> MIN_CU_LOG2);
    h = (mod_info_curr->cu_height >> MIN_CU_LOG2);
    map_cu_mode = ctx->map.map_cu_mode + mod_info_curr->scup;
#if TB_SPLIT_EXT
    map_pb_tb_part = ctx->map.map_pb_tb_part + mod_info_curr->scup;
#endif
#if CHROMA_NOT_SPLIT // encoder cu_cbf based on cbf_y in TREE_L
    if (ctx->tree_status == TREE_LC)
    {
        cu_cbf_flag = is_cu_nz(core->mod_info_curr.num_nz);
    }
    else if (ctx->tree_status == TREE_L)
    {
        cu_cbf_flag = is_cu_plane_nz(core->mod_info_curr.num_nz, Y_C);
    }
    else
        assert(0);
#endif

    for(i = 0; i < h; i++)
    {
        for(j = 0; j < w; j++)
        {

            //debug
            //Note: map_scu is initialized, so the following three lines fail
            //assert(MCU_GET_SF(map_scu[j]) == core->skip_flag);
            //assert(MCU_GET_CBFL(map_scu[j]) == core->mod_info_curr.num_nz[tb_idx_y][Y_C]);
            //assert(MCU_GET_AFF(map_scu[j]) == core->mod_info_curr.affine_flag);
            assert(MCU_GET_X_SCU_OFF(map_cu_mode[j]) == j);
            assert(MCU_GET_Y_SCU_OFF(map_cu_mode[j]) == i);
            assert(MCU_GET_LOGW(map_cu_mode[j]) == mod_info_curr->cu_width_log2);
            assert(MCU_GET_LOGH(map_cu_mode[j]) == mod_info_curr->cu_height_log2);
            assert(MCU_GET_TB_PART_LUMA(map_pb_tb_part[j]) == core->mod_info_curr.tb_part);

            if(core->skip_flag)
            {
                MCU_SET_SF(map_scu[j]);
            }
            else
            {
                MCU_CLR_SF(map_scu[j]);
            }
            if(cu_cbf_flag)
            {
                MCU_SET_CBF(map_scu[j]);
            }
            else
            {
                MCU_CLR_CBF(map_scu[j]);
            }
            MCU_SET_CODED_FLAG(map_scu[j]);
            if (cu_data->pred_mode[cup] == MODE_INTRA)
            {
                MCU_SET_INTRA_FLAG(map_scu[j]);
            }
            else
            {
                MCU_CLEAR_INTRA_FLAG(map_scu[j]);
            }

            if(core->mod_info_curr.affine_flag)
            {
                MCU_SET_AFF(map_scu[j], core->mod_info_curr.affine_flag);
            }
            else
            {
                MCU_CLR_AFF(map_scu[j]);
            }

            MCU_SET_X_SCU_OFF(map_cu_mode[j], j);
            MCU_SET_Y_SCU_OFF(map_cu_mode[j], i);
            MCU_SET_LOGW(map_cu_mode[j], mod_info_curr->cu_width_log2);
            MCU_SET_LOGH(map_cu_mode[j], mod_info_curr->cu_height_log2);
        }
        map_scu += ctx->info.pic_width_in_scu;
        map_cu_mode += ctx->info.pic_width_in_scu;
#if TB_SPLIT_EXT
        map_pb_tb_part += ctx->info.pic_width_in_scu;
#endif
    }
#if MVF_TRACE
    // Trace MVF in encoder
    {
        s8(*map_refi)[REFP_NUM];
        s16(*map_mv)[REFP_NUM][MV_D];
        u32  *map_scu;
        map_refi = ctx->map.map_refi + mod_info_curr->scup;
        map_scu = ctx->map.map_scu + mod_info_curr->scup;
        map_mv = ctx->map.map_mv + mod_info_curr->scup;
        map_cu_mode = ctx->map.map_cu_mode + mod_info_curr->scup;
        for(i = 0; i < h; i++)
        {
            for(j = 0; j < w; j++)
            {
                COM_TRACE_COUNTER;
                COM_TRACE_STR(" x: ");
                COM_TRACE_INT(j);
                COM_TRACE_STR(" y: ");
                COM_TRACE_INT(i);
                COM_TRACE_STR(" ref0: ");
                COM_TRACE_INT(map_refi[j][REFP_0]);
                COM_TRACE_STR(" mv: ");
                COM_TRACE_MV(map_mv[j][REFP_0][MV_X], map_mv[j][REFP_0][MV_Y]);
                COM_TRACE_STR(" ref1: ");
                COM_TRACE_INT(map_refi[j][REFP_1]);
                COM_TRACE_STR(" mv: ");
                COM_TRACE_MV(map_mv[j][REFP_1][MV_X], map_mv[j][REFP_1][MV_Y]);

                COM_TRACE_STR(" affine: ");
                COM_TRACE_INT(MCU_GET_AFF(map_scu[j]));
                if(MCU_GET_AFF(map_scu[j]))
                {
                    COM_TRACE_STR(" logw: ");
                    COM_TRACE_INT(MCU_GET_LOGW(map_cu_mode[j]));
                    COM_TRACE_STR(" logh: ");
                    COM_TRACE_INT(MCU_GET_LOGH(map_cu_mode[j]));
                    COM_TRACE_STR(" xoff: ");
                    COM_TRACE_INT(MCU_GET_X_SCU_OFF(map_cu_mode[j]));
                    COM_TRACE_STR(" yoff: ");
                    COM_TRACE_INT(MCU_GET_Y_SCU_OFF(map_cu_mode[j]));
                }

                COM_TRACE_STR("\n");
            }
            map_refi += ctx->info.pic_width_in_scu;
            map_mv += ctx->info.pic_width_in_scu;
            map_scu += ctx->info.pic_width_in_scu;
            map_cu_mode += ctx->info.pic_width_in_scu;
        }
    }
#endif
#if TRACE_REC
    {
        int s_rec;
        pel* rec;
        COM_PIC* pic = PIC_REC(ctx);
        if (ctx->tree_status != TREE_C)
        {
            s_rec = pic->stride_luma;
            rec = pic->y + (y * s_rec) + x;
            COM_TRACE_STR("rec y\n");
            for (int j = 0; j < cu_height; j++)
            {
                for (int i = 0; i < cu_width; i++)
                {
                    COM_TRACE_INT(rec[i]);
                    COM_TRACE_STR(" ");
                }
                COM_TRACE_STR("\n");
                rec += s_rec;
            }
        }

        if (ctx->tree_status != TREE_L)
        {
            cu_width = cu_width >> 1;
            cu_height = cu_height >> 1;
            s_rec = pic->stride_chroma;
            rec = pic->u + ((y >> 1) * s_rec) + (x >> 1);
            COM_TRACE_STR("rec u\n");
            for (int j = 0; j < cu_height; j++)
            {
                for (int i = 0; i < cu_width; i++)
                {
                    COM_TRACE_INT(rec[i]);
                    COM_TRACE_STR(" ");
                }
                COM_TRACE_STR("\n");
                rec += s_rec;
            }

            rec = pic->v + ((y >> 1) * s_rec) + (x >> 1);
            COM_TRACE_STR("rec v\n");
            for (int j = 0; j < cu_height; j++)
            {
                for (int i = 0; i < cu_width; i++)
                {
                    COM_TRACE_INT(rec[i]);
                    COM_TRACE_STR(" ");
                }
                COM_TRACE_STR("\n");
                rec += s_rec;
            }
        }
    }
#endif
    return COM_OK;
}

int enc_eco_DB_param(COM_BSW * bs, COM_PIC_HEADER *sh)
{
    com_bsw_write1(bs, sh->loop_filter_disable_flag);
    if (sh->loop_filter_disable_flag == 0)
    {
        com_bsw_write(bs, sh->loop_filter_parameter_flag, 1);
        if (sh->loop_filter_parameter_flag)
        {
            com_bsw_write_se(bs, sh->alpha_c_offset);
            com_bsw_write_se(bs, sh->beta_offset);
        }
        else
        {
            sh->alpha_c_offset = 0;
            sh->beta_offset = 0;
        }
    }
    return COM_OK;
}

void enc_eco_sao_mrg_flag(ENC_SBAC *sbac, COM_BSW *bs, int mergeleft_avail, int mergeup_avail, SAOBlkParam *saoBlkParam)
{
    int MergeLeft = 0;
    int MergeUp = 0;
    int value1 = 0;
    int value2 = 0;
    if (mergeleft_avail)
    {
        MergeLeft = ((saoBlkParam->modeIdc == SAO_MODE_MERGE) && (saoBlkParam->typeIdc == SAO_MERGE_LEFT));
        value1 = MergeLeft ? 1 : 0;
    }
    if (mergeup_avail && !MergeLeft)
    {
        MergeUp = ((saoBlkParam->modeIdc == SAO_MODE_MERGE) && (saoBlkParam->typeIdc == SAO_MERGE_ABOVE));
        value1 = MergeUp ? (1 + mergeleft_avail) : 0;
    }
    value2 = mergeleft_avail + mergeup_avail;
    if (value2 == 1)
    {
        assert(value1 <= 1);
        enc_sbac_encode_bin(value1, sbac, &sbac->ctx.sao_merge_flag[0], bs);
    }
    else if (value2 == 2)
    {
        assert(value1 <= 2);
        enc_sbac_encode_bin(value1 & 0x01, sbac, &sbac->ctx.sao_merge_flag[1], bs);
        if (value1 != 1)
        {
            enc_sbac_encode_bin((value1 >> 1) & 0x01, sbac, &sbac->ctx.sao_merge_flag[2], bs);
        }
    }
}


void enc_eco_sao_mode(ENC_SBAC *sbac, COM_BSW *bs, SAOBlkParam *saoBlkParam)
{
    int value1 = 0;
    if (saoBlkParam->modeIdc == SAO_MODE_OFF)
    {
        value1 = 0;
    }
    else if (saoBlkParam->typeIdc == SAO_TYPE_BO)
    {
        value1 = 1;
    }
    else
    {
        value1 = 3;
    }
    if (value1 == 0)
    {
        enc_sbac_encode_bin(1, sbac, sbac->ctx.sao_mode, bs);
    }
    else
    {
        enc_sbac_encode_bin(0, sbac, sbac->ctx.sao_mode, bs);
        sbac_encode_bin_ep(!((value1 >> 1) & 0x01), sbac, bs);
    }
}

static void eco_sao_offset_AEC(int value1, int value2, ENC_SBAC *sbac, COM_BSW *bs)
{
    int  act_sym;
    u32 signsymbol = value1 >= 0 ? 0 : 1;
    int temp, maxvalue;
    int offset_type = value2;
    assert(offset_type != SAO_CLASS_EO_PLAIN);
    if (offset_type == SAO_CLASS_EO_FULL_VALLEY)
    {
        assert(value1 < 7);
        act_sym = EO_OFFSET_MAP[value1 + 1];
    }
    else if (offset_type == SAO_CLASS_EO_FULL_PEAK)
    {
        assert(value1 > -7);
        act_sym = EO_OFFSET_MAP[-value1 + 1];
    }
    else
    {
        act_sym = abs(value1);
    }
    maxvalue = saoclip[offset_type][2];
    temp = act_sym;
    if (temp == 0)
    {
        if (offset_type == SAO_CLASS_BO)
        {
            enc_sbac_encode_bin(1, sbac, sbac->ctx.sao_offset, bs);
        }
        else
        {
            sbac_encode_bin_ep(1, sbac, bs);
        }
    }
    else
    {
        while (temp != 0)
        {
            if (offset_type == SAO_CLASS_BO && temp == act_sym)
            {
                enc_sbac_encode_bin(0, sbac, sbac->ctx.sao_offset, bs);
            }
            else
            {
                sbac_encode_bin_ep(0, sbac, bs);
            }
            temp--;
        }
        if (act_sym < maxvalue)
        {
            sbac_encode_bin_ep(1, sbac, bs);
        }
    }
    if (offset_type == SAO_CLASS_BO && act_sym)
    {
        sbac_encode_bin_ep(signsymbol, sbac, bs);
    }
}

void enc_eco_sao_offset(ENC_SBAC *sbac, COM_BSW *bs, SAOBlkParam *saoBlkParam)
{
    int value1 = 0;
    int value2 = 0;
    int i;
    int bandIdxBO[4];
    assert(saoBlkParam->modeIdc == SAO_MODE_NEW);
    if (saoBlkParam->typeIdc == SAO_TYPE_BO)
    {
        bandIdxBO[0] = saoBlkParam->startBand;
        bandIdxBO[1] = bandIdxBO[0] + 1;
        bandIdxBO[2] = saoBlkParam->startBand2;
        bandIdxBO[3] = bandIdxBO[2] + 1;
        for (i = 0; i < 4; i++)
        {
            value1 = saoBlkParam->offset[bandIdxBO[i]];
            value2 = SAO_CLASS_BO;
            assert(abs(value1) >= 0 && abs(value1) <= 7);
            eco_sao_offset_AEC(value1, value2, sbac, bs);
        }
    }
    else
    {
        assert(saoBlkParam->typeIdc >= SAO_TYPE_EO_0 && saoBlkParam->typeIdc <= SAO_TYPE_EO_45);
        for (i = SAO_CLASS_EO_FULL_VALLEY; i < NUM_SAO_EO_CLASSES; i++)
        {
            if (i == SAO_CLASS_EO_PLAIN)
            {
                continue;
            }
            value1 = saoBlkParam->offset[i];
            value2 = i;
            if (i == 0)
                assert(value1 >= -1 && value1 <= 6);
            if (i == 1)
                assert(value1 >= 0 && value1 <= 1);
            if (i == 2)
                assert(value1 >= -1 && value1 <= 0);
            if (i == 3)
                assert(value1 >= -6 && value1 <= 1);
            eco_sao_offset_AEC(value1, value2, sbac, bs);
        }
    }
}

static void eco_sao_type_AEC(int value1, int value2, ENC_SBAC *sbac, COM_BSW *bs)
{
    int  act_sym = value1;
    int temp;
    int i, length;
    int exp_golomb_order;
    temp = act_sym;
    exp_golomb_order = 1;
    switch (value2)
    {
    case 0:
        length = NUM_SAO_EO_TYPES_LOG2;
        break;
    case 1:
        length = NUM_SAO_BO_CLASSES_LOG2;
        break;
    case 2:
        length = NUM_SAO_BO_CLASSES_LOG2 - 1;
        break;
    default:
        length = 0;
        break;
    }
    if (value2 == 2)
    {
        while (1)
        {
            if ((unsigned int)temp >= (unsigned int)(1 << exp_golomb_order))
            {
                sbac_encode_bin_ep(0, sbac, bs);
                temp = temp - (1 << exp_golomb_order);
                exp_golomb_order++;
            }
            else
            {
                if (exp_golomb_order == 4)
                {
                    exp_golomb_order = 0;
                }
                else
                {
                    sbac_encode_bin_ep(1, sbac, bs);
                }
                while (exp_golomb_order--)     //next binary part
                {
                    sbac_encode_bin_ep((unsigned char)((temp >> exp_golomb_order) & 1), sbac, bs);
                }
                break;
            }
        }
    }
    else
    {
        for (i = 0; i < length; i++)
        {
            sbac_encode_bin_ep(temp & 0x0001, sbac, bs);
            temp = temp >> 1;
        }
    }
}

void enc_eco_sao_type(ENC_SBAC *sbac, COM_BSW *bs, SAOBlkParam *saoBlkParam)
{
    int value1 = 0;
    int value2 = 0;
    assert(saoBlkParam->modeIdc == SAO_MODE_NEW);
    if (saoBlkParam->typeIdc == SAO_TYPE_BO)
    {
        value1 = saoBlkParam->startBand;
        value2 = 1;//write start band for BO
        eco_sao_type_AEC(value1, value2, sbac, bs);
        assert(saoBlkParam->deltaband >= 2);
        value1 = saoBlkParam->deltaband - 2;
        value2 = 2;//write delta start band for BO
        eco_sao_type_AEC(value1, value2, sbac, bs);
    }
    else
    {
        assert(saoBlkParam->typeIdc >= SAO_TYPE_EO_0 && saoBlkParam->typeIdc <= SAO_TYPE_EO_45);
        value1 = saoBlkParam->typeIdc;
        value2 = 0;
        eco_sao_type_AEC(value1, value2, sbac, bs);
    }
}

int enc_eco_ALF_param(COM_BSW * bs, COM_PIC_HEADER *sh)
{
    int *pic_alf_on = sh->pic_alf_on;
    ALFParam **alfPictureParam = sh->m_alfPictureParam;

    com_bsw_write(bs, pic_alf_on[Y_C], 1); //"alf_pic_flag_Y",
    com_bsw_write(bs, pic_alf_on[U_C], 1); //"alf_pic_flag_Cb",
    com_bsw_write(bs, pic_alf_on[V_C], 1); //"alf_pic_flag_Cr",

    if (pic_alf_on[Y_C] || pic_alf_on[U_C] || pic_alf_on[V_C])
    {
        for (int compIdx = 0; compIdx < N_C; compIdx++)
        {
            if (pic_alf_on[compIdx])
            {
                enc_eco_AlfCoeff(bs, alfPictureParam[compIdx]);
            }
        }
    }
    return COM_OK;
}

void enc_eco_AlfLCUCtrl(ENC_SBAC *sbac, COM_BSW *bs, int iflag)
{
    enc_sbac_encode_bin(iflag, sbac, sbac->ctx.alf_lcu_enable, bs);
}

void enc_eco_AlfCoeff(COM_BSW *bs, ALFParam *Alfp)
{
    int pos, i;
    int groupIdx[NO_VAR_BINS];
    int f = 0;
    const int numCoeff = (int)ALF_MAX_NUM_COEF;
    unsigned int noFilters;
    //Bitstream *bitstream = currBitStream;
    switch (Alfp->componentID)
    {
    case U_C:
    case V_C:
    {
        for (pos = 0; pos < numCoeff; pos++)
        {
            com_bsw_write_se(bs, Alfp->coeffmulti[0][pos]);
            //se_v("Chroma ALF coefficients", Alfp->coeffmulti[0][pos], bitstream);
        }
    }
    break;
    case Y_C:
    {
        noFilters = Alfp->filters_per_group - 1;
        com_bsw_write_ue(bs, noFilters); //ue_v("ALF filter number", noFilters, bitstream);
        groupIdx[0] = 0;
        f++;
        if (Alfp->filters_per_group > 1)
        {
            for (i = 1; i < NO_VAR_BINS; i++)
            {
                if (Alfp->filterPattern[i] == 1)
                {
                    groupIdx[f] = i;
                    f++;
                }
            }
        }
        for (f = 0; f < Alfp->filters_per_group; f++)
        {
            if (f > 0 && Alfp->filters_per_group != 16)
            {
                com_bsw_write_ue(bs, (unsigned int)(groupIdx[f] - groupIdx[f - 1]));
                //ue_v("Region distance", (unsigned int)(groupIdx[f] - groupIdx[f - 1]), bitstream);
            }
            for (pos = 0; pos < numCoeff; pos++)
            {
                com_bsw_write_se(bs, Alfp->coeffmulti[f][pos]);
                //se_v("Luma ALF coefficients", Alfp->coeffmulti[f][pos], bitstream);
            }
        }
    }
    break;
    default:
    {
        printf("Not a legal component ID\n");
        assert(0);
        exit(-1);
    }
    }
}

void enc_eco_lcu_delta_qp(COM_BSW *bs, int val, int last_dqp)
{
    ENC_SBAC *sbac = GET_SBAC_ENC(bs);
    COM_SBAC_CTX *sbac_ctx = &sbac->ctx;
    int act_sym;
    int act_ctx = ((last_dqp != 0) ? 1 : 0);

    if (val > 0)
    {
        act_sym = 2 * val - 1;
    }
    else
    {
        act_sym = -2 * val;
    }

    if (act_sym == 0)
    {
        enc_sbac_encode_bin(1, sbac, sbac_ctx->delta_qp + act_ctx, bs);
    }
    else
    {
        enc_sbac_encode_bin(0, sbac, sbac_ctx->delta_qp + act_ctx, bs);
        act_ctx = 2;
        if (act_sym == 1)
        {
            enc_sbac_encode_bin(1, sbac, sbac_ctx->delta_qp + act_ctx, bs);
        }
        else
        {
            enc_sbac_encode_bin(0, sbac, sbac_ctx->delta_qp + act_ctx, bs);
            act_ctx++;
            while (act_sym > 2)
            {
                enc_sbac_encode_bin(0, sbac, sbac_ctx->delta_qp + act_ctx, bs);
                act_sym--;
            }
            enc_sbac_encode_bin(1, sbac, sbac_ctx->delta_qp + act_ctx, bs);
        }
    }
}

#if EXTENSION_USER_DATA
void enc_user_data(ENC_CTX * ctx, COM_BSW * bs)
{
    com_bsw_write(bs, 0x1, 24);
    com_bsw_write(bs, 0xB2, 8);
#if WRITE_MD5_IN_USER_DATA
    /* encode user data: MD5*/
    enc_eco_udata(ctx, bs);
#endif

}

int enc_sequence_display_extension(COM_BSW * bs,int colour_primaries,int transfer_characteristics)
{
    com_bsw_write(bs, 2, 4);                            // extension_id                 f(4)
    com_bsw_write(bs, 0, 3);                            // video_format                 u(3)
    com_bsw_write1(bs, 0);                              // sample_range                 u(1)
    com_bsw_write1(bs, 1);                              // colour_description           u(1)
    com_bsw_write(bs, colour_primaries, 8);             // colour_primaries             u(8)
    com_bsw_write(bs, transfer_characteristics, 8);     // transfer_characteristics     u(8)
    printf("colour_primaries = %d\n", colour_primaries);
    printf("transfer_characteristics = %d\n", transfer_characteristics);
    com_bsw_write(bs, 1, 8);                            // matrix_coefficients          u(8)

    com_bsw_write(bs, 8, 14);                           // display_horizontal_size      u(14)
    com_bsw_write1(bs, 1);                              // marker_bit                   f(1)
    com_bsw_write(bs, 8, 14);                           // display_vertical_size        u(14)
    com_bsw_write1(bs, 1);                              // td_mode_flag                 u(1)
    com_bsw_write(bs, 1, 8);                            // td_packing_mode              u(8)
    com_bsw_write1(bs, 0);                              // view_reverse_flag            u(1)

    com_bsw_write1(bs, 1);                              // stuffing bit
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        com_bsw_write1(bs, 0);
    }
    return 0;
}

int enc_temporal_scalability_extension(COM_BSW * bs)
{
    com_bsw_write(bs,3, 4); // extension_id f(4)
    {
        // reserved for temporal_scalability_extension
    }
    return 0;
}

int enc_copyright_extension(COM_BSW * bs)
{
    com_bsw_write(bs,4, 4); // extension_id f(4)
    {
        // reserved for copyright_extension
    }
    return 0;
}

int enc_mastering_display_and_content_metadata_extension(COM_BSW * bs)
{
    com_bsw_write(bs, 0xa,4); // extension_id f(4)
    {
        // reserved for mastering_display_and_content_metadata_extension
    }
    return 0;
}

int enc_camera_parameters_extension(COM_BSW * bs)
{
    com_bsw_write(bs, 0xb,4); // extension_id f(4)
    {
        // reserved for camera_parameters_extension
    }
    return 0;
}

#if HLS_12_8
int enc_cross_random_access_point_referencing_extension(COM_BSW * bs, unsigned int crr_lib_number, int* crr_lib_pid)
{
    com_bsw_write(bs, 0xd, 4);                              // extension_id            f(4)
    com_bsw_write(bs, crr_lib_number, 3);                   // crr_lib_number          u(3)
    com_bsw_write1(bs, 1);                                  // marker_bit              u(1)
    printf("crr_lib_number=%d:\n", crr_lib_number);

    unsigned int i = 0;
    while (i < crr_lib_number)
    {
        com_bsw_write(bs, crr_lib_pid[i], 9);               // crr_lib_pid[i]          u(9)
        printf("crr_lib_pid[%d]=%d\n", i, crr_lib_pid[i]);
        i++;
        if (i % 2 == 0)
        {
            com_bsw_write1(bs, 1);                          // marker_bit              u(1)
        }   
    }

    com_bsw_write1(bs, 1);                                  // stuffing bit
    while (!COM_BSR_IS_BYTE_ALIGN(bs))
    {
        com_bsw_write1(bs, 0);
    }
    return 0;
}
#endif

int enc_roi_parameters_extension(COM_BSW * bs, int slice_type)
{
    com_bsw_write(bs, 0xc, 4); // extension_id f(4)
    {
        // reserved for roi_parameters_extension
    }
    return 0;
}

int enc_picture_display_extension(COM_BSW * bs)
{
    com_bsw_write(bs, 2, 4);   // extension_id f(4)
    {
        // reserved for picture_display_extension
    }
    return 0;
}

int enc_extension_data(ENC_CTX* ctx, COM_BSW * bs, int i, COM_SQH* sqh, COM_PIC_HEADER* pic_header)
{
#if HLS_12_6_7
    int colour_primaries = 1;
    int transfer_characteristics = 1;
    for (colour_primaries = 1; colour_primaries <= 9; colour_primaries++)
    {
        com_bsw_write(bs, 0x1, 24);
        com_bsw_write(bs, 0xB5, 8); // extension_start_code f(32)
        enc_sequence_display_extension(bs, colour_primaries, transfer_characteristics);
    }
    for (transfer_characteristics = 1; transfer_characteristics <= 14; transfer_characteristics++)
    {
        com_bsw_write(bs, 0x1, 24);
        com_bsw_write(bs, 0xB5, 8); // extension_start_code f(32)
        enc_sequence_display_extension(bs, colour_primaries, transfer_characteristics);
    }
#endif
#if HLS_12_8
    unsigned int count;
    int crr_lib_pid[MAX_NUM_LIBPIC];

    count = 0;
    for (int j = 0; j < ctx->rpm.libvc_data->num_RLpic; j++)
    {
        if (ctx->rpm.libvc_data->list_poc_of_RLpic[j] >= ctx->ptr && (ctx->rpm.libvc_data->list_poc_of_RLpic[j] < ctx->ptr + ctx->param.i_period))
        {
            crr_lib_pid[count] = ctx->rpm.libvc_data->list_libidx_for_RLpic[j];
            count++;
        }
    }
    if (count > 0)
    {
        com_bsw_write(bs, 0x1, 24);
        com_bsw_write(bs, 0xB5, 8); // extension_start_code f(32)
        enc_cross_random_access_point_referencing_extension(bs, count, crr_lib_pid);
    }
#endif
    return 0;
}

int enc_extension_and_user_data(ENC_CTX* ctx, COM_BSW * bs, int i, u8 isExtension, COM_SQH * sqh, COM_PIC_HEADER* pic_header)
{
    if (isExtension)
    {
        enc_extension_data(ctx, bs, i, sqh, pic_header);
    }
    else
    {
        enc_user_data(ctx, bs);
    }

    return 0;
}
#endif

