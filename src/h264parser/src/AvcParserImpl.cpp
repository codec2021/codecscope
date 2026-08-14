#include "AvcParserImpl.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cassert>

namespace AVC
{

  namespace
  {
    bool isHighProfile(uint32_t profile_idc)
    {
      return profile_idc == 100 || profile_idc == 110 || profile_idc == 122 ||
             profile_idc == 244 || profile_idc == 44  || profile_idc == 83  ||
             profile_idc == 86  || profile_idc == 118 || profile_idc == 128 ||
             profile_idc == 138 || profile_idc == 139 || profile_idc == 134 ||
             profile_idc == 135;
    }

    uint32_t sliceTypeNorm(uint32_t sliceType)
    {
      return sliceType % 5;
    }
  }

  void AvcParserImpl::addConsumer(Consumer *pconsumer)
  {
    m_consumers.push_back(pconsumer);
  }

  void AvcParserImpl::releaseConsumer(Consumer *pconsumer)
  {
    m_consumers.remove(pconsumer);
  }

  void AvcParserImpl::onWarning(const std::string &warning, const Info *pInfo, WarningType type)
  {
    for(auto itr = m_consumers.begin(); itr != m_consumers.end(); itr++)
      (*itr) -> onWarning(warning, pInfo, type);
  }

  std::size_t AvcParserImpl::process(const uint8_t *pdata, std::size_t size, std::size_t offset)
  {
    std::size_t parsed = 0;
    bool parseFailed = false;

    for(std::size_t pos = 0; pos + 3 < size;)
    {
      std::size_t startOffset = 3;
      bool naluFinded = pdata[pos] == 0 && pdata[pos+1] == 0 && pdata[pos+2] == 1;

      if(!naluFinded)
      {
        if(size - pos >= 4 && pdata[pos] == 0 && pdata[pos+1] == 0 && pdata[pos+2] == 0 && pdata[pos+3] == 1)
        {
          naluFinded = true;
          startOffset = 4;
        }
      }

      if(naluFinded)
      {
        Parser::Info info;
        info.m_position = offset + pos;

        try
        {
          processNALUnit(pdata + pos + startOffset, size - pos - startOffset, info);
        }
        catch(std::runtime_error &)
        {
          parseFailed = true;
          break;
        }
        catch(std::bad_alloc &)
        {
        }

        parsed = pos;
        pos += 3;
      }
      else
        pos++;
    }

    if(!parseFailed)
    {
      parsed = size;
      for(std::size_t i = 0; i < 3 && i < size; i++)
      {
        if(pdata[size - i - 1] == 0)
          parsed--;
        else
          break;
      }
    }

    return parsed;
  }

  void AvcParserImpl::processNALUnit(const uint8_t *pdata, std::size_t size, const Parser::Info &info)
  {
    BitstreamReader bs(pdata, size);

    NALHeader header;
    processNALUnitHeader(bs, &header);

    std::shared_ptr<NALUnit> pnalU;

    switch(header.nal_unit_type)
    {
      case NAL_SPS:
      case NAL_SUBSET_SPS:
      {
        std::shared_ptr<SPS_NAL> psps(new SPS_NAL(header));
        processSPS(psps, bs, info);
        pnalU = psps;
        m_spsMap[psps->sps.seq_parameter_set_id] = psps;
        break;
      }

      case NAL_PPS:
      {
        std::shared_ptr<PPS_NAL> ppps(new PPS_NAL(header));
        processPPS(ppps, bs, info);
        pnalU = ppps;
        m_ppsMap[ppps->pps.pic_parameter_set_id] = ppps;
        break;
      }

      case NAL_SLICE:
      case NAL_IDR_SLICE:
      case NAL_DPA:
      case NAL_DPB:
      case NAL_DPC:
      case NAL_AUX_SLICE:
      case NAL_SLICE_EXT:
      {
        std::shared_ptr<Slice_NAL> pslice(new Slice_NAL(header));
        processSlice(pslice, bs, info);
        pnalU = pslice;
        break;
      }

      case NAL_AUD:
      {
        std::shared_ptr<AUD_NAL> paud(new AUD_NAL(header));
        processAUD(paud, bs);
        pnalU = paud;
        break;
      }

      case NAL_SEI:
      {
        std::shared_ptr<SEI_NAL> psei(new SEI_NAL(header));
        processSEI(psei, bs);
        pnalU = psei;
        break;
      }

      default:
        pnalU = std::shared_ptr<NALUnit>(new NALUnit(header));
    }

    for(auto itr = m_consumers.begin(); itr != m_consumers.end(); itr++)
      (*itr) -> onNALUnit(pnalU, &info);
  }

