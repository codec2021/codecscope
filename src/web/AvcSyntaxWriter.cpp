#include "AvcSyntaxWriter.h"

namespace web
{

  namespace
  {
    std::string n(uint32_t v) { return std::to_string(v); }
    std::string n(int32_t v)  { return std::to_string(v); }
    std::string n(uint8_t v)  { return std::to_string((int)v); }
    std::string n(uint16_t v) { return std::to_string(v); }
    std::string n(std::size_t v) { return std::to_string(v); }

    uint32_t sliceTypeNorm(uint32_t st) { return st % 5; }
  }

  std::string avcNalTypeName(AVC::NALUnitType type)
  {
    switch(type)
    {
      case AVC::NAL_SLICE:        return "NAL_SLICE";
      case AVC::NAL_DPA:          return "NAL_DPA";
      case AVC::NAL_DPB:          return "NAL_DPB";
      case AVC::NAL_DPC:          return "NAL_DPC";
      case AVC::NAL_IDR_SLICE:    return "NAL_IDR_SLICE";
      case AVC::NAL_SEI:          return "NAL_SEI";
      case AVC::NAL_SPS:          return "NAL_SPS";
      case AVC::NAL_PPS:          return "NAL_PPS";
      case AVC::NAL_AUD:          return "NAL_AUD";
      case AVC::NAL_END_SEQUENCE: return "NAL_END_SEQUENCE";
      case AVC::NAL_END_STREAM:   return "NAL_END_STREAM";
      case AVC::NAL_FILLER:       return "NAL_FILLER";
      case AVC::NAL_SPS_EXT:      return "NAL_SPS_EXT";
      case AVC::NAL_PREFIX:       return "NAL_PREFIX";
      case AVC::NAL_SUBSET_SPS:   return "NAL_SUBSET_SPS";
      case AVC::NAL_AUX_SLICE:    return "NAL_AUX_SLICE";
      case AVC::NAL_SLICE_EXT:    return "NAL_SLICE_EXT";
      default:                    return "NAL_UNSPEC";
    }
  }

  void AvcSyntaxWriter::setParameterSets(const std::map<uint32_t, std::shared_ptr<AVC::SPS_NAL> > &spsMap,
                                         const std::map<uint32_t, std::shared_ptr<AVC::PPS_NAL> > &ppsMap)
  {
    m_spsMap = spsMap;
    m_ppsMap = ppsMap;
  }

  std::string AvcSyntaxWriter::write(std::shared_ptr<AVC::NALUnit> pNALUnit)
  {
    SyntaxNode root;
    if(!pNALUnit)
    {
      root.name = "NULL";
    }
    else
    {
      switch(pNALUnit->m_nalHeader.nal_unit_type)
      {
        case AVC::NAL_SPS:
        case AVC::NAL_SUBSET_SPS:
        {
          std::shared_ptr<AVC::SPS_NAL> p = std::dynamic_pointer_cast<AVC::SPS_NAL>(pNALUnit);
          root.name = "SPS";
          createSPS(p->sps, root);
          break;
        }
        case AVC::NAL_PPS:
        {
          std::shared_ptr<AVC::PPS_NAL> p = std::dynamic_pointer_cast<AVC::PPS_NAL>(pNALUnit);
          root.name = "PPS";
          createPPS(p->pps, root);
          break;
        }
        case AVC::NAL_SLICE:
        case AVC::NAL_IDR_SLICE:
        case AVC::NAL_DPA:
        case AVC::NAL_DPB:
        case AVC::NAL_DPC:
        case AVC::NAL_AUX_SLICE:
        case AVC::NAL_SLICE_EXT:
        {
          std::shared_ptr<AVC::Slice_NAL> p = std::dynamic_pointer_cast<AVC::Slice_NAL>(pNALUnit);
          root.name = "Slice Syntax";
          createSlice(p->slice, root);
          break;
        }
        case AVC::NAL_AUD:
        {
          std::shared_ptr<AVC::AUD_NAL> p = std::dynamic_pointer_cast<AVC::AUD_NAL>(pNALUnit);
          root.name = "AUD";
          createAUD(p->aud, root);
          break;
        }
        case AVC::NAL_SEI:
        {
          std::shared_ptr<AVC::SEI_NAL> p = std::dynamic_pointer_cast<AVC::SEI_NAL>(pNALUnit);
          root.name = "SEI";
          createSEI(*p, root);
          break;
        }
        default:
          root.name = avcNalTypeName(pNALUnit->m_nalHeader.nal_unit_type);
      }
    }

    std::string out;
    root.toJson(out);
    return out;
  }

