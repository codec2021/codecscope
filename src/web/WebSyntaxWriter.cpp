#include "WebSyntaxWriter.h"

#include <HevcUtils.h>

#include <algorithm>
#include <sstream>

namespace web
{

  namespace
  {
    std::string n(uint32_t v) { return std::to_string(v); }
    std::string n(int32_t v)  { return std::to_string(v); }
    std::string n(uint8_t v)  { return std::to_string((int)v); }
    std::string n(uint16_t v) { return std::to_string(v); }
    std::string n(std::size_t v) { return std::to_string(v); }

    std::string hex2Upper(uint8_t v)
    {
      static const char *h = "0123456789ABCDEF";
      std::string r = "00";
      r[0] = h[(v >> 4) & 0xF];
      r[1] = h[v & 0xF];
      return r;
    }

    std::string hex2Lower(uint8_t v)
    {
      static const char *h = "0123456789abcdef";
      std::string r = "00";
      r[0] = h[(v >> 4) & 0xF];
      r[1] = h[v & 0xF];
      return r;
    }

    std::string uuidFormat(const uint8_t *p)
    {
      std::string r;
      for(int i = 0; i < 16; i++)
      {
        if(i == 4 || i == 6 || i == 8 || i == 10)
          r += "-";
        r += hex2Lower(p[i]);
      }
      return r;
    }
  }

  #define SLICE_B 0
  #define SLICE_P 1
  #define SLICE_I 2

  void SyntaxWriter::setParameterSets(const std::map<uint32_t, std::shared_ptr<HEVC::VPS> > &vpsMap,
                                      const std::map<uint32_t, std::shared_ptr<HEVC::SPS> > &spsMap,
                                      const std::map<uint32_t, std::shared_ptr<HEVC::PPS> > &ppsMap)
  {
    m_vpsMap = vpsMap;
    m_spsMap = spsMap;
    m_ppsMap = ppsMap;
  }

  std::string SyntaxWriter::write(std::shared_ptr<HEVC::NALUnit> pNALUnit)
  {
    SyntaxNode root;
    if(!pNALUnit)
    {
      root.name = "NULL";
    }
    else
    {
      switch(pNALUnit -> m_nalHeader.type)
      {
        case HEVC::NAL_VPS:
          root.name = "VPS";
          createVPS(std::dynamic_pointer_cast<HEVC::VPS>(pNALUnit), root);
          break;
        case HEVC::NAL_SPS:
          root.name = "SPS";
          createSPS(std::dynamic_pointer_cast<HEVC::SPS>(pNALUnit), root);
          break;
        case HEVC::NAL_PPS:
          root.name = "PPS";
          createPPS(std::dynamic_pointer_cast<HEVC::PPS>(pNALUnit), root);
          break;
        case HEVC::NAL_TRAIL_R:
        case HEVC::NAL_TRAIL_N:
        case HEVC::NAL_TSA_N:
        case HEVC::NAL_TSA_R:
        case HEVC::NAL_STSA_N:
        case HEVC::NAL_STSA_R:
        case HEVC::NAL_BLA_W_LP:
        case HEVC::NAL_BLA_W_RADL:
        case HEVC::NAL_BLA_N_LP:
        case HEVC::NAL_IDR_W_RADL:
        case HEVC::NAL_IDR_N_LP:
        case HEVC::NAL_CRA_NUT:
        case HEVC::NAL_RADL_N:
        case HEVC::NAL_RADL_R:
        case HEVC::NAL_RASL_N:
        case HEVC::NAL_RASL_R:
          root.name = "Slice Syntax";
          createSlice(std::dynamic_pointer_cast<HEVC::Slice>(pNALUnit), root);
          break;
        case HEVC::NAL_AUD:
          root.name = "AUD";
          createAUD(std::dynamic_pointer_cast<HEVC::AUD>(pNALUnit), root);
          break;
        case HEVC::NAL_SEI_PREFIX:
        case HEVC::NAL_SEI_SUFFIX:
          root.name = "SEI";
          createSEI(std::dynamic_pointer_cast<HEVC::SEI>(pNALUnit), root);
          break;
        default:
          root.name = "Unknown";
          break;
      }
    }

    std::string out;
    root.toJson(out);
    return out;
  }

  void SyntaxWriter::createVPS(std::shared_ptr<HEVC::VPS> pVPS, SyntaxNode &parent)
  {
    parent.add("vps_video_parameter_set_id = " + n(pVPS -> vps_video_parameter_set_id));
    parent.add("vps_max_layers_minus1 = " + n(pVPS -> vps_max_layers_minus1));
    parent.add("vps_max_sub_layers_minus1 = " + n(pVPS -> vps_max_sub_layers_minus1));
    parent.add("vps_temporal_id_nesting_flag = " + n(pVPS -> vps_temporal_id_nesting_flag));

    SyntaxNode &ptl = parent.add("profile_tier_level");
    createProfileTierLevel(pVPS -> profile_tier_level, ptl);

    parent.add("vps_sub_layer_ordering_info_present_flag = " + n(pVPS -> vps_sub_layer_ordering_info_present_flag));

    SyntaxNode &ploop = parent.add("for( i = ( vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1 ); i <= vps_max_sub_layers_minus1; i++ )");
    for(std::size_t i = (pVPS -> vps_sub_layer_ordering_info_present_flag ? 0 : pVPS -> vps_max_sub_layers_minus1); i <= pVPS -> vps_max_sub_layers_minus1; i++)
    {
      ploop.add("vps_max_dec_pic_buffering_minus1[" + n(i) + "] = " + n(pVPS -> vps_max_dec_pic_buffering_minus1[i]));
      ploop.add("vps_max_num_reorder_pics[" + n(i) + "] = " + n(pVPS -> vps_max_num_reorder_pics[i]));
      ploop.add("vps_max_latency_increase_plus1[" + n(i) + "] = " + n(pVPS -> vps_max_latency_increase_plus1[i]));
    }

    parent.add("vps_max_layer_id = " + n(pVPS -> vps_max_layer_id));
    parent.add("vps_num_layer_sets_minus1 = " + n(pVPS -> vps_num_layer_sets_minus1));

    if(pVPS -> vps_num_layer_sets_minus1 == 0)
    {
      parent.add("layer_id_included_flag = { }");
    }
    else
    {
      SyntaxNode &ploop2 = parent.add("for( i = 0; i <= vps_num_layer_sets_minus1; i++ )");
      for(std::size_t i = 0; i <= pVPS -> vps_num_layer_sets_minus1; i++)
      {
        std::string str;
        if(pVPS -> vps_max_layer_id == 0)
          str = "layer_id_included_flag[" + n(i) + "] = { } ";
        else
        {
          str = "layer_id_included_flag[" + n(i) + "] = { ";
          for(std::size_t j = 0; j <= pVPS -> vps_max_layer_id; j++)
          {
            if(j)
              str += ", ";
            str += n(pVPS -> layer_id_included_flag[i][j]);
          }
          str += " }";
        }
        ploop2.add(str);
      }
    }

    parent.add("vps_timing_info_present_flag = " + n(pVPS -> vps_timing_info_present_flag));

    if(pVPS -> vps_timing_info_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( vps_timing_info_present_flag )");
      pitem.add("vps_num_units_in_tick = " + n(pVPS -> vps_num_units_in_tick));
      pitem.add("vps_time_scale = " + n(pVPS -> vps_time_scale));
      pitem.add("vps_poc_proportional_to_timing_flag = " + n(pVPS -> vps_poc_proportional_to_timing_flag));

      if(pVPS -> vps_poc_proportional_to_timing_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( vps_poc_proportional_to_timing_flag )");
        pitemSecond.add("vps_num_ticks_poc_diff_one_minus1 = " + n(pVPS -> vps_num_ticks_poc_diff_one_minus1));
      }

      pitem.add("vps_num_hrd_parameters = " + n(pVPS -> vps_num_hrd_parameters));

      if(pVPS -> vps_num_hrd_parameters)
      {
        SyntaxNode &ploop2 = pitem.add("for( i = 0; i < vps_num_hrd_parameters; i++ )");
        for(std::size_t i = 0; i < pVPS -> vps_num_hrd_parameters; i++)
        {
          ploop2.add("hrd_layer_set_idx[" + n(i) + "] = " + n(pVPS -> hrd_layer_set_idx[i]));
          if(i > 0)
          {
            SyntaxNode &pitemSecond = ploop2.add("if( i > 0 )");
            pitemSecond.add("cprms_present_flag[" + n(i) + "] = " + n(pVPS -> cprms_present_flag[i]));
          }

          SyntaxNode &pitemThird = ploop2.add("hrd_parameters(" + n(i) + ", " + n(pVPS -> vps_max_sub_layers_minus1) + ")");
          createHrdParameters(pVPS -> hrd_parameters[i], pVPS -> cprms_present_flag[i], pitemThird);
        }
      }
    }

    parent.add("vps_extension_flag = " + n(pVPS -> vps_extension_flag));
  }

  void SyntaxWriter::createSPS(std::shared_ptr<HEVC::SPS> pSPS, SyntaxNode &parent)
  {
    parent.add("sps_video_parameter_set_id = " + n(pSPS -> sps_video_parameter_set_id));
    parent.add("sps_max_sub_layers_minus1 = " + n(pSPS -> sps_max_sub_layers_minus1));
    parent.add("sps_temporal_id_nesting_flag = " + n(pSPS -> sps_temporal_id_nesting_flag));

    SyntaxNode &ptl = parent.add("profile_tier_level");
    createProfileTierLevel(pSPS -> profile_tier_level, ptl);

    parent.add("sps_seq_parameter_set_id = " + n(pSPS -> sps_seq_parameter_set_id));
    parent.add("chroma_format_idc = " + n(pSPS -> chroma_format_idc));

    if(pSPS -> chroma_format_idc == 3)
    {
      SyntaxNode &pitem = parent.add("if( chroma_format_idc == 3 )");
      pitem.add("separate_colour_plane_flag = " + n(pSPS -> separate_colour_plane_flag));
    }

    parent.add("pic_width_in_luma_samples = " + n(pSPS -> pic_width_in_luma_samples));
    parent.add("pic_height_in_luma_samples = " + n(pSPS -> pic_height_in_luma_samples));
    parent.add("conformance_window_flag = " + n(pSPS -> conformance_window_flag));

    if(pSPS -> conformance_window_flag)
    {
      SyntaxNode &pitem = parent.add("if( conformance_window_flag )");
      pitem.add("conf_win_left_offset = " + n(pSPS -> conf_win_left_offset));
      pitem.add("conf_win_right_offset = " + n(pSPS -> conf_win_right_offset));
      pitem.add("conf_win_top_offset = " + n(pSPS -> conf_win_top_offset));
      pitem.add("conf_win_bottom_offset = " + n(pSPS -> conf_win_bottom_offset));
    }

    parent.add("bit_depth_luma_minus8 = " + n(pSPS -> bit_depth_luma_minus8));
    parent.add("bit_depth_chroma_minus8 = " + n(pSPS -> bit_depth_chroma_minus8));
    parent.add("log2_max_pic_order_cnt_lsb_minus4 = " + n(pSPS -> log2_max_pic_order_cnt_lsb_minus4));
    parent.add("sps_sub_layer_ordering_info_present_flag = " + n(pSPS -> sps_sub_layer_ordering_info_present_flag));

    SyntaxNode &ploop = parent.add("for( i = ( sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1 ); i <= sps_max_sub_layers_minus1; i++ )");
    for(std::size_t i = (pSPS -> sps_sub_layer_ordering_info_present_flag ? 0 : pSPS -> sps_max_sub_layers_minus1); i <= pSPS -> sps_max_sub_layers_minus1; i++)
    {
      ploop.add("sps_max_dec_pic_buffering_minus1[" + n(i) + "] = " + n(pSPS -> sps_max_dec_pic_buffering_minus1[i]));
      ploop.add("sps_max_num_reorder_pics[" + n(i) + "] = " + n(pSPS -> sps_max_num_reorder_pics[i]));
      ploop.add("sps_max_latency_increase_plus1[" + n(i) + "] = " + n(pSPS -> sps_max_latency_increase_plus1[i]));
    }

    parent.add("log2_min_luma_coding_block_size_minus3 = " + n(pSPS -> log2_min_luma_coding_block_size_minus3));
    parent.add("log2_diff_max_min_luma_coding_block_size = " + n(pSPS -> log2_diff_max_min_luma_coding_block_size));
    parent.add("log2_min_transform_block_size_minus2 = " + n(pSPS -> log2_min_transform_block_size_minus2));
    parent.add("log2_diff_max_min_transform_block_size = " + n(pSPS -> log2_diff_max_min_transform_block_size));
    parent.add("max_transform_hierarchy_depth_inter = " + n(pSPS -> max_transform_hierarchy_depth_inter));
    parent.add("max_transform_hierarchy_depth_intra = " + n(pSPS -> max_transform_hierarchy_depth_intra));
    parent.add("scaling_list_enabled_flag = " + n(pSPS -> scaling_list_enabled_flag));

    if(pSPS -> scaling_list_enabled_flag)
    {
      SyntaxNode &pitem = parent.add("if( scaling_list_enabled_flag )");
      pitem.add("sps_scaling_list_data_present_flag = " + n(pSPS -> sps_scaling_list_data_present_flag));

      if(pSPS -> sps_scaling_list_data_present_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("scaling_list_data( )");
        createScalingListData(pSPS -> scaling_list_data, pitemSecond);
      }
    }

    parent.add("amp_enabled_flag = " + n(pSPS -> amp_enabled_flag));
    parent.add("sample_adaptive_offset_enabled_flag = " + n(pSPS -> sample_adaptive_offset_enabled_flag));
    parent.add("pcm_enabled_flag = " + n(pSPS -> pcm_enabled_flag));

    if(pSPS -> pcm_enabled_flag)
    {
      SyntaxNode &pitem = parent.add("if( pcm_enabled_flag )");
      pitem.add("pcm_sample_bit_depth_luma_minus1 = " + n(pSPS -> pcm_sample_bit_depth_luma_minus1));
      pitem.add("pcm_sample_bit_depth_chroma_minus1 = " + n(pSPS -> pcm_sample_bit_depth_chroma_minus1));
      pitem.add("log2_min_pcm_luma_coding_block_size_minus3 = " + n(pSPS -> log2_min_pcm_luma_coding_block_size_minus3));
      pitem.add("log2_diff_max_min_pcm_luma_coding_block_size = " + n(pSPS -> log2_diff_max_min_pcm_luma_coding_block_size));
      pitem.add("pcm_loop_filter_disabled_flag = " + n(pSPS -> pcm_loop_filter_disabled_flag));
    }

    parent.add("num_short_term_ref_pic_sets = " + n(pSPS -> num_short_term_ref_pic_sets));

    if(pSPS -> num_short_term_ref_pic_sets)
    {
      SyntaxNode &pitem = parent.add("for( i = 0; i < num_short_term_ref_pic_sets; i++ )");
      for(std::size_t i = 0; i < pSPS -> num_short_term_ref_pic_sets; i++)
      {
        SyntaxNode &pStrpc = pitem.add("short_term_ref_pic_set(" + n(i) + ")");
        createShortTermRefPicSet(i, pSPS -> short_term_ref_pic_set[i], pSPS -> num_short_term_ref_pic_sets, pSPS -> short_term_ref_pic_set, pStrpc);
      }
    }

    parent.add("long_term_ref_pics_present_flag = " + n(pSPS -> long_term_ref_pics_present_flag));
    if(pSPS -> long_term_ref_pics_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( long_term_ref_pics_present_flag )");
      pitem.add("num_long_term_ref_pics_sps = " + n(pSPS -> num_long_term_ref_pics_sps));

      if(pSPS -> num_long_term_ref_pics_sps > 0)
      {
        SyntaxNode &pitemLoop = pitem.add("for( i = 0; i < num_long_term_ref_pics_sps; i++ )");
        for(std::size_t i = 0; i < pSPS -> num_long_term_ref_pics_sps; i++)
        {
          pitemLoop.add("lt_ref_pic_poc_lsb_sps[" + n(i) + "] = " + n(pSPS -> lt_ref_pic_poc_lsb_sps[i]));
          pitemLoop.add("used_by_curr_pic_lt_sps_flag[" + n(i) + "] = " + n(pSPS -> used_by_curr_pic_lt_sps_flag[i]));
        }
      }
    }

    parent.add("sps_temporal_mvp_enabled_flag = " + n(pSPS -> sps_temporal_mvp_enabled_flag));
    parent.add("strong_intra_smoothing_enabled_flag = " + n(pSPS -> strong_intra_smoothing_enabled_flag));
    parent.add("vui_parameters_present_flag = " + n(pSPS -> vui_parameters_present_flag));

    if(pSPS -> vui_parameters_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( vui_parameters_present_flag )");
      SyntaxNode &pVuiItem = pitem.add("vui_parameters");
      createVuiParameters(pSPS -> vui_parameters, pSPS -> sps_max_sub_layers_minus1, pVuiItem);
    }

    parent.add("sps_extension_flag = " + n(pSPS -> sps_extension_flag));
  }

