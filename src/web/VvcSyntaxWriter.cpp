#include "VvcSyntaxWriter.h"

namespace web
{

  namespace
  {
    std::string n(uint32_t v) { return std::to_string(v); }
    std::string n(int32_t v)  { return std::to_string(v); }
    std::string n(uint8_t v)  { return std::to_string((int)v); }
    std::string n(std::size_t v) { return std::to_string(v); }
  }

  std::string vvcNalTypeName(VVC::NALUnitType type)
  {
    switch(type)
    {
      case VVC::NAL_TRAIL_NUT: return "NAL_TRAIL_NUT";
      case VVC::NAL_STSA_NUT: return "NAL_STSA_NUT";
      case VVC::NAL_RADL_NUT: return "NAL_RADL_NUT";
      case VVC::NAL_RASL_NUT: return "NAL_RASL_NUT";
      case VVC::NAL_RSV_VCL_4: return "NAL_RSV_VCL_4";
      case VVC::NAL_RSV_VCL_5: return "NAL_RSV_VCL_5";
      case VVC::NAL_RSV_VCL_6: return "NAL_RSV_VCL_6";
      case VVC::NAL_IDR_W_RADL: return "NAL_IDR_W_RADL";
      case VVC::NAL_IDR_N_LP: return "NAL_IDR_N_LP";
      case VVC::NAL_CRA_NUT: return "NAL_CRA_NUT";
      case VVC::NAL_GDR_NUT: return "NAL_GDR_NUT";
      case VVC::NAL_RSV_IRAP_11: return "NAL_RSV_IRAP_11";
      case VVC::NAL_OPI: return "NAL_OPI";
      case VVC::NAL_DCI: return "NAL_DCI";
      case VVC::NAL_VPS: return "NAL_VPS";
      case VVC::NAL_SPS: return "NAL_SPS";
      case VVC::NAL_PPS: return "NAL_PPS";
      case VVC::NAL_PREFIX_APS: return "NAL_PREFIX_APS";
      case VVC::NAL_SUFFIX_APS: return "NAL_SUFFIX_APS";
      case VVC::NAL_PH: return "NAL_PH";
      case VVC::NAL_AUD: return "NAL_AUD";
      case VVC::NAL_EOS_NUT: return "NAL_EOS_NUT";
      case VVC::NAL_EOB_NUT: return "NAL_EOB_NUT";
      case VVC::NAL_PREFIX_SEI: return "NAL_PREFIX_SEI";
      case VVC::NAL_SUFFIX_SEI: return "NAL_SUFFIX_SEI";
      case VVC::NAL_FD_NUT: return "NAL_FD_NUT";
      default: return "NAL_UNKNOWN";
    }
  }

  void VvcSyntaxWriter::setParameterSets(const std::map<uint8_t, std::shared_ptr<VVC::SPS_NAL> > &spsMap,
                                         const std::map<uint8_t, std::shared_ptr<VVC::PPS_NAL> > &ppsMap)
  {
    m_spsMap = spsMap;
    m_ppsMap = ppsMap;
  }