  void AvcSyntaxWriter::createSPS(const AVC::SPS &s, SyntaxNode &p)
  {
    p.add("profile_idc = " + n(s.profile_idc));
    p.add("constraint_set0_flag = " + n(s.constraint_set0_flag));
    p.add("constraint_set1_flag = " + n(s.constraint_set1_flag));
    p.add("constraint_set2_flag = " + n(s.constraint_set2_flag));
    p.add("constraint_set3_flag = " + n(s.constraint_set3_flag));
    p.add("constraint_set4_flag = " + n(s.constraint_set4_flag));
    p.add("constraint_set5_flag = " + n(s.constraint_set5_flag));
    p.add("level_idc = " + n(s.level_idc));
    p.add("seq_parameter_set_id = " + n(s.seq_parameter_set_id));
    p.add("chroma_format_idc = " + n(s.chroma_format_idc));
    if(s.chroma_format_idc == 3)
    {
      SyntaxNode &c = p.add("if( chroma_format_idc == 3 )");
      c.add("separate_colour_plane_flag = " + n(s.separate_colour_plane_flag));
    }
    p.add("bit_depth_luma_minus8 = " + n(s.bit_depth_luma_minus8));
    p.add("bit_depth_chroma_minus8 = " + n(s.bit_depth_chroma_minus8));
    p.add("qpprime_y_zero_transform_bypass_flag = " + n(s.qpprime_y_zero_transform_bypass_flag));
    p.add("seq_scaling_matrix_present_flag = " + n(s.seq_scaling_matrix_present_flag));
    if(s.seq_scaling_matrix_present_flag)
    {
      SyntaxNode &c = p.add("if( seq_scaling_matrix_present_flag )");
      createScalingMatrix(s.scaling_matrix, c);
    }
    p.add("log2_max_frame_num_minus4 = " + n(s.log2_max_frame_num_minus4));
    p.add("pic_order_cnt_type = " + n(s.pic_order_cnt_type));
    if(s.pic_order_cnt_type == 0)
    {
      SyntaxNode &c = p.add("if( pic_order_cnt_type == 0 )");
      c.add("log2_max_pic_order_cnt_lsb_minus4 = " + n(s.log2_max_pic_order_cnt_lsb_minus4));
    }
    else if(s.pic_order_cnt_type == 1)
    {
      SyntaxNode &c = p.add("if( pic_order_cnt_type == 1 )");
      c.add("delta_pic_order_always_zero_flag = " + n(s.delta_pic_order_always_zero_flag));
      c.add("offset_for_non_ref_pic = " + n(s.offset_for_non_ref_pic));
      c.add("offset_for_top_to_bottom_field = " + n(s.offset_for_top_to_bottom_field));
      c.add("num_ref_frames_in_pic_order_cnt_cycle = " + n(s.num_ref_frames_in_pic_order_cnt_cycle));
      if(s.num_ref_frames_in_pic_order_cnt_cycle)
      {
        SyntaxNode &loop = c.add("for( i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )");
        for(std::size_t i = 0; i < s.offset_for_ref_frame.size(); i++)
          loop.add("offset_for_ref_frame[" + n(i) + "] = " + n(s.offset_for_ref_frame[i]));
      }
    }
    p.add("max_num_ref_frames = " + n(s.max_num_ref_frames));
    p.add("gaps_in_frame_num_value_allowed_flag = " + n(s.gaps_in_frame_num_value_allowed_flag));
    p.add("pic_width_in_mbs_minus1 = " + n(s.pic_width_in_mbs_minus1));
    p.add("pic_height_in_map_units_minus1 = " + n(s.pic_height_in_map_units_minus1));
    p.add("frame_mbs_only_flag = " + n(s.frame_mbs_only_flag));
    if(!s.frame_mbs_only_flag)
    {
      SyntaxNode &c = p.add("if( !frame_mbs_only_flag )");
      c.add("mb_adaptive_frame_field_flag = " + n(s.mb_adaptive_frame_field_flag));
    }
    p.add("direct_8x8_inference_flag = " + n(s.direct_8x8_inference_flag));
    p.add("frame_cropping_flag = " + n(s.frame_cropping_flag));
    if(s.frame_cropping_flag)
    {
      SyntaxNode &c = p.add("if( frame_cropping_flag )");
      c.add("frame_crop_left_offset = " + n(s.frame_crop_left_offset));
      c.add("frame_crop_right_offset = " + n(s.frame_crop_right_offset));
      c.add("frame_crop_top_offset = " + n(s.frame_crop_top_offset));
      c.add("frame_crop_bottom_offset = " + n(s.frame_crop_bottom_offset));
    }
    p.add("vui_parameters_present_flag = " + n(s.vui_parameters_present_flag));
    if(s.vui_parameters_present_flag)
    {
      SyntaxNode &c = p.add("if( vui_parameters_present_flag )");
      createVui(s.vui_parameters, c);
    }
  }