  void SyntaxWriter::createPPS(std::shared_ptr<HEVC::PPS> pPPS, SyntaxNode &parent)
  {
    parent.add("pps_pic_parameter_set_id = " + n(pPPS -> pps_pic_parameter_set_id));
    parent.add("pps_seq_parameter_set_id = " + n(pPPS -> pps_seq_parameter_set_id));
    parent.add("dependent_slice_segments_enabled_flag = " + n(pPPS -> dependent_slice_segments_enabled_flag));
    parent.add("output_flag_present_flag = " + n(pPPS -> output_flag_present_flag));
    parent.add("num_extra_slice_header_bits = " + n(pPPS -> num_extra_slice_header_bits));
    parent.add("sign_data_hiding_flag = " + n(pPPS -> sign_data_hiding_flag));
    parent.add("cabac_init_present_flag = " + n(pPPS -> cabac_init_present_flag));
    parent.add("num_ref_idx_l0_default_active_minus1 = " + n(pPPS -> num_ref_idx_l0_default_active_minus1));
    parent.add("num_ref_idx_l1_default_active_minus1 = " + n(pPPS -> num_ref_idx_l1_default_active_minus1));
    parent.add("init_qp_minus26 = " + n(pPPS -> init_qp_minus26));
    parent.add("constrained_intra_pred_flag = " + n(pPPS -> constrained_intra_pred_flag));
    parent.add("transform_skip_enabled_flag = " + n(pPPS -> transform_skip_enabled_flag));
    parent.add("cu_qp_delta_enabled_flag = " + n(pPPS -> cu_qp_delta_enabled_flag));

    if(pPPS -> cu_qp_delta_enabled_flag)
    {
      SyntaxNode &pitem = parent.add("if( cu_qp_delta_enabled_flag )");
      pitem.add("diff_cu_qp_delta_depth = " + n(pPPS -> diff_cu_qp_delta_depth));
    }

    parent.add("pps_cb_qp_offset = " + n(pPPS -> pps_cb_qp_offset));
    parent.add("pps_cr_qp_offset = " + n(pPPS -> pps_cr_qp_offset));
    parent.add("pps_slice_chroma_qp_offsets_present_flag = " + n(pPPS -> pps_slice_chroma_qp_offsets_present_flag));
    parent.add("weighted_pred_flag = " + n(pPPS -> weighted_pred_flag));
    parent.add("weighted_bipred_flag = " + n(pPPS -> weighted_bipred_flag));
    parent.add("transquant_bypass_enabled_flag = " + n(pPPS -> transquant_bypass_enabled_flag));
    parent.add("tiles_enabled_flag = " + n(pPPS -> tiles_enabled_flag));
    parent.add("entropy_coding_sync_enabled_flag = " + n(pPPS -> entropy_coding_sync_enabled_flag));

    if(pPPS -> tiles_enabled_flag)
    {
      SyntaxNode &pitem = parent.add("if( tiles_enabled_flag )");
      pitem.add("num_tile_columns_minus1 = " + n(pPPS -> num_tile_columns_minus1));
      pitem.add("num_tile_rows_minus1 = " + n(pPPS -> num_tile_rows_minus1));
      pitem.add("uniform_spacing_flag = " + n(pPPS -> uniform_spacing_flag));

      if(!pPPS -> uniform_spacing_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( !uniform_spacing_flag )");

        std::string str = "column_width_minus1 = { ";
        for(std::size_t i = 0; i < pPPS -> num_tile_columns_minus1; i++)
        {
          if(i)
            str += ", ";
          str += n(pPPS -> column_width_minus1[i]);
        }
        str += " }";
        pitemSecond.add(str);

        str = "row_height_minus1 = { ";
        for(std::size_t i = 0; i < pPPS -> num_tile_rows_minus1; i++)
        {
          if(i)
            str += ", ";
          str += n(pPPS -> row_height_minus1[i]);
        }
        str += " }";
        pitemSecond.add(str);
      }

      pitem.add("loop_filter_across_tiles_enabled_flag = " + n(pPPS -> loop_filter_across_tiles_enabled_flag));
    }

    parent.add("pps_loop_filter_across_slices_enabled_flag = " + n(pPPS -> pps_loop_filter_across_slices_enabled_flag));
    parent.add("deblocking_filter_control_present_flag = " + n(pPPS -> deblocking_filter_control_present_flag));

    if(pPPS -> deblocking_filter_control_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( deblocking_filter_control_present_flag )");
      pitem.add("deblocking_filter_override_enabled_flag = " + n(pPPS -> deblocking_filter_override_enabled_flag));
      pitem.add("pps_deblocking_filter_disabled_flag = " + n(pPPS -> pps_deblocking_filter_disabled_flag));

      if(!pPPS -> pps_deblocking_filter_disabled_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( !pps_deblocking_filter_disabled_flag )");
        pitemSecond.add("pps_beta_offset_div2 = " + n(pPPS -> pps_beta_offset_div2));
        pitemSecond.add("pps_tc_offset_div2 = " + n(pPPS -> pps_tc_offset_div2));
      }
    }

    parent.add("pps_scaling_list_data_present_flag = " + n(pPPS -> pps_scaling_list_data_present_flag));

    if(pPPS -> pps_scaling_list_data_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( pps_scaling_list_data_present_flag )");
      SyntaxNode &pitemSecond = pitem.add("scaling_list_data");
      createScalingListData(pPPS -> scaling_list_data, pitemSecond);
    }

    parent.add("lists_modification_present_flag = " + n(pPPS -> lists_modification_present_flag));
    parent.add("log2_parallel_merge_level_minus2 = " + n(pPPS -> log2_parallel_merge_level_minus2));
    parent.add("slice_segment_header_extension_present_flag = " + n(pPPS -> slice_segment_header_extension_present_flag));
    parent.add("pps_extension_flag = " + n(pPPS -> pps_extension_flag));
  }