  std::string VvcSyntaxWriter::write(std::shared_ptr<VVC::NALUnit> pNALUnit)
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
        case VVC::NAL_VPS:
          root.name = "VPS";
          createVPS(std::dynamic_pointer_cast<VVC::VPS_NAL>(pNALUnit)->vps, root);
          break;
        case VVC::NAL_SPS:
          root.name = "SPS";
          createSPS(std::dynamic_pointer_cast<VVC::SPS_NAL>(pNALUnit)->sps, root);
          break;
        case VVC::NAL_PPS:
          root.name = "PPS";
          createPPS(std::dynamic_pointer_cast<VVC::PPS_NAL>(pNALUnit)->pps, root);
          break;
        case VVC::NAL_PH:
          root.name = "PH (Picture Header)";
          createPH(std::dynamic_pointer_cast<VVC::PH_NAL>(pNALUnit)->ph, root);
          break;
        case VVC::NAL_TRAIL_NUT:
        case VVC::NAL_STSA_NUT:
        case VVC::NAL_RADL_NUT:
        case VVC::NAL_RASL_NUT:
        case VVC::NAL_IDR_W_RADL:
        case VVC::NAL_IDR_N_LP:
        case VVC::NAL_CRA_NUT:
        case VVC::NAL_GDR_NUT:
          root.name = "Slice Syntax";
          createSlice(std::dynamic_pointer_cast<VVC::Slice_NAL>(pNALUnit)->slice, root);
          break;
        case VVC::NAL_AUD:
          root.name = "AUD";
          createAUD(std::dynamic_pointer_cast<VVC::AUD_NAL>(pNALUnit)->aud, root);
          break;
        case VVC::NAL_PREFIX_SEI:
        case VVC::NAL_SUFFIX_SEI:
          root.name = "SEI";
          createSEI(*std::dynamic_pointer_cast<VVC::SEI_NAL>(pNALUnit), root);
          break;
        default:
          root.name = vvcNalTypeName(pNALUnit->m_nalHeader.nal_unit_type);
      }
    }

    std::string out;
    root.toJson(out);
    return out;
  }

  void VvcSyntaxWriter::createPTL(const VVC::ProfileTierLevel &ptl, SyntaxNode &p)
  {
    p.add("general_profile_idc = " + n(ptl.general_profile_idc));
    p.add("general_tier_flag = " + n(ptl.general_tier_flag));
    p.add("general_level_idc = " + n(ptl.general_level_idc));
    p.add("ptl_frame_only_constraint_flag = " + n(ptl.ptl_frame_only_constraint_flag));
    p.add("ptl_multilayer_enabled_flag = " + n(ptl.ptl_multilayer_enabled_flag));
    p.add("ptl_num_sub_profiles = " + n(ptl.ptl_num_sub_profiles));
  }

  void VvcSyntaxWriter::createVPS(const VVC::VPS &v, SyntaxNode &p)
  {
    p.add("vps_video_parameter_set_id = " + n(v.vps_video_parameter_set_id));
    p.add("vps_max_layers_minus1 = " + n(v.vps_max_layers_minus1));
    p.add("vps_max_sublayers_minus1 = " + n(v.vps_max_sublayers_minus1));
    p.add("vps_all_independent_layers_flag = " + n(v.vps_all_independent_layers_flag));
    p.add("vps_num_ptls_minus1 = " + n(v.vps_num_ptls_minus1));
    for(std::size_t i = 0; i < v.ptl.size(); i++)
    {
      SyntaxNode &c = p.add("profile_tier_level(" + n(i) + ")");
      createPTL(v.ptl[i], c);
    }
    p.add("vps_extension_flag = " + n(v.vps_extension_flag));
  }

  void VvcSyntaxWriter::createSPS(const VVC::SPS &s, SyntaxNode &p)
  {
    p.add("sps_seq_parameter_set_id = " + n(s.sps_seq_parameter_set_id));
    p.add("sps_video_parameter_set_id = " + n(s.sps_video_parameter_set_id));
    p.add("sps_max_sublayers_minus1 = " + n(s.sps_max_sublayers_minus1));
    p.add("sps_chroma_format_idc = " + n(s.sps_chroma_format_idc));
    p.add("sps_log2_ctu_size_minus5 = " + n(s.sps_log2_ctu_size_minus5));

    if(s.sps_ptl_dpb_hrd_params_present_flag)
    {
      SyntaxNode &c = p.add("profile_tier_level");
      createPTL(s.profile_tier_level, c);
    }

    p.add("sps_gdr_enabled_flag = " + n(s.sps_gdr_enabled_flag));
    p.add("sps_ref_pic_resampling_enabled_flag = " + n(s.sps_ref_pic_resampling_enabled_flag));
    p.add("sps_pic_width_max_in_luma_samples = " + n(s.sps_pic_width_max_in_luma_samples));
    p.add("sps_pic_height_max_in_luma_samples = " + n(s.sps_pic_height_max_in_luma_samples));
    if(s.sps_conformance_window_flag)
    {
      p.add("sps_conf_win_left_offset = " + n(s.sps_conf_win_left_offset));
      p.add("sps_conf_win_right_offset = " + n(s.sps_conf_win_right_offset));
      p.add("sps_conf_win_top_offset = " + n(s.sps_conf_win_top_offset));
      p.add("sps_conf_win_bottom_offset = " + n(s.sps_conf_win_bottom_offset));
    }
    p.add("sps_bitdepth_minus8 = " + n(s.sps_bitdepth_minus8));
    p.add("sps_log2_max_pic_order_cnt_lsb_minus4 = " + n(s.sps_log2_max_pic_order_cnt_lsb_minus4));
    p.add("sps_poc_msb_cycle_flag = " + n(s.sps_poc_msb_cycle_flag));
    p.add("sps_num_extra_ph_bytes = " + n(s.sps_num_extra_ph_bytes));
    p.add("sps_num_extra_sh_bytes = " + n(s.sps_num_extra_sh_bytes));
    p.add("sps_ccalf_enabled_flag = " + n(s.sps_ccalf_enabled_flag));
    p.add("sps_same_qp_table_for_chroma_flag = " + n(s.sps_same_qp_table_for_chroma_flag));
    p.add("sps_idr_rpl_present_flag = " + n(s.sps_idr_rpl_present_flag));
    p.add("sps_rpl1_same_as_rpl0_flag = " + n(s.sps_rpl1_same_as_rpl0_flag));
    p.add("sps_log2_min_luma_coding_block_size_minus2 = " + n(s.sps_log2_min_luma_coding_block_size_minus2));
    p.add("sps_partition_constraints_override_enabled_flag = " + n(s.sps_partition_constraints_override_enabled_flag));
    p.add("sps_virtual_boundaries_enabled_flag = " + n(s.sps_virtual_boundaries_enabled_flag));
    p.add("sps_virtual_boundaries_present_flag = " + n(s.sps_virtual_boundaries_present_flag));
    p.add("sps_max_luma_transform_size_64_flag = " + n(s.sps_max_luma_transform_size_64_flag));
    p.add("sps_transform_skip_enabled_flag = " + n(s.sps_transform_skip_enabled_flag));
    p.add("sps_mts_enabled_flag = " + n(s.sps_mts_enabled_flag));
    p.add("sps_lfnst_enabled_flag = " + n(s.sps_lfnst_enabled_flag));
    p.add("sps_joint_cbcr_enabled_flag = " + n(s.sps_joint_cbcr_enabled_flag));
    p.add("sps_sao_enabled_flag = " + n(s.sps_sao_enabled_flag));
    p.add("sps_alf_enabled_flag = " + n(s.sps_alf_enabled_flag));
    p.add("sps_lmcs_enabled_flag = " + n(s.sps_lmcs_enabled_flag));
    p.add("sps_weighted_pred_flag = " + n(s.sps_weighted_pred_flag));
    p.add("sps_weighted_bipred_flag = " + n(s.sps_weighted_bipred_flag));
    p.add("sps_long_term_ref_pics_flag = " + n(s.sps_long_term_ref_pics_flag));
    p.add("sps_num_ref_pic_lists[0] = " + n(s.sps_num_ref_pic_lists[0]));
    p.add("sps_num_ref_pic_lists[1] = " + n(s.sps_num_ref_pic_lists[1]));
    p.add("sps_temporal_mvp_enabled_flag = " + n(s.sps_temporal_mvp_enabled_flag));
    p.add("sps_bdof_enabled_flag = " + n(s.sps_bdof_enabled_flag));
    p.add("sps_dmvr_enabled_flag = " + n(s.sps_dmvr_enabled_flag));
    p.add("sps_mmvd_enabled_flag = " + n(s.sps_mmvd_enabled_flag));
    p.add("sps_affine_enabled_flag = " + n(s.sps_affine_enabled_flag));
    p.add("sps_bcw_enabled_flag = " + n(s.sps_bcw_enabled_flag));
    p.add("sps_ciip_enabled_flag = " + n(s.sps_ciip_enabled_flag));
    p.add("sps_gpm_enabled_flag = " + n(s.sps_gpm_enabled_flag));
    p.add("sps_ibc_enabled_flag = " + n(s.sps_ibc_enabled_flag));
    p.add("sps_explicit_scaling_list_enabled_flag = " + n(s.sps_explicit_scaling_list_enabled_flag));
    p.add("sps_dep_quant_enabled_flag = " + n(s.sps_dep_quant_enabled_flag));
    p.add("sps_sign_data_hiding_enabled_flag = " + n(s.sps_sign_data_hiding_enabled_flag));
    p.add("sps_field_seq_flag = " + n(s.sps_field_seq_flag));
    p.add("sps_vui_parameters_present_flag = " + n(s.sps_vui_parameters_present_flag));
    p.add("sps_extension_flag = " + n(s.sps_extension_flag));
  }

  void VvcSyntaxWriter::createPPS(const VVC::PPS &pp, SyntaxNode &p)
  {
    p.add("pps_pic_parameter_set_id = " + n(pp.pps_pic_parameter_set_id));
    p.add("pps_seq_parameter_set_id = " + n(pp.pps_seq_parameter_set_id));
    p.add("pps_mixed_nalu_types_in_pic_flag = " + n(pp.pps_mixed_nalu_types_in_pic_flag));
    p.add("pps_pic_width_in_luma_samples = " + n(pp.pps_pic_width_in_luma_samples));
    p.add("pps_pic_height_in_luma_samples = " + n(pp.pps_pic_height_in_luma_samples));
    p.add("pps_no_pic_partition_flag = " + n(pp.pps_no_pic_partition_flag));
    if(!pp.pps_no_pic_partition_flag)
    {
      p.add("pps_num_exp_tile_columns_minus1 = " + n(pp.pps_num_exp_tile_columns_minus1));
      p.add("pps_num_exp_tile_rows_minus1 = " + n(pp.pps_num_exp_tile_rows_minus1));
      p.add("pps_rect_slice_flag = " + n(pp.pps_rect_slice_flag));
      p.add("pps_single_slice_per_subpic_flag = " + n(pp.pps_single_slice_per_subpic_flag));
      p.add("pps_num_slices_in_pic_minus1 = " + n(pp.pps_num_slices_in_pic_minus1));
    }
    p.add("pps_cabac_init_present_flag = " + n(pp.pps_cabac_init_present_flag));
    p.add("pps_num_ref_idx_default_active_minus1[0] = " + n(pp.pps_num_ref_idx_default_active_minus1[0]));
    p.add("pps_num_ref_idx_default_active_minus1[1] = " + n(pp.pps_num_ref_idx_default_active_minus1[1]));
    p.add("pps_init_qp_minus26 = " + n(pp.pps_init_qp_minus26));
    p.add("pps_cu_qp_delta_enabled_flag = " + n(pp.pps_cu_qp_delta_enabled_flag));
    p.add("pps_chroma_tool_offsets_present_flag = " + n(pp.pps_chroma_tool_offsets_present_flag));
    p.add("pps_cb_qp_offset = " + n(pp.pps_cb_qp_offset));
    p.add("pps_cr_qp_offset = " + n(pp.pps_cr_qp_offset));
    p.add("pps_deblocking_filter_control_present_flag = " + n(pp.pps_deblocking_filter_control_present_flag));
    p.add("pps_rpl_info_in_ph_flag = " + n(pp.pps_rpl_info_in_ph_flag));
    p.add("pps_dbf_info_in_ph_flag = " + n(pp.pps_dbf_info_in_ph_flag));
    p.add("pps_sao_info_in_ph_flag = " + n(pp.pps_sao_info_in_ph_flag));
    p.add("pps_alf_info_in_ph_flag = " + n(pp.pps_alf_info_in_ph_flag));
    p.add("pps_wp_info_in_ph_flag = " + n(pp.pps_wp_info_in_ph_flag));
    p.add("pps_qp_delta_info_in_ph_flag = " + n(pp.pps_qp_delta_info_in_ph_flag));
    p.add("pps_output_flag_present_flag = " + n(pp.pps_output_flag_present_flag));
    p.add("pps_extension_flag = " + n(pp.pps_extension_flag));
  }

  void VvcSyntaxWriter::createPH(const VVC::PH &ph, SyntaxNode &p)
  {
    p.add("ph_gdr_or_irap_pic_flag = " + n(ph.ph_gdr_or_irap_pic_flag));
    p.add("ph_non_ref_pic_flag = " + n(ph.ph_non_ref_pic_flag));
    p.add("ph_inter_slice_allowed_flag = " + n(ph.ph_inter_slice_allowed_flag));
    p.add("ph_intra_slice_allowed_flag = " + n(ph.ph_intra_slice_allowed_flag));
    p.add("ph_pic_parameter_set_id = " + n(ph.ph_pic_parameter_set_id));
    p.add("ph_pic_order_cnt_lsb = " + n(ph.ph_pic_order_cnt_lsb));
    if(ph.ph_gdr_or_irap_pic_flag)
      p.add("ph_recovery_poc_cnt = " + n(ph.ph_recovery_poc_cnt));
  }

  void VvcSyntaxWriter::createSlice(const VVC::Slice &s, SyntaxNode &p)
  {
    p.add("picture_header_in_slice_header_flag = " + n(s.picture_header_in_slice_header_flag));
    p.add("slice_address = " + n(s.slice_address));
    p.add("slice_type = " + n(s.slice_type));
    p.add("slice_pic_parameter_set_id = " + n(s.slice_pic_parameter_set_id));
    p.add("slice_pic_order_cnt_lsb = " + n(s.slice_pic_order_cnt_lsb));
    p.add("slice_qp_delta = " + n(s.slice_qp_delta));
  }

  void VvcSyntaxWriter::createAUD(const VVC::AUD &a, SyntaxNode &p)
  {
    p.add("aud_irap_or_gdr_flag = " + n(a.aud_irap_or_gdr_flag));
    p.add("aud_pic_type = " + n(a.aud_pic_type));
  }

  void VvcSyntaxWriter::createSEI(const VVC::SEI_NAL &sei, SyntaxNode &p)
  {
    for(std::size_t i = 0; i < sei.messages.size(); i++)
    {
      const VVC::SeiMessage &m = sei.messages[i];
      SyntaxNode &c = p.add("sei_message(" + n(i) + ")");
      c.add("payload_type = " + n(m.payload_type));
      c.add("payload_size = " + n(m.payload_size));
    }
  }

}