  void AvcSyntaxWriter::createScalingMatrix(const AVC::ScalingMatrix &sm, SyntaxNode &p)
  {
    SyntaxNode &loop = p.add("for( i = 0; i < 8; i++ )");
    for(int i = 0; i < 8; i++)
    {
      loop.add("seq_scaling_list_present_flag[" + n(i) + "] = " + n(sm.seq_scaling_list_present_flag[i]));
      if(sm.seq_scaling_list_present_flag[i])
      {
        std::string str = "scaling_list[" + n(i) + "] = { ";
        for(std::size_t j = 0; j < sm.scaling_list[i].size(); j++)
        {
          if(j) str += ", ";
          str += n(sm.scaling_list[i][j]);
        }
        str += " }";
        loop.add(str);
      }
    }
  }

  void AvcSyntaxWriter::createVui(const AVC::VuiParameters &v, SyntaxNode &p)
  {
    p.add("aspect_ratio_info_present_flag = " + n(v.aspect_ratio_info_present_flag));
    if(v.aspect_ratio_info_present_flag)
    {
      SyntaxNode &c = p.add("if( aspect_ratio_info_present_flag )");
      c.add("aspect_ratio_idc = " + n(v.aspect_ratio_idc));
      if(v.aspect_ratio_idc == 255)
      {
        SyntaxNode &c2 = c.add("if( aspect_ratio_idc == 255 )");
        c2.add("sar_width = " + n(v.sar_width));
        c2.add("sar_height = " + n(v.sar_height));
      }
    }
    p.add("overscan_info_present_flag = " + n(v.overscan_info_present_flag));
    if(v.overscan_info_present_flag)
    {
      SyntaxNode &c = p.add("if( overscan_info_present_flag )");
      c.add("overscan_appropriate_flag = " + n(v.overscan_appropriate_flag));
    }
    p.add("video_signal_type_present_flag = " + n(v.video_signal_type_present_flag));
    if(v.video_signal_type_present_flag)
    {
      SyntaxNode &c = p.add("if( video_signal_type_present_flag )");
      c.add("video_format = " + n(v.video_format));
      c.add("video_full_range_flag = " + n(v.video_full_range_flag));
      c.add("colour_description_present_flag = " + n(v.colour_description_present_flag));
      if(v.colour_description_present_flag)
      {
        SyntaxNode &c2 = c.add("if( colour_description_present_flag )");
        c2.add("colour_primaries = " + n(v.colour_primaries));
        c2.add("transfer_characteristics = " + n(v.transfer_characteristics));
        c2.add("matrix_coefficients = " + n(v.matrix_coefficients));
      }
    }
    p.add("chroma_loc_info_present_flag = " + n(v.chroma_loc_info_present_flag));
    if(v.chroma_loc_info_present_flag)
    {
      SyntaxNode &c = p.add("if( chroma_loc_info_present_flag )");
      c.add("chroma_sample_loc_type_top_field = " + n(v.chroma_sample_loc_type_top_field));
      c.add("chroma_sample_loc_type_bottom_field = " + n(v.chroma_sample_loc_type_bottom_field));
    }
    p.add("timing_info_present_flag = " + n(v.timing_info_present_flag));
    if(v.timing_info_present_flag)
    {
      SyntaxNode &c = p.add("if( timing_info_present_flag )");
      c.add("num_units_in_tick = " + n(v.num_units_in_tick));
      c.add("time_scale = " + n(v.time_scale));
      c.add("fixed_frame_rate_flag = " + n(v.fixed_frame_rate_flag));
    }
    p.add("nal_hrd_parameters_present_flag = " + n(v.nal_hrd_parameters_present_flag));
    p.add("vcl_hrd_parameters_present_flag = " + n(v.vcl_hrd_parameters_present_flag));
    p.add("bitstream_restriction_flag = " + n(v.bitstream_restriction_flag));
  }