  void AvcParserImpl::processNALUnitHeader(BitstreamReader &bs, NALHeader *header)
  {
    header->forbidden_zero_bit = bs.getBit();
    header->nal_ref_idc = bs.getBits(2);
    header->nal_unit_type = (NALUnitType)bs.getBits(5);
  }

  void AvcParserImpl::processSPS(std::shared_ptr<SPS_NAL> pnal, BitstreamReader &bs, const Parser::Info &info)
  {
    SPS &sps = pnal->sps;
    sps.toDefault();

    sps.profile_idc = bs.getBits(8);
    sps.constraint_set0_flag = bs.getBit();
    sps.constraint_set1_flag = bs.getBit();
    sps.constraint_set2_flag = bs.getBit();
    sps.constraint_set3_flag = bs.getBit();
    sps.constraint_set4_flag = bs.getBit();
    sps.constraint_set5_flag = bs.getBit();
    bs.getBits(2); // reserved_zero_2bits
    sps.level_idc = bs.getBits(8);
    sps.seq_parameter_set_id = bs.getGolombU();

    if(isHighProfile(sps.profile_idc))
    {
      sps.chroma_format_idc = bs.getGolombU();
      if(sps.chroma_format_idc == 3)
        sps.separate_colour_plane_flag = bs.getBit();
      sps.bit_depth_luma_minus8 = bs.getGolombU();
      sps.bit_depth_chroma_minus8 = bs.getGolombU();
      sps.qpprime_y_zero_transform_bypass_flag = bs.getBit();
      sps.seq_scaling_matrix_present_flag = bs.getBit();
      if(sps.seq_scaling_matrix_present_flag)
        processScalingMatrix(sps.scaling_matrix, bs);
    }

    sps.log2_max_frame_num_minus4 = bs.getGolombU();
    sps.pic_order_cnt_type = bs.getGolombU();

    if(sps.pic_order_cnt_type == 0)
    {
      sps.log2_max_pic_order_cnt_lsb_minus4 = bs.getGolombU();
    }
    else if(sps.pic_order_cnt_type == 1)
    {
      sps.delta_pic_order_always_zero_flag = bs.getBit();
      sps.offset_for_non_ref_pic = bs.getGolombS();
      sps.offset_for_top_to_bottom_field = bs.getGolombS();
      sps.num_ref_frames_in_pic_order_cnt_cycle = bs.getGolombU();
      for(uint32_t i = 0; i < sps.num_ref_frames_in_pic_order_cnt_cycle; i++)
        sps.offset_for_ref_frame.push_back(bs.getGolombS());
    }

    sps.max_num_ref_frames = bs.getGolombU();
    sps.gaps_in_frame_num_value_allowed_flag = bs.getBit();
    sps.pic_width_in_mbs_minus1 = bs.getGolombU();
    sps.pic_height_in_map_units_minus1 = bs.getGolombU();
    sps.frame_mbs_only_flag = bs.getBit();
    if(!sps.frame_mbs_only_flag)
      sps.mb_adaptive_frame_field_flag = bs.getBit();
    sps.direct_8x8_inference_flag = bs.getBit();
    sps.frame_cropping_flag = bs.getBit();
    if(sps.frame_cropping_flag)
    {
      sps.frame_crop_left_offset = bs.getGolombU();
      sps.frame_crop_right_offset = bs.getGolombU();
      sps.frame_crop_top_offset = bs.getGolombU();
      sps.frame_crop_bottom_offset = bs.getGolombU();
    }
    sps.vui_parameters_present_flag = bs.getBit();
    if(sps.vui_parameters_present_flag)
      processVuiParameters(sps.vui_parameters, bs);
  }