  void SyntaxWriter::createSlice(std::shared_ptr<HEVC::Slice> pSlice, SyntaxNode &parent)
  {
    if(pSlice -> m_processFailed)
      return;

    std::shared_ptr<HEVC::PPS> pPPS = m_ppsMap[pSlice -> slice_pic_parameter_set_id];
    if(!pPPS)
      return;

    int32_t spsId = pPPS -> pps_seq_parameter_set_id;

    parent.add("forbidden_zero_bit = 0");
    parent.add("nal_unit_type = " + n(pSlice -> m_nalHeader.type));
    parent.add("nuh_layer_id = " + n(pSlice -> m_nalHeader.layer_id));
    parent.add("nuh_temporal_id_plus1 = " + n(pSlice -> m_nalHeader.temporal_id_plus1));

    parent.add("first_slice_segment_in_pic_flag = " + n(pSlice -> first_slice_segment_in_pic_flag));

    if(pSlice -> m_nalHeader.type >= HEVC::NAL_BLA_W_LP && pSlice -> m_nalHeader.type <= HEVC::NAL_IRAP_VCL23)
    {
      SyntaxNode &pitem = parent.add("if( nal_unit_type >= BLA_W_LP && nal_unit_type <= RSV_IRAP_VCL23 )");
      pitem.add("no_output_of_prior_pics_flag = " + n(pSlice -> no_output_of_prior_pics_flag));
    }

    parent.add("slice_pic_parameter_set_id = " + n(pSlice -> slice_pic_parameter_set_id));

    if(!pSlice -> first_slice_segment_in_pic_flag)
    {
      SyntaxNode &pitem = parent.add("if( !first_slice_segment_in_pic_flag )");

      if(pPPS -> dependent_slice_segments_enabled_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( dependent_slice_segments_enabled_flag )");
        pitemDepend.add("dependent_slice_segment_flag = " + n(pSlice -> dependent_slice_segment_flag));
      }
      pitem.add("slice_segment_address = " + n(pSlice -> slice_segment_address));
    }

    if(!pSlice -> dependent_slice_segment_flag)
    {
      SyntaxNode &pitem = parent.add("if( !dependent_slice_segment_flag )");

      std::size_t num_extra_slice_header_bits = pPPS -> num_extra_slice_header_bits;

      std::string str;
      if(num_extra_slice_header_bits > 0)
      {
        if(num_extra_slice_header_bits > pSlice -> slice_reserved_undetermined_flag.size())
          return;
        str = "slice_reserved_undetermined_flag = { ";
        for(std::size_t i = 0; i < num_extra_slice_header_bits; i++)
        {
          if(i)
            str += ", ";
          str += n(pSlice -> slice_reserved_undetermined_flag[i]);
        }
        str += " }";
      }
      else
        str = "slice_reserved_undetermined_flag = { }";

      pitem.add(str);
      pitem.add("slice_type = " + n(pSlice -> slice_type));

      if(pPPS -> output_flag_present_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( output_flag_present_flag )");
        pitemDepend.add("pic_output_flag = " + n(pSlice -> pic_output_flag));
      }

      if(m_spsMap.find(spsId) == m_spsMap.end() || !m_spsMap[spsId])
        return;

      if(m_spsMap[spsId] -> separate_colour_plane_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( separate_colour_plane_flag )");
        pitemDepend.add("colour_plane_id = " + n(pSlice -> colour_plane_id));
      }

      bool IdrPicFlag = pSlice -> m_nalHeader.type == HEVC::NAL_IDR_W_RADL || pSlice -> m_nalHeader.type == HEVC::NAL_IDR_N_LP;

      if(!IdrPicFlag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( nal_unit_type != IDR_W_RADL(19) && nal_unit_type != IDR_N_LP(20) )");
        pitemDepend.add("slice_pic_order_cnt_lsb = " + n(pSlice -> slice_pic_order_cnt_lsb));

        pitemDepend.add("short_term_ref_pic_set_sps_flag = " + n(pSlice -> short_term_ref_pic_set_sps_flag));

        if(!pSlice -> short_term_ref_pic_set_sps_flag)
        {
          SyntaxNode &pIf = pitemDepend.add("if( !short_term_ref_pic_set_sps_flag )");
          SyntaxNode &pStrpc = pIf.add("short_term_ref_pic_set(" + n(m_spsMap[spsId] -> num_short_term_ref_pic_sets) + ")");
          createShortTermRefPicSet(m_spsMap[spsId] -> num_short_term_ref_pic_sets, pSlice -> short_term_ref_pic_set, m_spsMap[spsId] -> num_short_term_ref_pic_sets, m_spsMap[spsId] -> short_term_ref_pic_set, pStrpc);
        }
        else if(m_spsMap[spsId] -> num_short_term_ref_pic_sets > 1)
        {
          SyntaxNode &pIf = pitemDepend.add("if( short_term_ref_pic_set_sps_flag && num_short_term_ref_pic_sets > 1 )");
          pIf.add("short_term_ref_pic_set_idx = " + n(pSlice -> short_term_ref_pic_set_idx));
        }

        if(m_spsMap[spsId] -> long_term_ref_pics_present_flag)
        {
          SyntaxNode &pitemThird2 = pitemDepend.add("if( long_term_ref_pics_present_flag )");

          if(m_spsMap[spsId] -> num_long_term_ref_pics_sps > 0)
          {
            SyntaxNode &pitemSecond = pitemThird2.add("if( num_long_term_ref_pics_sps )");
            pitemSecond.add("num_long_term_sps = " + n(pSlice -> num_long_term_sps));
          }

          pitemThird2.add("num_long_term_pics = " + n(pSlice -> num_long_term_pics));

          std::size_t num_long_term = pSlice -> num_long_term_sps + pSlice -> num_long_term_pics;
          SyntaxNode &pitemLoop = pitemThird2.add("for( i = 0; i < num_long_term_sps + num_long_term_pics; i++ )");

          for(std::size_t i = 0; i < num_long_term; i++)
          {
            if(i < pSlice -> num_long_term_sps)
            {
              SyntaxNode &pitem1 = pitemLoop.add("if( i < num_long_term_sps )");
              if(m_spsMap[spsId] -> num_long_term_ref_pics_sps > 1)
              {
                SyntaxNode &pitem2 = pitem1.add("if( num_long_term_ref_pics_sps > 1 )");
                pitem2.add("lt_idx_sps[" + n(i) + "] = " + n(pSlice -> lt_idx_sps[i]));
              }
            }
            else
            {
              SyntaxNode &pitem1 = pitemLoop.add("if( i >= num_long_term_sps )");
              pitem1.add("poc_lsb_lt[" + n(i) + "] = " + n(pSlice -> poc_lsb_lt[i]));
              pitem1.add("used_by_curr_pic_lt_flag[" + n(i) + "] = " + n(pSlice -> used_by_curr_pic_lt_flag[i]));
            }

            pitemLoop.add("delta_poc_msb_present_flag[" + n(i) + "] = " + n(pSlice -> delta_poc_msb_present_flag[i]));
            if(pSlice -> delta_poc_msb_present_flag[i])
            {
              SyntaxNode &pitem1 = pitemLoop.add("if( delta_poc_msb_present_flag[" + n(i) + "] )");
              pitem1.add("delta_poc_msb_cycle_lt[" + n(i) + "] = " + n(pSlice -> delta_poc_msb_cycle_lt[i]));
            }
          }
        }

        if(m_spsMap[spsId] -> sps_temporal_mvp_enabled_flag)
        {
          SyntaxNode &pitemSecond = pitemDepend.add("if( sps_temporal_mvp_enabled_flag )");
          pitemSecond.add("slice_temporal_mvp_enabled_flag = " + n(pSlice -> slice_temporal_mvp_enabled_flag));
        }
      }

      if(m_spsMap[spsId] -> sample_adaptive_offset_enabled_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( sample_adaptive_offset_enabled_flag )");
        pitemDepend.add("slice_sao_luma_flag = " + n(pSlice -> slice_sao_luma_flag));
        pitemDepend.add("slice_sao_chroma_flag = " + n(pSlice -> slice_sao_chroma_flag));
      }

      if(pSlice -> slice_type == SLICE_B || pSlice -> slice_type == SLICE_P)
      {
        SyntaxNode &pitemDepend = pitem.add("if( slice_type == P(1) || slice_type == B(0) )");
        pitemDepend.add("num_ref_idx_active_override_flag = " + n(pSlice -> num_ref_idx_active_override_flag));

        if(pSlice -> num_ref_idx_active_override_flag)
        {
          SyntaxNode &pitemSecond = pitemDepend.add("if( num_ref_idx_active_override_flag )");
          pitemSecond.add("num_ref_idx_l0_active_minus1 = " + n(pSlice -> num_ref_idx_l0_active_minus1));

          if(pSlice -> slice_type == SLICE_B)
          {
            SyntaxNode &pitemThird = pitemSecond.add("if( slice_type == B )");
            pitemThird.add("num_ref_idx_l1_active_minus1 = " + n(pSlice -> num_ref_idx_l1_active_minus1));
          }
        }

        if(pPPS -> lists_modification_present_flag)
        {
          std::size_t NumPocTotalCurr = HEVC::calcNumPocTotalCurr(pSlice, m_spsMap[spsId]);
          if(NumPocTotalCurr > 1)
          {
            SyntaxNode &pitemThird = pitemDepend.add("if( lists_modification_present_flag && NumPocTotalCurr > 1 )");
            SyntaxNode &pitemRplMod = pitemThird.add("ref_pic_lists_modification");
            createRefPicListModification(pSlice -> ref_pic_lists_modification, pitemRplMod);
          }
        }

        if(pSlice -> slice_type == SLICE_B)
        {
          SyntaxNode &pitemThird = pitemDepend.add("if( slice_type == B(0) )");
          pitemThird.add("mvd_l1_zero_flag = " + n(pSlice -> mvd_l1_zero_flag));
        }

        if(pPPS -> cabac_init_present_flag)
        {
          SyntaxNode &pitemThird = pitemDepend.add("if( cabac_init_present_flag )");
          pitemThird.add("cabac_init_flag = " + n(pSlice -> cabac_init_flag));
        }

        if(pSlice -> slice_temporal_mvp_enabled_flag)
        {
          SyntaxNode &pitemThird = pitemDepend.add("if( slice_temporal_mvp_enabled_flag )");

          if(pSlice -> slice_type == SLICE_B)
          {
            SyntaxNode &pitemSecond = pitemThird.add("if( slice_type == B )");
            pitemSecond.add("collocated_from_l0_flag = " + n(pSlice -> collocated_from_l0_flag));
          }

          if((pSlice -> collocated_from_l0_flag && pSlice -> num_ref_idx_l0_active_minus1) ||
             (!pSlice -> collocated_from_l0_flag && pSlice -> num_ref_idx_l1_active_minus1))
          {
            SyntaxNode &pitemSecond = pitemThird.add("if( (collocated_from_l0_flag && num_ref_idx_l0_active_minus1 > 0) || (!collocated_from_l0_flag && num_ref_idx_l1_active_minus1 > 0) )");
            pitemSecond.add("collocated_ref_idx = " + n(pSlice -> collocated_ref_idx));
          }
        }

        if((pPPS -> weighted_pred_flag && pSlice -> slice_type == SLICE_P) ||
           (pPPS -> weighted_bipred_flag && pSlice -> slice_type == SLICE_B))
        {
          SyntaxNode &pitemThird = pitemDepend.add("if( (weighted_pred_flag && slice_type == P) || (weighted_bipred_flag && slice_type == B) )");
          SyntaxNode &pitempwt = pitemThird.add("pred_weight_table");
          createPredWeightTable(pSlice -> pred_weight_table, pSlice, pitempwt);
        }

        pitemDepend.add("five_minus_max_num_merge_cand = " + n(pSlice -> five_minus_max_num_merge_cand));
      }

      pitem.add("slice_qp_delta = " + n(pSlice -> slice_qp_delta));

      if(pPPS -> pps_slice_chroma_qp_offsets_present_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( pps_slice_chroma_qp_offsets_present_flag )");
        pitemDepend.add("slice_cb_qp_offset = " + n(pSlice -> slice_cb_qp_offset));
        pitemDepend.add("slice_cr_qp_offset = " + n(pSlice -> slice_cr_qp_offset));
      }

      if(pPPS -> deblocking_filter_override_enabled_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( deblocking_filter_override_enabled_flag )");
        pitemDepend.add("deblocking_filter_override_flag = " + n(pSlice -> deblocking_filter_override_flag));
      }

      if(pSlice -> deblocking_filter_override_flag)
      {
        SyntaxNode &pitemDepend = pitem.add("if( deblocking_filter_override_flag )");
        pitemDepend.add("slice_deblocking_filter_disabled_flag = " + n(pSlice -> slice_deblocking_filter_disabled_flag));

        if(!pSlice -> slice_deblocking_filter_disabled_flag)
        {
          SyntaxNode &pitemThird = pitemDepend.add("if( !slice_deblocking_filter_disabled_flag )");
          pitemThird.add("slice_beta_offset_div2 = " + n(pSlice -> slice_beta_offset_div2));
          pitemThird.add("slice_tc_offset_div2 = " + n(pSlice -> slice_tc_offset_div2));
        }
      }

      if(pPPS -> pps_loop_filter_across_slices_enabled_flag &&
         (pSlice -> slice_sao_luma_flag || pSlice -> slice_sao_chroma_flag || !pSlice -> slice_deblocking_filter_disabled_flag))
      {
        SyntaxNode &pitemDepend = pitem.add("if( pps_loop_filter_across_slices_enabled_flag && (slice_sao_luma_flag || slice_sao_chroma_flag || !slice_deblocking_filter_disabled_flag) )");
        pitemDepend.add("slice_loop_filter_across_slices_enabled_flag = " + n(pSlice -> slice_loop_filter_across_slices_enabled_flag));
      }
    }

    if(pPPS -> tiles_enabled_flag || pPPS -> entropy_coding_sync_enabled_flag)
    {
      SyntaxNode &pitem = parent.add("if( tiles_enabled_flag || entropy_coding_sync_enabled_flag )");
      pitem.add("num_entry_point_offsets = " + n(pSlice -> num_entry_point_offsets));

      if(pSlice -> num_entry_point_offsets > 0)
      {
        SyntaxNode &pitemThird = pitem.add("if( num_entry_point_offsets > 0 )");
        pitemThird.add("offset_len_minus1 = " + n(pSlice -> offset_len_minus1));

        SyntaxNode &pitemLoop = pitemThird.add("for( i = 0; i < num_entry_point_offsets; i++ )");
        for(std::size_t i = 0; i < pSlice -> num_entry_point_offsets; i++)
          pitemLoop.add("entry_point_offset_minus1[" + n(i) + "] = " + n(pSlice -> entry_point_offset_minus1[i]));
      }
    }

    if(pPPS -> slice_segment_header_extension_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( slice_segment_header_extension_present_flag )");
      pitem.add("slice_segment_header_extension_length = " + n(pSlice -> slice_segment_header_extension_length));

      SyntaxNode &pitemLoop = pitem.add("for( i = 0; i < slice_segment_header_extension_length; i++ )");
      for(std::size_t i = 0; i < pSlice -> slice_segment_header_extension_length; i++)
        pitemLoop.add("slice_segment_header_extension_data_byte[" + n(i) + "] = " + n(pSlice -> slice_segment_header_extension_data_byte[i]));
    }
  }

  void SyntaxWriter::createAUD(std::shared_ptr<HEVC::AUD> pAUD, SyntaxNode &parent)
  {
    parent.add("pic_type = " + n(pAUD -> pic_type));
  }