  void AvcSyntaxWriter::createPPS(const AVC::PPS &p, SyntaxNode &parent)
  {
    parent.add("pic_parameter_set_id = " + n(p.pic_parameter_set_id));
    parent.add("seq_parameter_set_id = " + n(p.seq_parameter_set_id));
    parent.add("entropy_coding_mode_flag = " + n(p.entropy_coding_mode_flag));
    parent.add("bottom_field_pic_order_in_frame_present_flag = " + n(p.bottom_field_pic_order_in_frame_present_flag));
    parent.add("num_slice_groups_minus1 = " + n(p.num_slice_groups_minus1));
    if(p.num_slice_groups_minus1 > 0)
    {
      SyntaxNode &c = parent.add("if( num_slice_groups_minus1 > 0 )");
      c.add("slice_group_map_type = " + n(p.slice_group_map_type));
      if(p.slice_group_map_type == 0)
      {
        SyntaxNode &loop = c.add("for( iGroup = 0; iGroup <= num_slice_groups_minus1; iGroup++ )");
        for(std::size_t i = 0; i < p.run_length_minus1_list.size(); i++)
          loop.add("run_length_minus1[" + n(i) + "] = " + n(p.run_length_minus1_list[i]));
      }
      else if(p.slice_group_map_type == 2)
      {
        SyntaxNode &loop = c.add("for( iGroup = 0; iGroup < num_slice_groups_minus1; iGroup++ )");
        for(uint32_t i = 0; i < p.num_slice_groups_minus1; i++)
        {
          loop.add("top_left[" + n(i) + "] = " + n(p.top_left[i]));
          loop.add("bottom_right[" + n(i) + "] = " + n(p.bottom_right[i]));
        }
      }
      else if(p.slice_group_map_type >= 3 && p.slice_group_map_type <= 5)
      {
        c.add("slice_group_change_direction_flag = " + n(p.slice_group_change_direction_flag));
        c.add("slice_group_change_rate_minus1 = " + n(p.slice_group_change_rate_minus1));
      }
      else if(p.slice_group_map_type == 6)
      {
        c.add("pic_size_in_map_units_minus1 = " + n(p.pic_size_in_map_units_minus1));
        std::string str = "slice_group_id = { ";
        for(std::size_t i = 0; i < p.slice_group_id.size(); i++)
        {
          if(i) str += ", ";
          str += n(p.slice_group_id[i]);
        }
        str += " }";
        c.add(str);
      }
    }
    parent.add("num_ref_idx_l0_default_active_minus1 = " + n(p.num_ref_idx_l0_default_active_minus1));
    parent.add("num_ref_idx_l1_default_active_minus1 = " + n(p.num_ref_idx_l1_default_active_minus1));
    parent.add("weighted_pred_flag = " + n(p.weighted_pred_flag));
    parent.add("weighted_bipred_idc = " + n(p.weighted_bipred_idc));
    parent.add("pic_init_qp_minus26 = " + n(p.pic_init_qp_minus26));
    parent.add("pic_init_qs_minus26 = " + n(p.pic_init_qs_minus26));
    parent.add("chroma_qp_index_offset = " + n(p.chroma_qp_index_offset));
    parent.add("deblocking_filter_control_present_flag = " + n(p.deblocking_filter_control_present_flag));
    parent.add("constrained_intra_pred_flag = " + n(p.constrained_intra_pred_flag));
    parent.add("redundant_pic_cnt_present_flag = " + n(p.redundant_pic_cnt_present_flag));
    parent.add("transform_8x8_mode_flag = " + n(p.transform_8x8_mode_flag));
    parent.add("pic_scaling_matrix_present_flag = " + n(p.pic_scaling_matrix_present_flag));
    if(p.pic_scaling_matrix_present_flag)
    {
      SyntaxNode &c = parent.add("if( pic_scaling_matrix_present_flag )");
      createScalingMatrix(p.scaling_matrix, c);
    }
    parent.add("second_chroma_qp_index_offset = " + n(p.second_chroma_qp_index_offset));
  }