  void AvcParserImpl::processScalingMatrix(ScalingMatrix &sm, BitstreamReader &bs)
  {
    sm.toDefault();
    for(int i = 0; i < 8; i++)
    {
      sm.seq_scaling_list_present_flag[i] = bs.getBit();
      if(sm.seq_scaling_list_present_flag[i])
      {
        int sizeOfScalingList = (i < 6) ? 16 : 64;
        int lastScale = 8;
        int nextScale = 8;
        for(int j = 0; j < sizeOfScalingList; j++)
        {
          if(nextScale != 0)
          {
            int32_t deltaScale = bs.getGolombS();
            nextScale = (lastScale + deltaScale + 256) % 256;
            sm.use_default_scaling_matrix_flag[i] = (j == 0 && nextScale == 0);
          }
          int val = (nextScale == 0) ? lastScale : nextScale;
          sm.scaling_list[i].push_back(val);
          lastScale = val;
        }
      }
    }
  }

  void AvcParserImpl::processVuiParameters(VuiParameters &vui, BitstreamReader &bs)
  {
    vui.aspect_ratio_info_present_flag = bs.getBit();
    if(vui.aspect_ratio_info_present_flag)
    {
      vui.aspect_ratio_idc = bs.getBits(8);
      if(vui.aspect_ratio_idc == 255)
      {
        vui.sar_width = bs.getBits(16);
        vui.sar_height = bs.getBits(16);
      }
    }

    vui.overscan_info_present_flag = bs.getBit();
    if(vui.overscan_info_present_flag)
      vui.overscan_appropriate_flag = bs.getBit();

    vui.video_signal_type_present_flag = bs.getBit();
    if(vui.video_signal_type_present_flag)
    {
      vui.video_format = bs.getBits(3);
      vui.video_full_range_flag = bs.getBit();
      vui.colour_description_present_flag = bs.getBit();
      if(vui.colour_description_present_flag)
      {
        vui.colour_primaries = bs.getBits(8);
        vui.transfer_characteristics = bs.getBits(8);
        vui.matrix_coefficients = bs.getBits(8);
      }
    }

    vui.chroma_loc_info_present_flag = bs.getBit();
    if(vui.chroma_loc_info_present_flag)
    {
      vui.chroma_sample_loc_type_top_field = bs.getGolombU();
      vui.chroma_sample_loc_type_bottom_field = bs.getGolombU();
    }

    vui.timing_info_present_flag = bs.getBit();
    if(vui.timing_info_present_flag)
    {
      vui.num_units_in_tick = bs.getBits(32);
      vui.time_scale = bs.getBits(32);
      vui.fixed_frame_rate_flag = bs.getBit();
    }

    vui.nal_hrd_parameters_present_flag = bs.getBit();
    if(vui.nal_hrd_parameters_present_flag)
    {
      // hrd_parameters (解析以保持位流位置)
      uint32_t cpbCntMinus1 = bs.getGolombU();
      bs.getBits(4); // bit_rate_scale
      bs.getBits(4); // cpb_size_scale
      for(uint32_t i = 0; i <= cpbCntMinus1; i++)
      {
        bs.getGolombU(); // bit_rate_value_minus1
        bs.getGolombU(); // cpb_size_value_minus1
        bs.getBit();     // cbr_flag
      }
      bs.getBits(5); // initial_cpb_removal_delay_length_minus1
      bs.getBits(5); // cpb_removal_delay_length_minus1
      bs.getBits(5); // dpb_output_delay_length_minus1
      bs.getBits(5); // time_offset_length
    }

    vui.vcl_hrd_parameters_present_flag = bs.getBit();
    if(vui.vcl_hrd_parameters_present_flag)
    {
      uint32_t cpbCntMinus1 = bs.getGolombU();
      bs.getBits(4);
      bs.getBits(4);
      for(uint32_t i = 0; i <= cpbCntMinus1; i++)
      {
        bs.getGolombU();
        bs.getGolombU();
        bs.getBit();
      }
      bs.getBits(5);
      bs.getBits(5);
      bs.getBits(5);
      bs.getBits(5);
    }

    if(vui.nal_hrd_parameters_present_flag || vui.vcl_hrd_parameters_present_flag)
      bs.getBit(); // low_delay_hrd_flag

    bs.getBit(); // pic_struct_present_flag

    vui.bitstream_restriction_flag = bs.getBit();
    if(vui.bitstream_restriction_flag)
    {
      bs.getBit(); // motion_vectors_over_pic_boundaries_flag
      bs.getGolombU(); // max_bytes_per_pic_denom
      bs.getGolombU(); // max_bits_per_mb_denom
      bs.getGolombU(); // log2_max_mv_length_horizontal
      bs.getGolombU(); // log2_max_mv_length_vertical
      bs.getGolombU(); // max_num_reorder_frames
      bs.getGolombU(); // max_dec_frame_buffering
    }
  }