  void SyntaxWriter::createSEI(std::shared_ptr<HEVC::SEI> pSEI, SyntaxNode &parent)
  {
    for(std::size_t i = 0; i < pSEI -> sei_message.size(); i++)
    {
      std::size_t payloadType = 0;
      std::size_t payloadSize = 0;

      SyntaxNode &pitem = parent.add("sei_message(" + n(i) + ")");

      if(pSEI -> sei_message[i].num_payload_type_ff_bytes)
      {
        SyntaxNode &pitemSecond = pitem.add("while( next_bits( 8 ) == 0xFF )");
        for(std::size_t k = 0; k < pSEI -> sei_message[i].num_payload_type_ff_bytes; k++)
        {
          pitemSecond.add("0xFF");
          payloadType += 255;
        }
      }

      pitem.add("last_payload_type_byte = " + n(pSEI -> sei_message[i].last_payload_type_byte));
      payloadType += pSEI -> sei_message[i].last_payload_type_byte;

      if(pSEI -> sei_message[i].num_payload_size_ff_bytes)
      {
        SyntaxNode &pitemSecond = pitem.add("while( next_bits( 8 ) == 0xFF )");
        for(std::size_t j = 0; j < pSEI -> sei_message[i].num_payload_size_ff_bytes; j++)
        {
          pitemSecond.add("0xFF");
          payloadSize += 255;
        }
      }

      pitem.add("last_payload_size_byte = " + n(pSEI -> sei_message[i].last_payload_size_byte));
      payloadSize += pSEI -> sei_message[i].last_payload_size_byte;

      switch(payloadType)
      {
        case HEVC::SeiMessage::DECODED_PICTURE_HASH:
        {
          std::shared_ptr<HEVC::DecodedPictureHash> p = std::dynamic_pointer_cast<HEVC::DecodedPictureHash>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("decoded_picture_hash(" + n(payloadSize) + ")");
          createDecodedPictureHash(p, c);
          break;
        }
        case HEVC::SeiMessage::USER_DATA_UNREGISTERED:
        {
          std::shared_ptr<HEVC::UserDataUnregistered> p = std::dynamic_pointer_cast<HEVC::UserDataUnregistered>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("user_data_unregistered(" + n(payloadSize) + ")");
          createUserDataUnregistered(p, c);
          break;
        }
        case HEVC::SeiMessage::SCENE_INFO:
        {
          std::shared_ptr<HEVC::SceneInfo> p = std::dynamic_pointer_cast<HEVC::SceneInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("scene_info(" + n(payloadSize) + ")");
          createSceneInfo(p, c);
          break;
        }
        case HEVC::SeiMessage::FULL_FRAME_SNAPSHOT:
        {
          std::shared_ptr<HEVC::FullFrameSnapshot> p = std::dynamic_pointer_cast<HEVC::FullFrameSnapshot>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("picture_snapshot(" + n(payloadSize) + ")");
          createFullFrameSnapshot(p, c);
          break;
        }
        case HEVC::SeiMessage::PROGRESSIVE_REFINEMENT_SEGMENT_START:
        {
          std::shared_ptr<HEVC::ProgressiveRefinementSegmentStart> p = std::dynamic_pointer_cast<HEVC::ProgressiveRefinementSegmentStart>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("progressive_refinement_segment_start(" + n(payloadSize) + ")");
          createProgressiveRefinementSegmentStart(p, c);
          break;
        }
        case HEVC::SeiMessage::PROGRESSIVE_REFINEMENT_SEGMENT_END:
        {
          std::shared_ptr<HEVC::ProgressiveRefinementSegmentEnd> p = std::dynamic_pointer_cast<HEVC::ProgressiveRefinementSegmentEnd>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("progressive_refinement_segment_end(" + n(payloadSize) + ")");
          createProgressiveRefinementSegmentEnd(p, c);
          break;
        }
        case HEVC::SeiMessage::BUFFERING_PERIOD:
        {
          std::shared_ptr<HEVC::BufferingPeriod> p = std::dynamic_pointer_cast<HEVC::BufferingPeriod>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("buffering_period(" + n(payloadSize) + ")");
          createBufferingPeriod(p, c);
          break;
        }
        case HEVC::SeiMessage::FILLER_PAYLOAD:
          pitem.add("filler_payload(" + n(payloadSize) + ")");
          break;
        case HEVC::SeiMessage::PICTURE_TIMING:
        {
          std::shared_ptr<HEVC::PicTiming> p = std::dynamic_pointer_cast<HEVC::PicTiming>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("pic_timing(" + n(payloadSize) + ")");
          createPicTiming(p, c);
          break;
        }
        case HEVC::SeiMessage::RECOVERY_POINT:
        {
          std::shared_ptr<HEVC::RecoveryPoint> p = std::dynamic_pointer_cast<HEVC::RecoveryPoint>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("recovery_point(" + n(payloadSize) + ")");
          createRecoveryPoint(p, c);
          break;
        }
        case HEVC::SeiMessage::TONE_MAPPING_INFO:
        {
          std::shared_ptr<HEVC::ToneMapping> p = std::dynamic_pointer_cast<HEVC::ToneMapping>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("tone_mapping_info(" + n(payloadSize) + ")");
          createToneMapping(p, c);
          break;
        }
        case HEVC::SeiMessage::FRAME_PACKING:
        {
          std::shared_ptr<HEVC::FramePacking> p = std::dynamic_pointer_cast<HEVC::FramePacking>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("frame_packing_arrangement(" + n(payloadSize) + ")");
          createFramePacking(p, c);
          break;
        }
        case HEVC::SeiMessage::DISPLAY_ORIENTATION:
        {
          std::shared_ptr<HEVC::DisplayOrientation> p = std::dynamic_pointer_cast<HEVC::DisplayOrientation>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("display_orientation(" + n(payloadSize) + ")");
          createDisplayOrientation(p, c);
          break;
        }
        case HEVC::SeiMessage::SOP_DESCRIPTION:
        {
          std::shared_ptr<HEVC::SOPDescription> p = std::dynamic_pointer_cast<HEVC::SOPDescription>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("structure_of_pictures_info(" + n(payloadSize) + ")");
          createSOPDescription(p, c);
          break;
        }
        case HEVC::SeiMessage::ACTIVE_PARAMETER_SETS:
        {
          std::shared_ptr<HEVC::ActiveParameterSets> p = std::dynamic_pointer_cast<HEVC::ActiveParameterSets>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("active_parameter_sets(" + n(payloadSize) + ")");
          createActiveParameterSets(p, c);
          break;
        }
        case HEVC::SeiMessage::TEMPORAL_LEVEL0_INDEX:
        {
          std::shared_ptr<HEVC::TemporalLevel0Index> p = std::dynamic_pointer_cast<HEVC::TemporalLevel0Index>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("temporal_sub_layer_zero_index(" + n(payloadSize) + ")");
          createTemporalLevel0Index(p, c);
          break;
        }
        case HEVC::SeiMessage::REGION_REFRESH_INFO:
        {
          std::shared_ptr<HEVC::RegionRefreshInfo> p = std::dynamic_pointer_cast<HEVC::RegionRefreshInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("region_refresh_info(" + n(payloadSize) + ")");
          createRegionRefreshInfo(p, c);
          break;
        }
        case HEVC::SeiMessage::TIME_CODE:
        {
          std::shared_ptr<HEVC::TimeCode> p = std::dynamic_pointer_cast<HEVC::TimeCode>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("time_code(" + n(payloadSize) + ")");
          createTimeCode(p, c);
          break;
        }
        case HEVC::SeiMessage::MASTERING_DISPLAY_INFO:
        {
          std::shared_ptr<HEVC::MasteringDisplayInfo> p = std::dynamic_pointer_cast<HEVC::MasteringDisplayInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("mastering_display_info(" + n(payloadSize) + ")");
          createMasteringDisplayInfo(p, c);
          break;
        }
        case HEVC::SeiMessage::SEGM_RECT_FRAME_PACKING:
        {
          std::shared_ptr<HEVC::SegmRectFramePacking> p = std::dynamic_pointer_cast<HEVC::SegmRectFramePacking>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("segm_rect_frame_packing(" + n(payloadSize) + ")");
          createSegmRectFramePacking(p, c);
          break;
        }
        case HEVC::SeiMessage::KNEE_FUNCTION_INFO:
        {
          std::shared_ptr<HEVC::KneeFunctionInfo> p = std::dynamic_pointer_cast<HEVC::KneeFunctionInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("knee_function_info(" + n(payloadSize) + ")");
          createKneeFunctionInfo(p, c);
          break;
        }
        case HEVC::SeiMessage::CHROMA_RESAMPLING_FILTER_HINT:
        {
          std::shared_ptr<HEVC::ChromaResamplingFilterHint> p = std::dynamic_pointer_cast<HEVC::ChromaResamplingFilterHint>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("chroma_resampling_filter_hint(" + n(payloadSize) + ")");
          createChromaResamplingFilterHint(p, c);
          break;
        }
        case HEVC::SeiMessage::COLOUR_REMAPPING_INFO:
        {
          std::shared_ptr<HEVC::ColourRemappingInfo> p = std::dynamic_pointer_cast<HEVC::ColourRemappingInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("colour_remapping_info(" + n(payloadSize) + ")");
          createColourRemappingInfo(p, c);
          break;
        }
        case HEVC::SeiMessage::CONTENT_LIGHT_LEVEL_INFO:
        {
          std::shared_ptr<HEVC::ContentLightLevelInfo> p = std::dynamic_pointer_cast<HEVC::ContentLightLevelInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("content_light_level_info(" + n(payloadSize) + ")");
          createContentLightLevelInfo(p, c);
          break;
        }
        case HEVC::SeiMessage::ALTERNATIVE_TRANSFER_CHARACTERISTICS:
        {
          std::shared_ptr<HEVC::AlternativeTransferCharacteristics> p = std::dynamic_pointer_cast<HEVC::AlternativeTransferCharacteristics>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("alternative_transfer_characteristics(" + n(payloadSize) + ")");
          createAlternativeTransferCharacteristics(p, c);
          break;
        }
        default:
        {
          std::shared_ptr<HEVC::SeiReservedInfo> p = std::dynamic_pointer_cast<HEVC::SeiReservedInfo>(pSEI -> sei_message[i].sei_payload);
          SyntaxNode &c = pitem.add("sei_reserved_info(" + n(payloadSize) + ")");
          createReserved(p, c);
          pitem.add("sei_payload(" + n(payloadType) + ", " + n(payloadSize) + ")");
        }
      }
    }
  }

  void SyntaxWriter::createProfileTierLevel(const HEVC::ProfileTierLevel &ptl, SyntaxNode &parent)
  {
    parent.add("general_profile_space = " + n(ptl.general_profile_space));
    parent.add("general_tier_flag = " + n(ptl.general_tier_flag));
    parent.add("general_profile_idc = " + n(ptl.general_profile_idc));

    std::string str = "general_profile_compatibility_flag[i] = { ";
    for(std::size_t i = 0; i < 32; i++)
    {
      if(i)
        str += ", ";
      str += n(ptl.general_profile_compatibility_flag[i]);
    }
    str += " }";
    parent.add(str);

    parent.add("general_progressive_source_flag = " + n(ptl.general_progressive_source_flag));
    parent.add("general_interlaced_source_flag = " + n(ptl.general_interlaced_source_flag));
    parent.add("general_non_packed_constraint_flag = " + n(ptl.general_non_packed_constraint_flag));
    parent.add("general_frame_only_constraint_flag = " + n(ptl.general_frame_only_constraint_flag));
    parent.add("general_level_idc = " + n(ptl.general_level_idc));

    if(ptl.sub_layer_profile_present_flag.size() == 0)
    {
      parent.add("sub_layer_profile_present_flag = { }");
      parent.add("sub_layer_level_present_flag = { }");
    }
    else
    {
      str = "sub_layer_profile_present_flag = { ";
      for(std::size_t i = 0; i < ptl.sub_layer_profile_present_flag.size(); i++)
      {
        if(i)
          str += ", ";
        str += n(ptl.sub_layer_profile_present_flag[i]);
      }
      str += " }";
      parent.add(str);

      str = "sub_layer_level_present_flag = { ";
      for(std::size_t i = 0; i < ptl.sub_layer_level_present_flag.size(); i++)
      {
        if(i)
          str += ", ";
        str += n(ptl.sub_layer_level_present_flag[i]);
      }
      str += " }";
      parent.add(str);
    }

    std::size_t maxNumSubLayersMinus1 = ptl.sub_layer_profile_present_flag.size();
    bool needLoop = false;
    for(std::size_t i = 0; i < maxNumSubLayersMinus1 && !needLoop; i++)
    {
      if(ptl.sub_layer_profile_present_flag[i] || ptl.sub_layer_level_present_flag[i])
        needLoop = true;
    }

    if(needLoop)
    {
      SyntaxNode &pitemLoop = parent.add("for( i = 0; i < maxNumSubLayersMinus1; i++ )");
      for(std::size_t i = 0; i < maxNumSubLayersMinus1; i++)
      {
        if(ptl.sub_layer_profile_present_flag[i])
        {
          SyntaxNode &pitem = pitemLoop.add("if( sub_layer_profile_present_flag[" + n(i) + "] )");
          pitem.add("sub_layer_profile_space[" + n(i) + "] = " + n(ptl.sub_layer_profile_space[i]));
          pitem.add("sub_layer_tier_flag[" + n(i) + "] = " + n(ptl.sub_layer_tier_flag[i]));
          pitem.add("sub_layer_profile_idc[" + n(i) + "] = " + n(ptl.sub_layer_profile_idc[i]));

          std::string s = "sub_layer_profile_compatibility_flag[" + n(i) + "] = { ";
          for(std::size_t j = 0; j < 32; j++)
          {
            if(j)
              s += ", ";
            s += n(ptl.sub_layer_profile_compatibility_flag[i][j]);
          }
          s += " }";
          pitem.add(s);

          pitem.add("sub_layer_progressive_source_flag[" + n(i) + "] = " + n(ptl.sub_layer_progressive_source_flag[i]));
          pitem.add("sub_layer_interlaced_source_flag[" + n(i) + "] = " + n(ptl.sub_layer_interlaced_source_flag[i]));
          pitem.add("sub_layer_non_packed_constraint_flag[" + n(i) + "] = " + n(ptl.sub_layer_non_packed_constraint_flag[i]));
          pitem.add("sub_layer_frame_only_constraint_flag[" + n(i) + "] = " + n(ptl.sub_layer_frame_only_constraint_flag[i]));
        }

        if(ptl.sub_layer_level_present_flag[i])
        {
          SyntaxNode &pitem = pitemLoop.add("if( sub_layer_level_present_flag[" + n(i) + "] )");
          pitem.add("sub_layer_level_idc[" + n(i) + "] = " + n(ptl.sub_layer_level_idc[i]));
        }
      }
    }
  }

  void SyntaxWriter::createVuiParameters(const HEVC::VuiParameters &vui, std::size_t maxNumSubLayersMinus1, SyntaxNode &parent)
  {
    parent.add("aspect_ratio_info_present_flag = " + n(vui.aspect_ratio_info_present_flag));

    if(vui.aspect_ratio_info_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( aspect_ratio_info_present_flag )");
      pitem.add("aspect_ratio_idc = " + n(vui.aspect_ratio_idc));

      if(vui.aspect_ratio_idc == 255)
      {
        SyntaxNode &pitemSecond = pitem.add("if( aspect_ratio_idc == EXTENDED_SAR )");
        pitemSecond.add("sar_width = " + n(vui.sar_width));
        pitemSecond.add("sar_height = " + n(vui.sar_height));
      }
    }

    parent.add("overscan_info_present_flag = " + n(vui.overscan_info_present_flag));
    if(vui.overscan_info_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( overscan_info_present_flag )");
      pitem.add("overscan_appropriate_flag = " + n(vui.overscan_appropriate_flag));
    }

    parent.add("video_signal_type_present_flag = " + n(vui.video_signal_type_present_flag));
    if(vui.video_signal_type_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( video_signal_type_present_flag )");
      pitem.add("video_format = " + n(vui.video_format));
      pitem.add("video_full_range_flag = " + n(vui.video_full_range_flag));
      pitem.add("colour_description_present_flag = " + n(vui.colour_description_present_flag));

      if(vui.colour_description_present_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( colour_description_present_flag )");
        pitemSecond.add("colour_primaries = " + n(vui.colour_primaries));
        pitemSecond.add("transfer_characteristics = " + n(vui.transfer_characteristics));
        pitemSecond.add("matrix_coeffs = " + n(vui.matrix_coeffs));
      }
    }

    parent.add("chroma_loc_info_present_flag = " + n(vui.chroma_loc_info_present_flag));
    if(vui.chroma_loc_info_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( chroma_loc_info_present_flag )");
      pitem.add("chroma_sample_loc_type_top_field = " + n(vui.chroma_sample_loc_type_top_field));
      pitem.add("chroma_sample_loc_type_bottom_field = " + n(vui.chroma_sample_loc_type_bottom_field));
    }

    parent.add("neutral_chroma_indication_flag = " + n(vui.neutral_chroma_indication_flag));
    parent.add("field_seq_flag = " + n(vui.field_seq_flag));
    parent.add("frame_field_info_present_flag = " + n(vui.frame_field_info_present_flag));
    parent.add("default_display_window_flag = " + n(vui.default_display_window_flag));

    if(vui.default_display_window_flag)
    {
      SyntaxNode &pitem = parent.add("if( default_display_window_flag )");
      pitem.add("def_disp_win_left_offset = " + n(vui.def_disp_win_left_offset));
      pitem.add("def_disp_win_right_offset = " + n(vui.def_disp_win_right_offset));
      pitem.add("def_disp_win_top_offset = " + n(vui.def_disp_win_top_offset));
      pitem.add("def_disp_win_bottom_offset = " + n(vui.def_disp_win_bottom_offset));
    }

    parent.add("vui_timing_info_present_flag = " + n(vui.vui_timing_info_present_flag));
    if(vui.vui_timing_info_present_flag)
    {
      SyntaxNode &pitem = parent.add("if( vui_timing_info_present_flag )");
      pitem.add("vui_num_units_in_tick = " + n(vui.vui_num_units_in_tick));
      pitem.add("vui_time_scale = " + n(vui.vui_time_scale));
      pitem.add("vui_poc_proportional_to_timing_flag = " + n(vui.vui_poc_proportional_to_timing_flag));

      if(vui.vui_poc_proportional_to_timing_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( vui_poc_proportional_to_timing_flag )");
        pitemSecond.add("vui_num_ticks_poc_diff_one_minus1 = " + n(vui.vui_num_ticks_poc_diff_one_minus1));
      }

      pitem.add("vui_hrd_parameters_present_flag = " + n(vui.vui_hrd_parameters_present_flag));

      if(vui.vui_hrd_parameters_present_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( vui_hrd_parameters_present_flag )");
        SyntaxNode &pitemHrd = pitemSecond.add("hrd_parameters(1, " + n(maxNumSubLayersMinus1) + ")");
        createHrdParameters(vui.hrd_parameters, 1, pitemHrd);
      }
    }

    parent.add("bitstream_restriction_flag = " + n(vui.bitstream_restriction_flag));
    if(vui.bitstream_restriction_flag)
    {
      SyntaxNode &pitem = parent.add("if( bitstream_restriction_flag )");
      pitem.add("tiles_fixed_structure_flag = " + n(vui.tiles_fixed_structure_flag));
      pitem.add("motion_vectors_over_pic_boundaries_flag = " + n(vui.motion_vectors_over_pic_boundaries_flag));
      pitem.add("restricted_ref_pic_lists_flag = " + n(vui.restricted_ref_pic_lists_flag));
      pitem.add("min_spatial_segmentation_idc = " + n(vui.min_spatial_segmentation_idc));
      pitem.add("max_bytes_per_pic_denom = " + n(vui.max_bytes_per_pic_denom));
      pitem.add("max_bits_per_min_cu_denom = " + n(vui.max_bits_per_min_cu_denom));
      pitem.add("log2_max_mv_length_horizontal = " + n(vui.log2_max_mv_length_horizontal));
      pitem.add("log2_max_mv_length_vertical = " + n(vui.log2_max_mv_length_vertical));
    }
  }