  void AvcSyntaxWriter::createSlice(const AVC::Slice &s, SyntaxNode &p)
  {
    p.add("first_mb_in_slice = " + n(s.first_mb_in_slice));
    p.add("slice_type = " + n(s.slice_type));
    p.add("pic_parameter_set_id = " + n(s.pic_parameter_set_id));

    std::shared_ptr<AVC::PPS_NAL> ppps;
    auto itPps = m_ppsMap.find(s.pic_parameter_set_id);
    if(itPps != m_ppsMap.end())
      ppps = itPps->second;

    std::shared_ptr<AVC::SPS_NAL> psps;
    if(ppps)
    {
      auto itSps = m_spsMap.find(ppps->pps.seq_parameter_set_id);
      if(itSps != m_spsMap.end())
        psps = itSps->second;
    }

    bool separateColourPlane = psps && psps->sps.separate_colour_plane_flag;
    if(separateColourPlane)
    {
      SyntaxNode &c = p.add("if( separate_colour_plane_flag )");
      c.add("colour_plane_id = " + n(s.colour_plane_id));
    }

    p.add("frame_num = " + n(s.frame_num));

    if(psps && !psps->sps.frame_mbs_only_flag)
    {
      SyntaxNode &c = p.add("if( !frame_mbs_only_flag )");
      c.add("field_pic_flag = " + n(s.field_pic_flag));
      if(s.field_pic_flag)
      {
        SyntaxNode &c2 = c.add("if( field_pic_flag )");
        c2.add("bottom_field_flag = " + n(s.bottom_field_flag));
      }
    }

    p.add("idr_pic_id = " + n(s.idr_pic_id));
    p.add("pic_order_cnt_lsb = " + n(s.pic_order_cnt_lsb));
    if(s.delta_pic_order_cnt_bottom != 0)
      p.add("delta_pic_order_cnt_bottom = " + n(s.delta_pic_order_cnt_bottom));
    if(s.delta_pic_order_cnt[0] != 0 || s.delta_pic_order_cnt[1] != 0)
    {
      p.add("delta_pic_order_cnt[0] = " + n(s.delta_pic_order_cnt[0]));
      p.add("delta_pic_order_cnt[1] = " + n(s.delta_pic_order_cnt[1]));
    }
    if(ppps && ppps->pps.redundant_pic_cnt_present_flag)
      p.add("redundant_pic_cnt = " + n(s.redundant_pic_cnt));

    uint32_t st = sliceTypeNorm(s.slice_type);
    bool isB = (st == 1);
    bool isIorSI = (st == 2 || st == 4);
    bool isPorSP = (st == 0 || st == 3);

    if(isB)
      p.add("direct_spatial_mv_pred_flag = " + n(s.direct_spatial_mv_pred_flag));

    if(isPorSP || isB)
    {
      SyntaxNode &c = p.add("if( slice_type != I && slice_type != SI )");
      c.add("num_ref_idx_active_override_flag = " + n(s.num_ref_idx_active_override_flag));
      if(s.num_ref_idx_active_override_flag)
      {
        SyntaxNode &c2 = c.add("if( num_ref_idx_active_override_flag )");
        c2.add("num_ref_idx_l0_active_minus1 = " + n(s.num_ref_idx_l0_active_minus1));
        if(isB)
          c2.add("num_ref_idx_l1_active_minus1 = " + n(s.num_ref_idx_l1_active_minus1));
      }
    }

    if(!isIorSI)
      createRefPicListModification(s.ref_pic_list_modification, isB, p);

    if(ppps && ((ppps->pps.weighted_pred_flag && isPorSP) || (ppps->pps.weighted_bipred_idc == 1 && isB)))
      createPredWeightTable(s.pred_weight_table, s.slice_type, p);

    createDecRefPicMarking(s, p);

    if(ppps && ppps->pps.entropy_coding_mode_flag && !isIorSI)
      p.add("cabac_init_idc = " + n(s.cabac_init_idc));

    p.add("slice_qp_delta = " + n(s.slice_qp_delta));

    if(st == 3 || st == 4)
    {
      if(st == 3)
        p.add("sp_for_switch_flag = " + n(s.sp_for_switch_flag));
      p.add("slice_qs_delta = " + n(s.slice_qs_delta));
    }

    if(ppps && ppps->pps.deblocking_filter_control_present_flag)
    {
      SyntaxNode &c = p.add("if( deblocking_filter_control_present_flag )");
      c.add("disable_deblocking_filter_idc = " + n(s.disable_deblocking_filter_idc));
      if(s.disable_deblocking_filter_idc != 1)
      {
        SyntaxNode &c2 = c.add("if( disable_deblocking_filter_idc != 1 )");
        c2.add("slice_alpha_c0_offset_div2 = " + n(s.slice_alpha_c0_offset_div2));
        c2.add("slice_beta_offset_div2 = " + n(s.slice_beta_offset_div2));
      }
    }
  }