  void AvcParserImpl::processPPS(std::shared_ptr<PPS_NAL> pnal, BitstreamReader &bs, const Parser::Info &info)
  {
    PPS &pps = pnal->pps;
    pps.toDefault();

    pps.pic_parameter_set_id = bs.getGolombU();
    pps.seq_parameter_set_id = bs.getGolombU();
    pps.entropy_coding_mode_flag = bs.getBit();
    pps.bottom_field_pic_order_in_frame_present_flag = bs.getBit();
    pps.num_slice_groups_minus1 = bs.getGolombU();

    if(pps.num_slice_groups_minus1 > 0)
    {
      pps.slice_group_map_type = bs.getGolombU();

      if(pps.slice_group_map_type == 0)
      {
        for(uint32_t i = 0; i <= pps.num_slice_groups_minus1; i++)
          pps.run_length_minus1_list.push_back(bs.getGolombU());
      }
      else if(pps.slice_group_map_type == 2)
      {
        for(uint32_t i = 0; i < pps.num_slice_groups_minus1; i++)
        {
          pps.top_left[i] = bs.getGolombU();
          pps.bottom_right[i] = bs.getGolombU();
        }
      }
      else if(pps.slice_group_map_type == 3 || pps.slice_group_map_type == 4 || pps.slice_group_map_type == 5)
      {
        pps.slice_group_change_direction_flag = bs.getBit();
        pps.slice_group_change_rate_minus1 = bs.getGolombU();
      }
      else if(pps.slice_group_map_type == 6)
      {
        pps.pic_size_in_map_units_minus1 = bs.getGolombU();
        uint32_t bits = 1;
        uint32_t n = pps.num_slice_groups_minus1 + 1;
        while(n > 1) { bits++; n >>= 1; }
        for(uint32_t i = 0; i <= pps.pic_size_in_map_units_minus1; i++)
          pps.slice_group_id.push_back(bs.getBits(bits));
      }
    }

    pps.num_ref_idx_l0_default_active_minus1 = bs.getGolombU();
    pps.num_ref_idx_l1_default_active_minus1 = bs.getGolombU();
    pps.weighted_pred_flag = bs.getBit();
    pps.weighted_bipred_idc = bs.getBits(2);
    pps.pic_init_qp_minus26 = bs.getGolombS();
    pps.pic_init_qs_minus26 = bs.getGolombS();
    pps.chroma_qp_index_offset = bs.getGolombS();
    pps.deblocking_filter_control_present_flag = bs.getBit();
    pps.constrained_intra_pred_flag = bs.getBit();
    pps.redundant_pic_cnt_present_flag = bs.getBit();

    if(bs.available() >= 8)
    {
      pps.transform_8x8_mode_flag = bs.getBit();
      pps.pic_scaling_matrix_present_flag = bs.getBit();
      if(pps.pic_scaling_matrix_present_flag)
        processScalingMatrix(pps.scaling_matrix, bs);
      pps.second_chroma_qp_index_offset = bs.getGolombS();
    }
  }