  void SyntaxWriter::createHrdParameters(const HEVC::HrdParameters &hrd, uint8_t commonInfPresentFlag, SyntaxNode &parent)
  {
    if(commonInfPresentFlag)
    {
      SyntaxNode &pitem = parent.add("if( commonInfPresentFlag )");
      pitem.add("nal_hrd_parameters_present_flag = " + n(hrd.nal_hrd_parameters_present_flag));
      pitem.add("vcl_hrd_parameters_present_flag = " + n(hrd.vcl_hrd_parameters_present_flag));

      if(hrd.nal_hrd_parameters_present_flag || hrd.vcl_hrd_parameters_present_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag )");
        pitemSecond.add("sub_pic_hrd_params_present_flag = " + n(hrd.sub_pic_hrd_params_present_flag));

        if(hrd.sub_pic_hrd_params_present_flag)
        {
          SyntaxNode &pitemThird = pitemSecond.add("if( sub_pic_hrd_params_present_flag )");
          pitemThird.add("tick_divisor_minus2 = " + n(hrd.tick_divisor_minus2));
          pitemThird.add("du_cpb_removal_delay_increment_length_minus1 = " + n(hrd.du_cpb_removal_delay_increment_length_minus1));
          pitemThird.add("sub_pic_cpb_params_in_pic_timing_sei_flag = " + n(hrd.sub_pic_cpb_params_in_pic_timing_sei_flag));
          pitemThird.add("dpb_output_delay_du_length_minus1 = " + n(hrd.dpb_output_delay_du_length_minus1));
        }

        pitemSecond.add("bit_rate_scale = " + n(hrd.bit_rate_scale));
        pitemSecond.add("cpb_size_scale = " + n(hrd.cpb_size_scale));

        if(hrd.sub_pic_hrd_params_present_flag)
        {
          SyntaxNode &pitemThird = pitemSecond.add("if( sub_pic_hrd_params_present_flag )");
          pitemThird.add("cpb_size_du_scale = " + n(hrd.cpb_size_du_scale));
        }

        pitemSecond.add("initial_cpb_removal_delay_length_minus1 = " + n(hrd.initial_cpb_removal_delay_length_minus1));
        pitemSecond.add("au_cpb_removal_delay_length_minus1 = " + n(hrd.au_cpb_removal_delay_length_minus1));
        pitemSecond.add("dpb_output_delay_length_minus1 = " + n(hrd.dpb_output_delay_length_minus1));
      }
    }