  void AvcSyntaxWriter::createRefPicListModification(const AVC::RefPicListModification &r, bool isB, SyntaxNode &p)
  {
    p.add("ref_pic_list_modification_flag_l0 = " + n(r.ref_pic_list_modification_flag_l0));
    if(r.ref_pic_list_modification_flag_l0)
    {
      SyntaxNode &loop = p.add("for( i = 0; i < num_ref_idx_l0_active_minus1 + 1; i++ )");
      for(std::size_t i = 0; i < r.modification_of_pic_nums_idc_l0.size(); i++)
      {
        loop.add("modification_of_pic_nums_idc = " + n(r.modification_of_pic_nums_idc_l0[i]));
        if(r.modification_of_pic_nums_idc_l0[i] == 0 || r.modification_of_pic_nums_idc_l0[i] == 1)
          loop.add("abs_diff_pic_num_minus1 = " + n(r.abs_diff_pic_num_minus1_l0[i]));
        else if(r.modification_of_pic_nums_idc_l0[i] == 2)
          loop.add("long_term_pic_num = " + n(r.long_term_pic_num_l0[i]));
      }
    }
    if(isB)
    {
      p.add("ref_pic_list_modification_flag_l1 = " + n(r.ref_pic_list_modification_flag_l1));
      if(r.ref_pic_list_modification_flag_l1)
      {
        SyntaxNode &loop = p.add("for( i = 0; i < num_ref_idx_l1_active_minus1 + 1; i++ )");
        for(std::size_t i = 0; i < r.modification_of_pic_nums_idc_l1.size(); i++)
        {
          loop.add("modification_of_pic_nums_idc = " + n(r.modification_of_pic_nums_idc_l1[i]));
          if(r.modification_of_pic_nums_idc_l1[i] == 0 || r.modification_of_pic_nums_idc_l1[i] == 1)
            loop.add("abs_diff_pic_num_minus1 = " + n(r.abs_diff_pic_num_minus1_l1[i]));
          else if(r.modification_of_pic_nums_idc_l1[i] == 2)
            loop.add("long_term_pic_num = " + n(r.long_term_pic_num_l1[i]));
        }
      }
    }
  }