  void AvcParserImpl::processSlice(std::shared_ptr<Slice_NAL> pnal, BitstreamReader &bs, const Parser::Info &info)
  {
    processSliceHeader(pnal, bs, info);
  }

  void AvcParserImpl::processSliceHeader(std::shared_ptr<Slice_NAL> pnal, BitstreamReader &bs, const Parser::Info &info)
  {
    Slice &slice = pnal->slice;
    slice.toDefault();

    slice.first_mb_in_slice = bs.getGolombU();
    slice.slice_type = bs.getGolombU();
    slice.pic_parameter_set_id = bs.getGolombU();

    std::shared_ptr<PPS_NAL> ppps = m_ppsMap[slice.pic_parameter_set_id];
    if(!ppps)
    {
      pnal->m_processFailed = true;
      onWarning("Reference PPS not present", &info, REFERENCE_STRUCT_NOT_PRESENT);
      return;
    }

    std::shared_ptr<SPS_NAL> psps = m_spsMap[ppps->pps.seq_parameter_set_id];
    if(!psps)
    {
      pnal->m_processFailed = true;
      onWarning("Reference SPS not present", &info, REFERENCE_STRUCT_NOT_PRESENT);
      return;
    }

    const SPS &sps = psps->sps;
    const PPS &pps = ppps->pps;

    if(sps.separate_colour_plane_flag)
      slice.colour_plane_id = bs.getBits(2);

    slice.frame_num = bs.getBits(sps.log2_max_frame_num_minus4 + 4);

    if(!sps.frame_mbs_only_flag)
    {
      slice.field_pic_flag = bs.getBit();
      if(slice.field_pic_flag)
        slice.bottom_field_flag = bs.getBit();
    }

    bool IdrPicFlag = pnal->m_nalHeader.nal_unit_type == NAL_IDR_SLICE;
    if(IdrPicFlag)
      slice.idr_pic_id = bs.getGolombU();

    if(sps.pic_order_cnt_type == 0)
    {
      slice.pic_order_cnt_lsb = bs.getBits(sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
      if(pps.bottom_field_pic_order_in_frame_present_flag && !slice.field_pic_flag)
        slice.delta_pic_order_cnt_bottom = bs.getGolombS();
    }

    if(sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag)
    {
      slice.delta_pic_order_cnt[0] = bs.getGolombS();
      if(pps.bottom_field_pic_order_in_frame_present_flag && !slice.field_pic_flag)
        slice.delta_pic_order_cnt[1] = bs.getGolombS();
    }

    if(pps.redundant_pic_cnt_present_flag)
      slice.redundant_pic_cnt = bs.getGolombU();

    uint32_t st = sliceTypeNorm(slice.slice_type);
    bool isB = (st == 1);
    bool isIorSI = (st == 2 || st == 4);
    bool isPorSP = (st == 0 || st == 3);

    if(isB)
      slice.direct_spatial_mv_pred_flag = bs.getBit();

    if(isPorSP || isB)
    {
      slice.num_ref_idx_active_override_flag = bs.getBit();
      if(slice.num_ref_idx_active_override_flag)
      {
        slice.num_ref_idx_l0_active_minus1 = bs.getGolombU();
        if(isB)
          slice.num_ref_idx_l1_active_minus1 = bs.getGolombU();
      }
    }

    if(!isIorSI)
      processRefPicListModification(slice.ref_pic_list_modification, bs, isB, info);

    if((pps.weighted_pred_flag && isPorSP) || (pps.weighted_bipred_idc == 1 && isB))
    {
      uint32_t chromaArrayType = sps.separate_colour_plane_flag ? 0 : sps.chroma_format_idc;
      processPredWeightTable(slice.pred_weight_table, slice.slice_type,
                             slice.num_ref_idx_l0_active_minus1, slice.num_ref_idx_l1_active_minus1,
                             chromaArrayType, bs);
    }

    if(pnal->m_nalHeader.nal_ref_idc != 0)
      processDecRefPicMarking(slice, bs, st, IdrPicFlag, info);

    if(pps.entropy_coding_mode_flag && !isIorSI)
      slice.cabac_init_idc = bs.getGolombU();

    slice.slice_qp_delta = bs.getGolombS();

    if(st == 3 || st == 4)
    {
      if(st == 3)
        slice.sp_for_switch_flag = bs.getBit();
      slice.slice_qs_delta = bs.getGolombS();
    }

    if(pps.deblocking_filter_control_present_flag)
    {
      slice.disable_deblocking_filter_idc = bs.getGolombU();
      if(slice.disable_deblocking_filter_idc != 1)
      {
        slice.slice_alpha_c0_offset_div2 = bs.getGolombS();
        slice.slice_beta_offset_div2 = bs.getGolombS();
      }
    }

    if(pps.num_slice_groups_minus1 > 0 &&
       (pps.slice_group_map_type == 3 || pps.slice_group_map_type == 4 || pps.slice_group_map_type == 5))
    {
      uint32_t v = pps.pic_size_in_map_units_minus1 / (pps.slice_group_change_rate_minus1 + 1) + 1;
      uint32_t bits = 0;
      while(v > 1) { bits++; v >>= 1; }
      uint32_t cyc = bs.getBits(bits);
      for(uint32_t i = 0; i < bits; i++)
        slice.slice_group_change_cycle.push_back((cyc >> (bits - i - 1)) & 1);
    }
  }

  void AvcParserImpl::processRefPicListModification(RefPicListModification &r, BitstreamReader &bs, bool isB, const Parser::Info &info)
  {
    r.toDefault();

    r.ref_pic_list_modification_flag_l0 = bs.getBit();
    if(r.ref_pic_list_modification_flag_l0)
    {
      uint32_t idc;
      do
      {
        idc = bs.getGolombU();
        r.modification_of_pic_nums_idc_l0.push_back(idc);
        if(idc == 0 || idc == 1)
          r.abs_diff_pic_num_minus1_l0.push_back(bs.getGolombU());
        else if(idc == 2)
          r.long_term_pic_num_l0.push_back(bs.getGolombU());
      }
      while(idc != 3);
    }

    if(isB)
    {
      r.ref_pic_list_modification_flag_l1 = bs.getBit();
      if(r.ref_pic_list_modification_flag_l1)
      {
        uint32_t idc;
        do
        {
          idc = bs.getGolombU();
          r.modification_of_pic_nums_idc_l1.push_back(idc);
          if(idc == 0 || idc == 1)
            r.abs_diff_pic_num_minus1_l1.push_back(bs.getGolombU());
          else if(idc == 2)
            r.long_term_pic_num_l1.push_back(bs.getGolombU());
        }
        while(idc != 3);
      }
    }
  }

  void AvcParserImpl::processDecRefPicMarking(Slice &slice, BitstreamReader &bs, uint32_t st, bool idrPic, const Parser::Info &info)
  {
    (void)st;
    (void)info;
    DecRefPicMarking &d = slice.dec_ref_pic_marking;
    d.toDefault();

    if(idrPic)
    {
      slice.no_output_of_prior_pics_flag = bs.getBit();
      slice.long_term_reference_flag = bs.getBit();
    }
    else
    {
      d.adaptive_ref_pic_marking_mode_flag = bs.getBit();
      if(d.adaptive_ref_pic_marking_mode_flag)
      {
        uint32_t op;
        do
        {
          op = bs.getGolombU();
          DecRefPicMarking::Op o;
          o.memory_management_control_operation = op;
          if(op == 1)
            o.difference_of_pic_nums_minus1 = bs.getGolombU();
          if(op == 2)
            o.long_term_pic_num = bs.getGolombU();
          if(op == 3 || op == 6)
          {
            o.long_term_frame_idx = bs.getGolombU();
            if(op == 3)
              o.max_long_term_frame_idx_plus1 = 0;
            else
              o.max_long_term_frame_idx_plus1 = bs.getGolombU();
          }
          d.operations.push_back(o);
        }
        while(op != 0);
      }
    }
  }

  void AvcParserImpl::processPredWeightTable(PredWeightTable &p, uint32_t sliceType, uint32_t numL0, uint32_t numL1, uint32_t chromaArrayType, BitstreamReader &bs)
  {
    p.toDefault();

    p.luma_log2_weight_denom = bs.getGolombU();
    if(chromaArrayType != 0)
      p.chroma_log2_weight_denom = bs.getGolombS();

    for(uint32_t i = 0; i <= numL0; i++)
    {
      PredWeightTable::W w;
      w.luma_weight_flag = bs.getBit();
      if(w.luma_weight_flag)
      {
        w.luma_weight = bs.getGolombS();
        w.luma_offset = bs.getGolombS();
      }
      if(chromaArrayType != 0)
      {
        w.chroma_weight_flag = bs.getBit();
        if(w.chroma_weight_flag)
        {
          for(int j = 0; j < 2; j++)
          {
            w.chroma_weight[j] = bs.getGolombS();
            w.chroma_offset[j] = bs.getGolombS();
          }
        }
      }
      p.l0.push_back(w);
    }

    if(sliceTypeNorm(sliceType) == 1) // B slice
    {
      for(uint32_t i = 0; i <= numL1; i++)
      {
        PredWeightTable::W w;
        w.luma_weight_flag = bs.getBit();
        if(w.luma_weight_flag)
        {
          w.luma_weight = bs.getGolombS();
          w.luma_offset = bs.getGolombS();
        }
        if(chromaArrayType != 0)
        {
          w.chroma_weight_flag = bs.getBit();
          if(w.chroma_weight_flag)
          {
            for(int j = 0; j < 2; j++)
            {
              w.chroma_weight[j] = bs.getGolombS();
              w.chroma_offset[j] = bs.getGolombS();
            }
          }
        }
        p.l1.push_back(w);
      }
    }
  }

  void AvcParserImpl::processAUD(std::shared_ptr<AUD_NAL> pnal, BitstreamReader &bs)
  {
    pnal->aud.toDefault();
    pnal->aud.primary_pic_type = bs.getBits(3);
  }

  void AvcParserImpl::processSEI(std::shared_ptr<SEI_NAL> pnal, BitstreamReader &bs)
  {
    while(bs.availableInNalU() > 8 && bs.showBits(8) != 0x80)
    {
      SeiMessage msg;
      msg.toDefault();

      uint32_t payloadType = 0;
      uint8_t b = bs.getBits(8);
      while(b == 0xFF)
      {
        payloadType += 255;
        b = bs.getBits(8);
      }
      payloadType += b;
      msg.payload_type = payloadType;

      uint32_t payloadSize = 0;
      b = bs.getBits(8);
      while(b == 0xFF)
      {
        payloadSize += 255;
        b = bs.getBits(8);
      }
      payloadSize += b;
      msg.payload_size = payloadSize;

      if(payloadSize > bs.availableInNalU() / 8 + 2)
        break;

      for(uint32_t i = 0; i < payloadSize; i++)
        msg.payload_data.push_back(bs.getBits(8));

      pnal->messages.push_back(msg);
    }
  }

}