    if(hrd.fixed_pic_rate_general_flag.size() > 0)
    {
      SyntaxNode &pitem = parent.add("for( i = 0; i <= maxNumSubLayersMinus1; i++ )");
      for(std::size_t i = 0; i < hrd.fixed_pic_rate_general_flag.size(); i++)
      {
        pitem.add("fixed_pic_rate_general_flag[" + n(i) + "] = " + n(hrd.fixed_pic_rate_general_flag[i]));

        if(!hrd.fixed_pic_rate_general_flag[i])
        {
          SyntaxNode &pitemSecond = pitem.add("if( !fixed_pic_rate_general_flag[" + n(i) + "] )");
          pitemSecond.add("fixed_pic_rate_within_cvs_flag[" + n(i) + "] = " + n(hrd.fixed_pic_rate_within_cvs_flag[i]));
        }

        if(hrd.fixed_pic_rate_within_cvs_flag[i])
        {
          SyntaxNode &pitemSecond = pitem.add("if( fixed_pic_rate_within_cvs_flag[" + n(i) + "] )");
          pitemSecond.add("elemental_duration_in_tc_minus1[" + n(i) + "] = " + n(hrd.elemental_duration_in_tc_minus1[i]));
        }
        else
        {
          SyntaxNode &pitemSecond = pitem.add("if( !fixed_pic_rate_within_cvs_flag[" + n(i) + "] )");
          pitemSecond.add("low_delay_hrd_flag[" + n(i) + "] = " + n(hrd.low_delay_hrd_flag[i]));
        }

        if(!hrd.low_delay_hrd_flag[i])
        {
          SyntaxNode &pitemSecond = pitem.add("if( !low_delay_hrd_flag[" + n(i) + "] )");
          pitemSecond.add("cpb_cnt_minus1[" + n(i) + "] = " + n(hrd.cpb_cnt_minus1[i]));
        }

        if(hrd.nal_hrd_parameters_present_flag)
        {
          SyntaxNode &pitemSecond = pitem.add("if( nal_hrd_parameters_present_flag )");
          SyntaxNode &pitemThird = pitemSecond.add("sub_layer_hrd_parameters(" + n(i) + ")");
          createSubLayerHrdParameters(hrd.nal_sub_layer_hrd_parameters[i], hrd.sub_pic_hrd_params_present_flag, pitemThird);
        }

        if(hrd.vcl_hrd_parameters_present_flag)
        {
          SyntaxNode &pitemSecond = pitem.add("if( vcl_hrd_parameters_present_flag )");
          SyntaxNode &pitemThird = pitemSecond.add("sub_layer_hrd_parameters(" + n(i) + ")");
          createSubLayerHrdParameters(hrd.vcl_sub_layer_hrd_parameters[i], hrd.sub_pic_hrd_params_present_flag, pitemThird);
        }
      }
    }
  }

  void SyntaxWriter::createSubLayerHrdParameters(const HEVC::SubLayerHrdParameters &slhrd, uint8_t sub_pic_hrd_params_present_flag, SyntaxNode &parent)
  {
    SyntaxNode &pitem = parent.add("for( i = 0; i <= CpbCnt; i++ )");
    for(std::size_t i = 0; i < slhrd.bit_rate_value_minus1.size(); i++)
    {
      pitem.add("bit_rate_value_minus1[" + n(i) + "] = " + n(slhrd.bit_rate_value_minus1[i]));
      pitem.add("cpb_size_value_minus1[" + n(i) + "] = " + n(slhrd.cpb_size_value_minus1[i]));

      if(sub_pic_hrd_params_present_flag)
      {
        SyntaxNode &pitemSecond = pitem.add("if( sub_pic_hrd_params_present_flag )");
        pitemSecond.add("cpb_size_du_value_minus1[" + n(i) + "] = " + n(slhrd.cpb_size_du_value_minus1[i]));
        pitemSecond.add("bit_rate_du_value_minus1[" + n(i) + "] = " + n(slhrd.bit_rate_du_value_minus1[i]));
      }

      pitem.add("cbr_flag[" + n(i) + "] = " + n(slhrd.cbr_flag[i]));
    }
  }

  void SyntaxWriter::createShortTermRefPicSet(std::size_t stRpsIdx, const HEVC::ShortTermRefPicSet &rpset, std::size_t num_short_term_ref_pic_sets, const std::vector<HEVC::ShortTermRefPicSet> &refPicSets, SyntaxNode &parent)
  {
    if(stRpsIdx)
    {
      SyntaxNode &pitem = parent.add("if( stRpsIdx != 0 )");
      pitem.add("inter_ref_pic_set_prediction_flag = " + n(rpset.inter_ref_pic_set_prediction_flag));
    }

    if(rpset.inter_ref_pic_set_prediction_flag)
    {
      SyntaxNode &pitem = parent.add("if( inter_ref_pic_set_prediction_flag )");

      if(stRpsIdx == num_short_term_ref_pic_sets)
      {
        SyntaxNode &pitemSecond = pitem.add("if( stRpsIdx == num_short_term_ref_pic_sets )");
        pitemSecond.add("delta_idx_minus1 = " + n(rpset.delta_idx_minus1));
      }

      pitem.add("delta_rps_sign = " + n(rpset.delta_rps_sign));
      pitem.add("abs_delta_rps_minus1 = " + n(rpset.abs_delta_rps_minus1));

      std::size_t RefRpsIdx = stRpsIdx - (rpset.delta_idx_minus1 + 1);
      std::size_t NumDeltaPocs = 0;

      if(RefRpsIdx < refPicSets.size() && refPicSets[RefRpsIdx].inter_ref_pic_set_prediction_flag)
      {
        for(std::size_t i = 0; i < refPicSets[RefRpsIdx].used_by_curr_pic_flag.size(); i++)
          if(refPicSets[RefRpsIdx].used_by_curr_pic_flag[i] || refPicSets[RefRpsIdx].use_delta_flag[i])
            NumDeltaPocs++;
      }
      else if(RefRpsIdx < refPicSets.size())
        NumDeltaPocs = refPicSets[RefRpsIdx].num_negative_pics + refPicSets[RefRpsIdx].num_positive_pics;

      SyntaxNode &pitemLoop = pitem.add("for( j = 0; j <= NumDeltaPocs[ RefRpsIdx ]; j++ )");
      for(std::size_t i = 0; i <= NumDeltaPocs; i++)
      {
        pitemLoop.add("used_by_curr_pic_flag[" + n(i) + "] = " + n(rpset.used_by_curr_pic_flag[i]));
        if(!rpset.used_by_curr_pic_flag[i])
        {
          SyntaxNode &p = pitemLoop.add("if( !used_by_curr_pic_flag[j] )");
          p.add("use_delta_flag[" + n(i) + "] = " + n(rpset.use_delta_flag[i]));
        }
      }
    }
    else
    {
      SyntaxNode &pitem = parent.add("if( !inter_ref_pic_set_prediction_flag )");
      pitem.add("num_negative_pics = " + n(rpset.num_negative_pics));
      pitem.add("num_positive_pics = " + n(rpset.num_positive_pics));

      SyntaxNode &pitemLoop = pitem.add("for( i = 0; i < num_negative_pics; i++ )");
      for(std::size_t i = 0; i < rpset.num_negative_pics; i++)
      {
        pitemLoop.add("delta_poc_s0_minus1[" + n(i) + "] = " + n(rpset.delta_poc_s0_minus1[i]));
        pitemLoop.add("used_by_curr_pic_s0_flag[" + n(i) + "] = " + n(rpset.used_by_curr_pic_s0_flag[i]));
      }

      SyntaxNode &pitemLoop2 = pitem.add("for( i = 0; i < num_positive_pics; i++ )");
      for(std::size_t i = 0; i < rpset.num_positive_pics; i++)
      {
        pitemLoop2.add("delta_poc_s1_minus1[" + n(i) + "] = " + n(rpset.delta_poc_s1_minus1[i]));
        pitemLoop2.add("used_by_curr_pic_s1_flag[" + n(i) + "] = " + n(rpset.used_by_curr_pic_s1_flag[i]));
      }
    }
  }

  void SyntaxWriter::createScalingListData(const HEVC::ScalingListData &scdata, SyntaxNode &parent)
  {
    SyntaxNode &pitem = parent.add("for( sizeId = 0; sizeId < 4; sizeId++ )");
    SyntaxNode &pitemSecond = pitem.add("for( matrixId = 0; matrixId < ( ( sizeId == 3 ) ? 2 : 6 ); matrixId++ )");

    for(std::size_t sizeId = 0; sizeId < 4; sizeId++)
    {
      for(std::size_t matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); matrixId++)
      {
        pitemSecond.add("scaling_list_pred_mode_flag[" + n(sizeId) + "][" + n(matrixId) + "] = " + n(scdata.scaling_list_pred_mode_flag[sizeId][matrixId]));

        if(!scdata.scaling_list_pred_mode_flag[sizeId][matrixId])
        {
          SyntaxNode &p = pitemSecond.add("if( !scaling_list_pred_mode_flag[ sizeId ][ matrixId ] )");
          p.add("scaling_list_pred_matrix_id_delta[" + n(sizeId) + "][" + n(matrixId) + "] = " + n(scdata.scaling_list_pred_matrix_id_delta[sizeId][matrixId]));
        }
        else
        {
          std::size_t coefNum = std::min<std::size_t>(64, (1 << (4 + (sizeId << 1))));
          if(sizeId > 1)
          {
            SyntaxNode &p = pitemSecond.add("if( sizeId > 1 )");
            p.add("scaling_list_dc_coef_minus8[" + n(sizeId) + "][" + n(matrixId) + "] = " + n(scdata.scaling_list_dc_coef_minus8[sizeId - 2][matrixId]));
          }

          SyntaxNode &p = pitemSecond.add("for( i = 0; i < coefNum; i++ )");
          for(std::size_t i = 0; i < coefNum; i++)
            p.add("scaling_list_delta_coef[" + n(sizeId) + "][" + n(matrixId) + "][" + n(i) + "] = " + n(scdata.scaling_list_delta_coef[sizeId][matrixId][i]));
        }
      }
    }
  }

  void SyntaxWriter::createRefPicListModification(const HEVC::RefPicListModification &rplModif, SyntaxNode &parent)
  {
    parent.add("ref_pic_list_modification_flag_l0 = " + n(rplModif.ref_pic_list_modification_flag_l0));
    if(rplModif.ref_pic_list_modification_flag_l0)
    {
      SyntaxNode &pitem = parent.add("for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ )");
      for(std::size_t i = 0; i < rplModif.list_entry_l0.size(); i++)
        pitem.add("list_entry_l0[" + n(i) + "] = " + n(rplModif.list_entry_l0[i]));
    }

    parent.add("ref_pic_list_modification_flag_l1 = " + n(rplModif.ref_pic_list_modification_flag_l1));
    if(rplModif.ref_pic_list_modification_flag_l1)
    {
      SyntaxNode &pitem = parent.add("for( i = 0; i <= num_ref_idx_l1_active_minus1; i++ )");
      for(std::size_t i = 0; i < rplModif.list_entry_l1.size(); i++)
        pitem.add("list_entry_l1[" + n(i) + "] = " + n(rplModif.list_entry_l1[i]));
    }
  }

  void SyntaxWriter::createPredWeightTable(const HEVC::PredWeightTable &pwt, std::shared_ptr<HEVC::Slice> pSlice, SyntaxNode &parent)
  {
    std::shared_ptr<HEVC::PPS> ppps = m_ppsMap[pSlice -> slice_pic_parameter_set_id];
    if(!ppps)
      return;

    std::shared_ptr<HEVC::SPS> psps = m_spsMap[ppps -> pps_seq_parameter_set_id];
    if(!psps)
      return;

    parent.add("luma_log2_weight_denom = " + n(pwt.luma_log2_weight_denom));

    if(psps -> chroma_format_idc != 0)
    {
      SyntaxNode &pitem = parent.add("if( chroma_format_idc != 0 )");
      pitem.add("delta_chroma_log2_weight_denom = " + n(pwt.delta_chroma_log2_weight_denom));
    }

    SyntaxNode &pitemLoop = parent.add("for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ )");
    for(std::size_t i = 0; i <= pSlice -> num_ref_idx_l0_active_minus1; i++)
      pitemLoop.add("luma_weight_l0_flag[" + n(i) + "] = " + n(pwt.luma_weight_l0_flag[i]));

    if(psps -> chroma_format_idc != 0)
    {
      SyntaxNode &pitem = parent.add("if( chroma_format_idc != 0 )");
      SyntaxNode &pl = pitem.add("for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ )");
      for(std::size_t i = 0; i <= pSlice -> num_ref_idx_l0_active_minus1; i++)
        pl.add("chroma_weight_l0_flag[" + n(i) + "] = " + n(pwt.chroma_weight_l0_flag[i]));
    }

    for(std::size_t i = 0; i <= pSlice -> num_ref_idx_l0_active_minus1; i++)
    {
      SyntaxNode &pl = parent.add("for( i = 0; i <= num_ref_idx_l0_active_minus1; i++ )");

      if(pwt.luma_weight_l0_flag[i])
      {
        SyntaxNode &p = pl.add("if( luma_weight_l0_flag[" + n(i) + "] )");
        p.add("delta_luma_weight_l0[" + n(i) + "] = " + n(pwt.delta_luma_weight_l0[i]));
        p.add("luma_offset_l0[" + n(i) + "] = " + n(pwt.luma_offset_l0[i]));
      }
      if(pwt.chroma_weight_l0_flag[i])
      {
        SyntaxNode &p = pl.add("if( chroma_weight_l0_flag[" + n(i) + "] )");
        p.add("delta_chroma_weight_l0[" + n(i) + "] = { " + n(pwt.delta_chroma_weight_l0[i][0]) + ", " + n(pwt.delta_chroma_weight_l0[i][1]) + " }");
        p.add("delta_chroma_offset_l0[" + n(i) + "] = { " + n(pwt.delta_chroma_offset_l0[i][0]) + ", " + n(pwt.delta_chroma_offset_l0[i][1]) + " }");
      }
    }

    if(pSlice -> slice_type == SLICE_B)
    {
      SyntaxNode &pitemBSlice = parent.add("if( slice_type == B )");

      SyntaxNode &pitemLoop2 = pitemBSlice.add("for( i = 0; i <= num_ref_idx_l1_active_minus1; i++ )");
      for(std::size_t i = 0; i <= pSlice -> num_ref_idx_l1_active_minus1; i++)
        pitemLoop2.add("luma_weight_l1_flag[" + n(i) + "] = " + n(pwt.luma_weight_l1_flag[i]));

      if(psps -> chroma_format_idc != 0)
      {
        SyntaxNode &pitem = pitemBSlice.add("if( chroma_format_idc != 0 )");
        SyntaxNode &pl = pitem.add("for( i = 0; i <= num_ref_idx_l1_active_minus1; i++ )");
        for(std::size_t i = 0; i <= pSlice -> num_ref_idx_l1_active_minus1; i++)
          pl.add("chroma_weight_l1_flag[" + n(i) + "] = " + n(pwt.chroma_weight_l1_flag[i]));
      }

      for(std::size_t i = 0; i <= pSlice -> num_ref_idx_l1_active_minus1; i++)
      {
        SyntaxNode &pl = pitemBSlice.add("for( i = 0; i <= num_ref_idx_l1_active_minus1; i++ )");

        if(pwt.luma_weight_l1_flag[i])
        {
          SyntaxNode &p = pl.add("if( luma_weight_l1_flag[" + n(i) + "] )");
          p.add("delta_luma_weight_l1[" + n(i) + "] = " + n(pwt.delta_luma_weight_l1[i]));
          p.add("luma_offset_l1[" + n(i) + "] = " + n(pwt.luma_offset_l1[i]));
        }
        if(pwt.chroma_weight_l1_flag[i])
        {
          SyntaxNode &p = pl.add("if( chroma_weight_l1_flag[" + n(i) + "] )");
          p.add("delta_chroma_weight_l1[" + n(i) + "] = { " + n(pwt.delta_chroma_weight_l1[i][0]) + ", " + n(pwt.delta_chroma_weight_l1[i][1]) + " }");
          p.add("delta_chroma_offset_l1[" + n(i) + "] = { " + n(pwt.delta_chroma_offset_l1[i][0]) + ", " + n(pwt.delta_chroma_offset_l1[i][1]) + " }");
        }
      }
    }
  }

  void SyntaxWriter::createDecodedPictureHash(std::shared_ptr<HEVC::DecodedPictureHash> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("hash_type = " + n(p->hash_type));

    SyntaxNode &ploop = parent.add("for( cIdx = 0; cIdx < ( chroma_format_idc == 0 ? 1 : 3 ); cIdx++ )");
    SyntaxNode &pif = ploop.add("if( hash_type == " + n(p->hash_type) + " )");

    if(p->hash_type == 0)
    {
      for(std::size_t i = 0; i < p->picture_md5.size(); i++)
      {
        std::string str;
        for(std::size_t j = 0; j < 16; j++)
          str += hex2Upper(p->picture_md5[i][j]);
        pif.add("picture_md5[" + n(i) + "] = " + str);
      }
    }
    else if(p->hash_type == 1)
    {
      for(std::size_t i = 0; i < p->picture_crc.size(); i++)
        pif.add("picture_crc[" + n(i) + "] = " + n(p->picture_crc[i]));
    }
    else
    {
      for(std::size_t i = 0; i < p->picture_checksum.size(); i++)
        pif.add("picture_checksum[" + n(i) + "] = " + n(p->picture_checksum[i]));
    }
  }

  void SyntaxWriter::createUserDataUnregistered(std::shared_ptr<HEVC::UserDataUnregistered> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("uuid_iso_iec_11578 = " + uuidFormat(p->uuid_iso_iec_11578));

    if(p->user_data_payload_byte.empty())
    {
      parent.add("user_data_payload_byte = { }");
    }
    else
    {
      std::string str = "user_data_payload_byte = { ";
      for(std::size_t i = 0; i < p->user_data_payload_byte.size(); i++)
      {
        if(i)
          str += ", ";
        str += n(p->user_data_payload_byte[i]);
      }
      str += " }";
      parent.add(str);
    }
  }

  void SyntaxWriter::createReserved(std::shared_ptr<HEVC::SeiReservedInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("uuid_iso_iec_11578 = " + uuidFormat(p->uuid_iso_iec_11578));

    if(p->user_data_payload_byte.empty())
    {
      parent.add("reserved_payload_byte = { }");
    }
    else
    {
      std::string str = "reserved_payload_byte = { ";
      for(std::size_t i = 0; i < p->user_data_payload_byte.size(); i++)
      {
        if(i)
          str += ", ";
        str += n(p->user_data_payload_byte[i]);
      }
      str += " }";
      parent.add(str);
    }
  }

  void SyntaxWriter::createSceneInfo(std::shared_ptr<HEVC::SceneInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("scene_info_present_flag = " + n(p->scene_info_present_flag));
    if(p->scene_info_present_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( scene_info_present_flag )");
      pitemIf.add("prev_scene_id_valid_flag = " + n(p->prev_scene_id_valid_flag));
      pitemIf.add("scene_id = " + n(p->scene_id));
      pitemIf.add("scene_transition_type = " + n(p->scene_transition_type));

      if(p->scene_transition_type > 3)
      {
        SyntaxNode &pitemIfSecond = pitemIf.add("if( scene_transition_type > 3 )");
        pitemIfSecond.add("second_scene_id = " + n(p->second_scene_id));
      }
    }
  }

  void SyntaxWriter::createFullFrameSnapshot(std::shared_ptr<HEVC::FullFrameSnapshot> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("snapshot_id = " + n(p->snapshot_id));
  }

  void SyntaxWriter::createProgressiveRefinementSegmentStart(std::shared_ptr<HEVC::ProgressiveRefinementSegmentStart> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("progressive_refinement_id = " + n(p->progressive_refinement_id));
    parent.add("pic_order_cnt_delta = " + n(p->pic_order_cnt_delta));
  }

  void SyntaxWriter::createProgressiveRefinementSegmentEnd(std::shared_ptr<HEVC::ProgressiveRefinementSegmentEnd> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("progressive_refinement_id = " + n(p->progressive_refinement_id));
  }

  void SyntaxWriter::createBufferingPeriod(std::shared_ptr<HEVC::BufferingPeriod> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("bp_seq_parameter_set_id = " + n(p->bp_seq_parameter_set_id));

    std::shared_ptr<HEVC::SPS> psps = m_spsMap[p->bp_seq_parameter_set_id];
    if(!psps)
      return;

    if(!psps->vui_parameters.hrd_parameters.sub_pic_hrd_params_present_flag)
    {
      SyntaxNode &pitemIrap = parent.add("if( !sub_pic_hrd_params_present_flag )");
      pitemIrap.add("irap_cpb_params_present_flag = " + n(p->irap_cpb_params_present_flag));
    }

    if(p->irap_cpb_params_present_flag)
    {
      SyntaxNode &pitemIrap = parent.add("if( irap_cpb_params_present_flag )");
      pitemIrap.add("cpb_delay_offset = " + n(p->cpb_delay_offset));
      pitemIrap.add("dpb_delay_offset = " + n(p->dpb_delay_offset));
    }

    parent.add("concatenation_flag = " + n(p->concatenation_flag));
    parent.add("au_cpb_removal_delay_delta_minus1 = " + n(p->au_cpb_removal_delay_delta_minus1));

    bool NalHrdBpPresentFlag = psps->vui_parameters.hrd_parameters.nal_hrd_parameters_present_flag;
    if(NalHrdBpPresentFlag)
    {
      SyntaxNode &pitemBp = parent.add("if( NalHrdBpPresentFlag )");
      SyntaxNode &ploop = pitemBp.add("for( i = 0; i <= CpbCnt; i++ )");
      for(std::size_t i = 0; i < p->nal_initial_cpb_removal_delay.size(); i++)
      {
        ploop.add("nal_initial_cpb_removal_delay[" + n(i) + "] = " + n(p->nal_initial_cpb_removal_delay[i]));
        ploop.add("nal_initial_cpb_removal_offset[" + n(i) + "] = " + n(p->nal_initial_cpb_removal_offset[i]));

        if(psps->vui_parameters.hrd_parameters.sub_pic_hrd_params_present_flag || p->irap_cpb_params_present_flag)
        {
          SyntaxNode &pitemAlt = ploop.add("if( sub_pic_hrd_params_present_flag || irap_cpb_params_present_flag )");
          pitemAlt.add("nal_initial_alt_cpb_removal_delay[" + n(i) + "] = " + n(p->nal_initial_alt_cpb_removal_delay[i]));
          pitemAlt.add("nal_initial_alt_cpb_removal_offset[" + n(i) + "] = " + n(p->nal_initial_alt_cpb_removal_offset[i]));
        }
      }
    }

    bool VclHrdBpPresentFlag = psps->vui_parameters.hrd_parameters.vcl_hrd_parameters_present_flag;
    if(VclHrdBpPresentFlag)
    {
      SyntaxNode &pitemBp = parent.add("if( VclHrdBpPresentFlag )");
      SyntaxNode &ploop = pitemBp.add("for( i = 0; i <= CpbCnt; i++ )");
      for(std::size_t i = 0; i < p->vcl_initial_cpb_removal_delay.size(); i++)
      {
        ploop.add("vcl_initial_cpb_removal_delay[" + n(i) + "] = " + n(p->vcl_initial_cpb_removal_delay[i]));
        ploop.add("vcl_initial_cpb_removal_offset[" + n(i) + "] = " + n(p->vcl_initial_cpb_removal_offset[i]));

        if(psps->vui_parameters.hrd_parameters.sub_pic_hrd_params_present_flag || p->irap_cpb_params_present_flag)
        {
          SyntaxNode &pitemAlt = ploop.add("if( sub_pic_hrd_params_present_flag || irap_cpb_params_present_flag )");
          pitemAlt.add("vcl_initial_alt_cpb_removal_delay[" + n(i) + "] = " + n(p->vcl_initial_alt_cpb_removal_delay[i]));
          pitemAlt.add("vcl_initial_alt_cpb_removal_offset[" + n(i) + "] = " + n(p->vcl_initial_alt_cpb_removal_offset[i]));
        }
      }
    }
  }

  void SyntaxWriter::createPicTiming(std::shared_ptr<HEVC::PicTiming> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    std::shared_ptr<HEVC::SPS> psps;
    if(m_spsMap.size())
      psps = m_spsMap.begin() -> second;
    if(!psps)
      return;

    if(psps->vui_parameters.frame_field_info_present_flag)
    {
      SyntaxNode &pitemField = parent.add("if( frame_field_info_present_flag )");
      pitemField.add("pic_struct = " + n(p->pic_struct));
      pitemField.add("source_scan_type = " + n(p->source_scan_type));
      pitemField.add("duplicate_flag = " + n(p->duplicate_flag));
    }

    bool CpbDpbDelaysPresentFlag = psps->vui_parameters.hrd_parameters.nal_hrd_parameters_present_flag ||
                                   psps->vui_parameters.hrd_parameters.vcl_hrd_parameters_present_flag;

    if(CpbDpbDelaysPresentFlag)
    {
      SyntaxNode &pitemDpb = parent.add("if( CpbDpbDelaysPresentFlag )");
      pitemDpb.add("au_cpb_removal_delay_minus1 = " + n(p->au_cpb_removal_delay_minus1));
      pitemDpb.add("pic_dpb_output_delay = " + n(p->pic_dpb_output_delay));

      if(psps->vui_parameters.hrd_parameters.sub_pic_hrd_params_present_flag)
      {
        SyntaxNode &pitemDu = pitemDpb.add("if( sub_pic_hrd_params_present_flag )");
        pitemDu.add("pic_dpb_output_du_delay = " + n(p->pic_dpb_output_du_delay));
      }

      if(psps->vui_parameters.hrd_parameters.sub_pic_hrd_params_present_flag &&
         psps->vui_parameters.hrd_parameters.sub_pic_cpb_params_in_pic_timing_sei_flag)
      {
        SyntaxNode &pitemIf = pitemDpb.add("if( sub_pic_cpb_params_in_pic_timing_sei_flag )");
        pitemIf.add("num_decoding_units_minus1 = " + n(p->num_decoding_units_minus1));
        pitemIf.add("du_common_cpb_removal_delay_flag = " + n(p->du_common_cpb_removal_delay_flag));

        if(p->du_common_cpb_removal_delay_flag)
        {
          SyntaxNode &pitemDuComm = pitemIf.add("if( du_common_cpb_removal_delay_flag )");
          pitemDuComm.add("du_common_cpb_removal_delay_increment_minus1 = " + n(p->du_common_cpb_removal_delay_increment_minus1));
        }

        SyntaxNode &ploop = pitemIf.add("for( i = 0; i <= num_decoding_units_minus1; i++ )");
        for(std::size_t i = 0; i <= p->num_decoding_units_minus1; i++)
        {
          ploop.add("num_nalus_in_du_minus1[" + n(i) + "] = " + n(p->num_nalus_in_du_minus1[i]));

          if(!p->du_common_cpb_removal_delay_flag && i < p->num_decoding_units_minus1)
          {
            SyntaxNode &pitemDuComm = ploop.add("if( !du_common_cpb_removal_delay_flag && i < num_decoding_units_minus1 )");
            pitemDuComm.add("du_cpb_removal_delay_increment_minus1[" + n(i) + "] = " + n(p->du_cpb_removal_delay_increment_minus1[i]));
          }
        }
      }
    }
  }

  void SyntaxWriter::createRecoveryPoint(std::shared_ptr<HEVC::RecoveryPoint> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("recovery_poc_cnt = " + n(p->recovery_poc_cnt));
    parent.add("exact_match_flag = " + n(p->exact_match_flag));
    parent.add("broken_link_flag = " + n(p->broken_link_flag));
  }

  void SyntaxWriter::createToneMapping(std::shared_ptr<HEVC::ToneMapping> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("tone_map_id = " + n(p->tone_map_id));
    parent.add("tone_map_cancel_flag = " + n(p->tone_map_cancel_flag));

    if(!p->tone_map_cancel_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( !tone_map_cancel_flag )");
      pitemIf.add("tone_map_persistence_flag = " + n(p->tone_map_persistence_flag));
      pitemIf.add("coded_data_bit_depth = " + n(p->coded_data_bit_depth));
      pitemIf.add("target_bit_depth = " + n(p->target_bit_depth));
      pitemIf.add("tone_map_model_id = " + n(p->tone_map_model_id));

      if(p->tone_map_model_id == 0)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( tone_map_model_id == 0 )");
        pitemSecond.add("min_value = " + n(p->min_value));
        pitemSecond.add("max_value = " + n(p->max_value));
      }
      else if(p->tone_map_model_id == 1)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( tone_map_model_id == 1 )");
        pitemSecond.add("sigmoid_midpoint = " + n(p->sigmoid_midpoint));
        pitemSecond.add("sigmoid_width = " + n(p->sigmoid_width));
      }
      else if(p->tone_map_model_id == 2)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( tone_map_model_id == 2 )");
        SyntaxNode &ploop = pitemSecond.add("for( i = 0; i < ( 1 << target_bit_depth ); i++ )");
        for(std::size_t i = 0; i < (std::size_t)(1 << p->target_bit_depth); i++)
          ploop.add("start_of_coded_interval[" + n(i) + "] = " + n(p->start_of_coded_interval[i]));
      }
      else if(p->tone_map_model_id == 3)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( tone_map_model_id == 3 )");
        pitemSecond.add("num_pivots = " + n(p->num_pivots));
        SyntaxNode &ploop = pitemSecond.add("for( i = 0; i < num_pivots; i++ )");
        for(std::size_t i = 0; i < p->num_pivots; i++)
        {
          ploop.add("coded_pivot_value[" + n(i) + "] = " + n(p->coded_pivot_value[i]));
          ploop.add("target_pivot_value[" + n(i) + "] = " + n(p->target_pivot_value[i]));
        }
      }
      else if(p->tone_map_model_id == 4)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( tone_map_model_id == 4 )");
        pitemSecond.add("camera_iso_speed_idc = " + n(p->camera_iso_speed_idc));
        if(p->camera_iso_speed_idc == 255)
        {
          SyntaxNode &pitemThird = pitemSecond.add("if( camera_iso_speed_idc == EXTENDED_ISO )");
          pitemThird.add("camera_iso_speed_value = " + n(p->camera_iso_speed_value));
        }

        pitemSecond.add("exposure_index_idc = " + n(p->exposure_index_idc));
        if(p->exposure_index_idc == 255)
        {
          SyntaxNode &pitemThird = pitemSecond.add("if( exposure_index_idc == EXTENDED_ISO )");
          pitemThird.add("exposure_index_value = " + n(p->exposure_index_value));
        }

        pitemSecond.add("exposure_compensation_value_sign_flag = " + n(p->exposure_compensation_value_sign_flag));
        pitemSecond.add("exposure_compensation_value_numerator = " + n(p->exposure_compensation_value_numerator));
        pitemSecond.add("exposure_compensation_value_denom_idc = " + n(p->exposure_compensation_value_denom_idc));
        pitemSecond.add("ref_screen_luminance_white = " + n(p->ref_screen_luminance_white));
        pitemSecond.add("extended_range_white_level = " + n(p->extended_range_white_level));
        pitemSecond.add("nominal_black_level_code_value = " + n(p->nominal_black_level_code_value));
        pitemSecond.add("nominal_white_level_code_value = " + n(p->nominal_white_level_code_value));
        pitemSecond.add("extended_white_level_code_value = " + n(p->extended_white_level_code_value));
      }
    }
  }

  void SyntaxWriter::createFramePacking(std::shared_ptr<HEVC::FramePacking> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("frame_packing_arrangement_id = " + n(p->frame_packing_arrangement_id));
    parent.add("frame_packing_arrangement_cancel_flag = " + n(p->frame_packing_arrangement_cancel_flag));

    if(!p->frame_packing_arrangement_cancel_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( !frame_packing_arrangement_cancel_flag )");
      pitemIf.add("frame_packing_arrangement_type = " + n(p->frame_packing_arrangement_type));
      pitemIf.add("quincunx_sampling_flag = " + n(p->quincunx_sampling_flag));
      pitemIf.add("content_interpretation_type = " + n(p->content_interpretation_type));
      pitemIf.add("spatial_flipping_flag = " + n(p->spatial_flipping_flag));
      pitemIf.add("frame0_flipped_flag = " + n(p->frame0_flipped_flag));
      pitemIf.add("field_views_flag = " + n(p->field_views_flag));
      pitemIf.add("current_frame_is_frame0_flag = " + n(p->current_frame_is_frame0_flag));
      pitemIf.add("frame0_self_contained_flag = " + n(p->frame0_self_contained_flag));
      pitemIf.add("frame1_self_contained_flag = " + n(p->frame1_self_contained_flag));

      if(!p->quincunx_sampling_flag && p->frame_packing_arrangement_type != 5)
      {
        SyntaxNode &pitemGrid = pitemIf.add("if( !quincunx_sampling_flag && frame_packing_arrangement_type != 5 )");
        pitemGrid.add("frame0_grid_position_x = " + n(p->frame0_grid_position_x));
        pitemGrid.add("frame0_grid_position_y = " + n(p->frame0_grid_position_y));
        pitemGrid.add("frame1_grid_position_x = " + n(p->frame1_grid_position_x));
        pitemGrid.add("frame1_grid_position_y = " + n(p->frame1_grid_position_y));
      }

      pitemIf.add("frame_packing_arrangement_reserved_byte = " + n(p->frame_packing_arrangement_reserved_byte));
      pitemIf.add("frame_packing_arrangement_persistence_flag = " + n(p->frame_packing_arrangement_persistence_flag));
    }

    parent.add("upsampled_aspect_ratio_flag = " + n(p->upsampled_aspect_ratio_flag));
  }

  void SyntaxWriter::createDisplayOrientation(std::shared_ptr<HEVC::DisplayOrientation> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("display_orientation_cancel_flag = " + n(p->display_orientation_cancel_flag));
    if(!p->display_orientation_cancel_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( !display_orientation_cancel_flag )");
      pitemIf.add("hor_flip = " + n(p->hor_flip));
      pitemIf.add("ver_flip = " + n(p->ver_flip));
      pitemIf.add("anticlockwise_rotation = " + n(p->anticlockwise_rotation));
      pitemIf.add("display_orientation_persistence_flag = " + n(p->display_orientation_persistence_flag));
    }
  }

  void SyntaxWriter::createSOPDescription(std::shared_ptr<HEVC::SOPDescription> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("sop_seq_parameter_set_id = " + n(p->sop_seq_parameter_set_id));
    parent.add("num_entries_in_sop_minus1 = " + n(p->num_entries_in_sop_minus1));

    SyntaxNode &ploop = parent.add("for( i = 0; i <= num_entries_in_sop_minus1; i++ )");
    for(std::size_t i = 0; i <= p->num_entries_in_sop_minus1; i++)
    {
      ploop.add("sop_vcl_nut[" + n(i) + "] = " + n(p->sop_vcl_nut[i]));
      ploop.add("sop_temporal_id[" + n(i) + "] = " + n(p->sop_temporal_id[i]));

      if(p->sop_vcl_nut[i] != 19 && p->sop_vcl_nut[i] != 20)
      {
        SyntaxNode &pitemIf = ploop.add("if( sop_vcl_nut[ i ] != IDR_W_RADL(19) && sop_vcl_nut[ i ] != IDR_N_LP(20) )");
        pitemIf.add("sop_short_term_rps_idx[" + n(i) + "] = " + n(p->sop_short_term_rps_idx[i]));
      }

      if(i > 0)
      {
        SyntaxNode &pitemIf = ploop.add("if( i > 0 )");
        pitemIf.add("sop_poc_delta[" + n(i) + "] = " + n(p->sop_poc_delta[i]));
      }
    }
  }

  void SyntaxWriter::createActiveParameterSets(std::shared_ptr<HEVC::ActiveParameterSets> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("active_video_parameter_set_id = " + n(p->active_video_parameter_set_id));
    parent.add("self_contained_cvs_flag = " + n(p->self_contained_cvs_flag));
    parent.add("no_parameter_set_update_flag = " + n(p->no_parameter_set_update_flag));
    parent.add("num_sps_ids_minus1 = " + n(p->num_sps_ids_minus1));

    std::string str = "active_seq_parameter_set_id = { ";
    for(uint32_t i = 0; i <= p->num_sps_ids_minus1; i++)
    {
      if(i)
        str += ", ";
      str += n(p->active_seq_parameter_set_id[i]);
    }
    str += " }";
    parent.add(str);
  }

  void SyntaxWriter::createTemporalLevel0Index(std::shared_ptr<HEVC::TemporalLevel0Index> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("temporal_sub_layer_zero_idx = " + n(p->temporal_sub_layer_zero_idx));
    parent.add("irap_pic_id = " + n(p->irap_pic_id));
  }

  void SyntaxWriter::createRegionRefreshInfo(std::shared_ptr<HEVC::RegionRefreshInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("refreshed_region_flag = " + n(p->refreshed_region_flag));
  }

  void SyntaxWriter::createTimeCode(std::shared_ptr<HEVC::TimeCode> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("num_clock_ts = " + n(p->num_clock_ts));

    if(p->num_clock_ts > 0)
    {
      SyntaxNode &ploop = parent.add("for( i = 0; i < num_clock_ts; i++ )");
      for(std::size_t i = 0; i < p->num_clock_ts; i++)
      {
        ploop.add("clock_time_stamp_flag[" + n(i) + "] = " + n(p->clock_time_stamp_flag[i]));

        if(p->clock_time_stamp_flag[i])
        {
          SyntaxNode &pitemIf = ploop.add("if( clock_time_stamp_flag[" + n(i) + "] )");
          pitemIf.add("nuit_field_based_flag[" + n(i) + "] = " + n(p->nuit_field_based_flag[i]));
          pitemIf.add("counting_type[" + n(i) + "] = " + n(p->counting_type[i]));
          pitemIf.add("full_timestamp_flag[" + n(i) + "] = " + n(p->full_timestamp_flag[i]));
          pitemIf.add("discontinuity_flag[" + n(i) + "] = " + n(p->discontinuity_flag[i]));
          pitemIf.add("cnt_dropped_flag[" + n(i) + "] = " + n(p->cnt_dropped_flag[i]));
          pitemIf.add("n_frames[" + n(i) + "] = " + n(p->n_frames[i]));

          if(p->full_timestamp_flag[i])
          {
            SyntaxNode &pitemSecond = pitemIf.add("if( full_timestamp_flag[" + n(i) + "] )");
            pitemSecond.add("seconds_value[" + n(i) + "] = " + n(p->seconds_value[i]));
            pitemSecond.add("minutes_value[" + n(i) + "] = " + n(p->minutes_value[i]));
            pitemSecond.add("hours_value[" + n(i) + "] = " + n(p->hours_value[i]));
          }
          else
          {
            SyntaxNode &pitemSecond = pitemIf.add("if( !full_timestamp_flag[" + n(i) + "] )");
            pitemSecond.add("seconds_flag[" + n(i) + "] = " + n(p->seconds_flag[i]));

            if(p->seconds_flag[i])
            {
              SyntaxNode &pitemSeconds = pitemSecond.add("if( seconds_flag[" + n(i) + "] )");
              pitemSeconds.add("seconds_value[" + n(i) + "] = " + n(p->seconds_value[i]));
              pitemSeconds.add("minutes_flag[" + n(i) + "] = " + n(p->minutes_flag[i]));

              if(p->minutes_flag[i])
              {
                SyntaxNode &pitemMinuts = pitemSeconds.add("if( minutes_flag[" + n(i) + "] )");
                pitemMinuts.add("minutes_value[" + n(i) + "] = " + n(p->minutes_value[i]));
                pitemMinuts.add("hours_flag[" + n(i) + "] = " + n(p->hours_flag[i]));

                if(p->hours_flag[i])
                {
                  SyntaxNode &pitemHours = pitemMinuts.add("if( hours_flag[" + n(i) + "] )");
                  pitemHours.add("hours_value[" + n(i) + "] = " + n(p->hours_value[i]));
                }
              }
            }
          }

          pitemIf.add("time_offset_length[" + n(i) + "] = " + n(p->time_offset_length[i]));
          if(p->time_offset_length[i])
          {
            SyntaxNode &pitemSecond = pitemIf.add("if( time_offset_length[" + n(i) + "] )");
            pitemSecond.add("time_offset_value[" + n(i) + "] = " + n(p->time_offset_value[i]));
          }
        }
      }
    }
  }

  void SyntaxWriter::createMasteringDisplayInfo(std::shared_ptr<HEVC::MasteringDisplayInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    SyntaxNode &pitemLoop = parent.add("for( i = 0; i < 3; i++ )");
    for(uint32_t i = 0; i < 3; i++)
    {
      pitemLoop.add("display_primary_x[" + n(i) + "] = " + n(p->display_primary_x[i]));
      pitemLoop.add("display_primary_y[" + n(i) + "] = " + n(p->display_primary_y[i]));
    }

    parent.add("white_point_x = " + n(p->white_point_x));
    parent.add("white_point_y = " + n(p->white_point_y));
    parent.add("max_display_mastering_luminance = " + n(p->max_display_mastering_luminance));
    parent.add("min_display_mastering_luminance = " + n(p->min_display_mastering_luminance));
  }

  void SyntaxWriter::createSegmRectFramePacking(std::shared_ptr<HEVC::SegmRectFramePacking> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("segmented_rect_frame_packing_arrangement_cancel_flag = " + n(p->segmented_rect_frame_packing_arrangement_cancel_flag));

    if(!p->segmented_rect_frame_packing_arrangement_cancel_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( !segmented_rect_frame_packing_arrangement_cancel_flag )");
      pitemIf.add("segmented_rect_content_interpretation_type = " + n(p->segmented_rect_content_interpretation_type));
      pitemIf.add("segmented_rect_frame_packing_arrangement_persistence = " + n(p->segmented_rect_frame_packing_arrangement_persistence));
    }
  }

  void SyntaxWriter::createKneeFunctionInfo(std::shared_ptr<HEVC::KneeFunctionInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("knee_function_id = " + n(p->knee_function_id));
    parent.add("knee_function_cancel_flag = " + n(p->knee_function_cancel_flag));

    if(!p->knee_function_cancel_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( !knee_function_cancel_flag )");
      pitemIf.add("knee_function_persistence_flag = " + n(p->knee_function_persistence_flag));
      pitemIf.add("input_d_range = " + n(p->input_d_range));
      pitemIf.add("input_disp_luminance = " + n(p->input_disp_luminance));
      pitemIf.add("output_d_range = " + n(p->output_d_range));
      pitemIf.add("output_disp_luminance = " + n(p->output_disp_luminance));
      pitemIf.add("num_knee_points_minus1 = " + n(p->num_knee_points_minus1));

      SyntaxNode &ploop = pitemIf.add("for( i = 0; i <= num_knee_points_minus1; i++ )");
      for(std::size_t i = 0; i <= p->num_knee_points_minus1; i++)
      {
        ploop.add("input_knee_point[" + n(i) + "] = " + n(p->input_knee_point[i]));
        ploop.add("output_knee_point[" + n(i) + "] = " + n(p->output_knee_point[i]));
      }
    }
  }

  void SyntaxWriter::createChromaResamplingFilterHint(std::shared_ptr<HEVC::ChromaResamplingFilterHint> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("ver_chroma_filter_idc = " + n(p->ver_chroma_filter_idc));
    parent.add("hor_chroma_filter_idc = " + n(p->hor_chroma_filter_idc));
    parent.add("ver_filtering_field_processing_flag = " + n(p->ver_filtering_field_processing_flag));

    if(p->ver_chroma_filter_idc == 1 || p->hor_chroma_filter_idc == 1)
    {
      SyntaxNode &pitemIf = parent.add("if( ver_chroma_filter_idc == 1 || hor_chroma_filter_idc == 1 )");
      pitemIf.add("target_format_idc = " + n(p->target_format_idc));

      if(p->ver_chroma_filter_idc == 1)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( ver_chroma_filter_idc == 1 )");
        pitemSecond.add("num_vertical_filters = " + n(p->num_vertical_filters));

        if(p->num_vertical_filters)
        {
          SyntaxNode &ploop = pitemSecond.add("for( i = 0; i < num_vertical_filters; i++ )");
          for(std::size_t i = 0; i < p->num_vertical_filters; i++)
          {
            ploop.add("ver_tap_length_minus_1[" + n(i) + "] = " + n(p->ver_tap_length_minus_1[i]));

            std::string str = "ver_filter_coeff[" + n(i) + "] = { ";
            for(std::size_t j = 0; j < p->ver_tap_length_minus_1[i]; j++)
            {
              if(j)
                str += ", ";
              str += n(p->ver_filter_coeff[i][j]);
            }
            str += " }";
            ploop.add(str);
          }
        }
      }

      if(p->hor_chroma_filter_idc == 1)
      {
        SyntaxNode &pitemSecond = pitemIf.add("if( hor_chroma_filter_idc == 1 )");
        pitemSecond.add("num_horizontal_filters = " + n(p->num_horizontal_filters));

        if(p->num_horizontal_filters)
        {
          SyntaxNode &ploop = pitemSecond.add("for( i = 0; i < num_horizontal_filters; i++ )");
          for(std::size_t i = 0; i < p->num_horizontal_filters; i++)
          {
            ploop.add("hor_tap_length_minus_1[" + n(i) + "] = " + n(p->hor_tap_length_minus_1[i]));

            std::string str = "hor_filter_coeff[" + n(i) + "] = { ";
            for(std::size_t j = 0; j < p->hor_tap_length_minus_1[i]; j++)
            {
              if(j)
                str += ", ";
              str += n(p->hor_filter_coeff[i][j]);
            }
            str += " }";
            ploop.add(str);
          }
        }
      }
    }
  }

  void SyntaxWriter::createColourRemappingInfo(std::shared_ptr<HEVC::ColourRemappingInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("colour_remap_id = " + n(p->colour_remap_id));
    parent.add("colour_remap_cancel_flag = " + n(p->colour_remap_cancel_flag));

    if(!p->colour_remap_cancel_flag)
    {
      SyntaxNode &pitemIf = parent.add("if( !colour_remap_cancel_flag )");
      pitemIf.add("colour_remap_persistence_flag = " + n(p->colour_remap_persistence_flag));
      pitemIf.add("colour_remap_video_signal_info_present_flag = " + n(p->colour_remap_video_signal_info_present_flag));

      if(p->colour_remap_video_signal_info_present_flag)
      {
        SyntaxNode &pitemIfSecond = pitemIf.add("if( colour_remap_video_signal_info_present_flag )");
        pitemIfSecond.add("colour_remap_full_range_flag = " + n(p->colour_remap_full_range_flag));
        pitemIfSecond.add("colour_remap_primaries = " + n(p->colour_remap_primaries));
        pitemIfSecond.add("colour_remap_transfer_function = " + n(p->colour_remap_transfer_function));
        pitemIfSecond.add("colour_remap_matrix_coefficients = " + n(p->colour_remap_matrix_coefficients));
      }

      pitemIf.add("colour_remap_input_bit_depth = " + n(p->colour_remap_input_bit_depth));
      pitemIf.add("colour_remap_bit_depth = " + n(p->colour_remap_bit_depth));

      SyntaxNode &ploop = pitemIf.add("for( i = 0; i < 3; i++ )");
      for(std::size_t i = 0; i < 3; i++)
      {
        ploop.add("pre_lut_num_val_minus1[" + n(i) + "] = " + n(p->pre_lut_num_val_minus1[i]));

        if(p->pre_lut_num_val_minus1[i] > 0)
        {
          SyntaxNode &pitemIfSecond = ploop.add("if( pre_lut_num_val_minus1[" + n(i) + "] > 0 )");
          SyntaxNode &ploopSecond = pitemIfSecond.add("for( j = 0; j <= pre_lut_num_val_minus1[" + n(i) + "]; j++ )");

          for(std::size_t j = 0; j <= p->pre_lut_num_val_minus1[i]; j++)
          {
            ploopSecond.add("pre_lut_coded_value[" + n(i) + "][" + n(j) + "] = " + n(p->pre_lut_coded_value[i][j]));
            ploopSecond.add("pre_lut_target_value[" + n(i) + "][" + n(j) + "] = " + n(p->pre_lut_target_value[i][j]));
          }
        }
      }

      pitemIf.add("colour_remap_matrix_present_flag = " + n(p->colour_remap_matrix_present_flag));

      if(p->colour_remap_matrix_present_flag)
      {
        SyntaxNode &pitemIfSecond = pitemIf.add("if( colour_remap_matrix_present_flag )");
        pitemIfSecond.add("log2_matrix_denom = " + n(p->log2_matrix_denom));

        SyntaxNode &ploop2 = pitemIfSecond.add("for( i = 0; i < 3; i++ )");
        for(std::size_t i = 0; i < 3; i++)
        {
          std::string str = "colour_remap_coeffs[" + n(i) + "] = { ";
          for(std::size_t j = 0; j < 3; j++)
          {
            if(j)
              str += ", ";
            str += n(p->colour_remap_coeffs[i][j]);
          }
          str += " }";
          ploop2.add(str);
        }
      }

      SyntaxNode &ploop3 = pitemIf.add("for( i = 0; i < 3; i++ )");
      for(std::size_t i = 0; i < 3; i++)
      {
        ploop3.add("post_lut_num_val_minus1[" + n(i) + "] = " + n(p->post_lut_num_val_minus1[i]));

        if(p->post_lut_num_val_minus1[i] > 0)
        {
          SyntaxNode &pitemIfSecond = ploop3.add("if( post_lut_num_val_minus1[" + n(i) + "] > 0 )");
          SyntaxNode &ploopSecond = pitemIfSecond.add("for( j = 0; j <= post_lut_num_val_minus1[" + n(i) + "]; j++ )");

          for(std::size_t j = 0; j <= p->post_lut_num_val_minus1[i]; j++)
          {
            ploopSecond.add("post_lut_coded_value[" + n(i) + "][" + n(j) + "] = " + n(p->post_lut_coded_value[i][j]));
            ploopSecond.add("post_lut_target_value[" + n(i) + "][" + n(j) + "] = " + n(p->post_lut_target_value[i][j]));
          }
        }
      }
    }
  }

  void SyntaxWriter::createContentLightLevelInfo(std::shared_ptr<HEVC::ContentLightLevelInfo> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("max_content_light_level = " + n(p->max_content_light_level));
    parent.add("max_pic_average_light_level = " + n(p->max_pic_average_light_level));
  }

  void SyntaxWriter::createAlternativeTransferCharacteristics(std::shared_ptr<HEVC::AlternativeTransferCharacteristics> p, SyntaxNode &parent)
  {
    if(!p)
      return;
    parent.add("alternative_transfer_characteristics = " + n(p->alternative_transfer_characteristics));
  }

}