  void AvcSyntaxWriter::createDecRefPicMarking(const AVC::Slice &s, SyntaxNode &p)
  {
    if(s.dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag)
    {
      p.add("adaptive_ref_pic_marking_mode_flag = " + n(s.dec_ref_pic_marking.adaptive_ref_pic_marking_mode_flag));
      for(std::size_t i = 0; i < s.dec_ref_pic_marking.operations.size(); i++)
      {
        const AVC::DecRefPicMarking::Op &o = s.dec_ref_pic_marking.operations[i];
        p.add("memory_management_control_operation = " + n(o.memory_management_control_operation));
        if(o.memory_management_control_operation == 1)
          p.add("difference_of_pic_nums_minus1 = " + n(o.difference_of_pic_nums_minus1));
        if(o.memory_management_control_operation == 2)
          p.add("long_term_pic_num = " + n(o.long_term_pic_num));
        if(o.memory_management_control_operation == 3 || o.memory_management_control_operation == 6)
          p.add("long_term_frame_idx = " + n(o.long_term_frame_idx));
      }
    }
  }

  void AvcSyntaxWriter::createPredWeightTable(const AVC::PredWeightTable &w, uint32_t sliceType, SyntaxNode &p)
  {
    p.add("luma_log2_weight_denom = " + n(w.luma_log2_weight_denom));
    p.add("chroma_log2_weight_denom = " + n(w.chroma_log2_weight_denom));

    for(std::size_t i = 0; i < w.l0.size(); i++)
    {
      p.add("luma_weight_l0_flag[" + n(i) + "] = " + n(w.l0[i].luma_weight_flag));
      if(w.l0[i].luma_weight_flag)
      {
        p.add("luma_weight_l0[" + n(i) + "] = " + n(w.l0[i].luma_weight));
        p.add("luma_offset_l0[" + n(i) + "] = " + n(w.l0[i].luma_offset));
      }
      p.add("chroma_weight_l0_flag[" + n(i) + "] = " + n(w.l0[i].chroma_weight_flag));
      if(w.l0[i].chroma_weight_flag)
      {
        p.add("chroma_weight_l0[" + n(i) + "] = { " + n(w.l0[i].chroma_weight[0]) + ", " + n(w.l0[i].chroma_weight[1]) + " }");
        p.add("chroma_offset_l0[" + n(i) + "] = { " + n(w.l0[i].chroma_offset[0]) + ", " + n(w.l0[i].chroma_offset[1]) + " }");
      }
    }

    if(sliceTypeNorm(sliceType) == 1)
    {
      for(std::size_t i = 0; i < w.l1.size(); i++)
      {
        p.add("luma_weight_l1_flag[" + n(i) + "] = " + n(w.l1[i].luma_weight_flag));
        if(w.l1[i].luma_weight_flag)
        {
          p.add("luma_weight_l1[" + n(i) + "] = " + n(w.l1[i].luma_weight));
          p.add("luma_offset_l1[" + n(i) + "] = " + n(w.l1[i].luma_offset));
        }
        p.add("chroma_weight_l1_flag[" + n(i) + "] = " + n(w.l1[i].chroma_weight_flag));
        if(w.l1[i].chroma_weight_flag)
        {
          p.add("chroma_weight_l1[" + n(i) + "] = { " + n(w.l1[i].chroma_weight[0]) + ", " + n(w.l1[i].chroma_weight[1]) + " }");
          p.add("chroma_offset_l1[" + n(i) + "] = { " + n(w.l1[i].chroma_offset[0]) + ", " + n(w.l1[i].chroma_offset[1]) + " }");
        }
      }
    }
  }

  void AvcSyntaxWriter::createAUD(const AVC::AUD &a, SyntaxNode &p)
  {
    p.add("primary_pic_type = " + n(a.primary_pic_type));
  }

  void AvcSyntaxWriter::createSEI(const AVC::SEI_NAL &sei, SyntaxNode &p)
  {
    for(std::size_t i = 0; i < sei.messages.size(); i++)
    {
      const AVC::SeiMessage &m = sei.messages[i];
      SyntaxNode &c = p.add("sei_message(" + n(i) + ")");
      c.add("payload_type = " + n(m.payload_type));
      c.add("payload_size = " + n(m.payload_size));
      std::string str = "payload_data = { ";
      for(std::size_t j = 0; j < m.payload_data.size() && j < 64; j++)
      {
        if(j) str += ", ";
        str += n(m.payload_data[j]);
      }
      if(m.payload_data.size() > 64)
        str += ", ... (" + n((uint32_t)m.payload_data.size()) + " bytes)";
      str += " }";
      c.add(str);
    }
  }

}
