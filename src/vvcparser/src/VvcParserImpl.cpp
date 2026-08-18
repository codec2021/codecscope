#include "VvcParserImpl.h"

#include <iostream>
#include <stdexcept>
#include <sstream>
#include <cassert>
#include <algorithm>

namespace VVC
{

  namespace
  {
    uint32_t ceilLog2(uint32_t v)
    {
      uint32_t r = 0;
      while((1u << r) < v) r++;
      return r;
    }

    void readRefPicListStruct(BitstreamReader &bs, const SPS &s)
    {
      uint32_t numRefEntries = bs.getGolombU();
      bool ltrpInSliceHeader = false;
      if(s.sps_long_term_ref_pics_flag && numRefEntries > 0)
        ltrpInSliceHeader = bs.getBit();
      for(uint32_t k = 0; k < numRefEntries; k++)
      {
        bool interLayer = false;
        if(s.sps_inter_layer_prediction_enabled_flag)
          interLayer = bs.getBit();
        if(!interLayer)
        {
          bool stRef = true;
          if(s.sps_long_term_ref_pics_flag)
            stRef = bs.getBit();
          if(stRef)
          {
            uint32_t absDeltaPocSt = bs.getGolombU(); // abs_delta_poc_st
            bool readSign;
            if((s.sps_weighted_pred_flag || s.sps_weighted_bipred_flag) && k != 0)
              readSign = (absDeltaPocSt != 0);
            else
              readSign = (absDeltaPocSt + 1 != 0);
            if(readSign)
              bs.getBit(); // strp_entry_sign_flag
          }
          else
          {
            if(!ltrpInSliceHeader)
              bs.getBits(s.sps_log2_max_pic_order_cnt_lsb_minus4 + 4); // rpls_poc_lsb_lt
          }
        }
        else
        {
          bs.getGolombU(); // ilrp_idx
        }
      }
    }
  }

  void VvcParserImpl::addConsumer(Consumer *pconsumer) { m_consumers.push_back(pconsumer); }
  void VvcParserImpl::releaseConsumer(Consumer *pconsumer) { m_consumers.remove(pconsumer); }

  void VvcParserImpl::onWarning(const std::string &warning, const Info *pInfo, WarningType type)
  {
    for(auto itr = m_consumers.begin(); itr != m_consumers.end(); itr++)
      (*itr) -> onWarning(warning, pInfo, type);
  }

  std::size_t VvcParserImpl::process(const uint8_t *pdata, std::size_t size, std::size_t offset)
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
        if(pdata[size - i - 1] == 0) parsed--;
        else break;
      }
    }

    return parsed;
  }

  void VvcParserImpl::processNALUnit(const uint8_t *pdata, std::size_t size, const Parser::Info &info)
  {
    BitstreamReader bs(pdata, size);

    NALHeader header;
    processNALUnitHeader(bs, &header);

    std::shared_ptr<NALUnit> pnalU;

    switch(header.nal_unit_type)
    {
      case NAL_VPS:
      {
        std::shared_ptr<VPS_NAL> p(new VPS_NAL(header));
        processVPS(p, bs);
        pnalU = p;
        m_vpsMap[p->vps.vps_video_parameter_set_id] = p;
        break;
      }
      case NAL_SPS:
      {
        std::shared_ptr<SPS_NAL> p(new SPS_NAL(header));
        processSPS(p, bs);
        pnalU = p;
        m_spsMap[p->sps.sps_seq_parameter_set_id] = p;
        break;
      }
      case NAL_PPS:
      {
        std::shared_ptr<PPS_NAL> p(new PPS_NAL(header));
        processPPS(p, bs, info);
        pnalU = p;
        m_ppsMap[p->pps.pps_pic_parameter_set_id] = p;
        break;
      }
      case NAL_PH:
      {
        std::shared_ptr<PH_NAL> p(new PH_NAL(header));
        processPH(p, bs, info);
        pnalU = p;
        m_lastPH = p;
        break;
      }
      case NAL_TRAIL_NUT:
      case NAL_STSA_NUT:
      case NAL_RADL_NUT:
      case NAL_RASL_NUT:
      case NAL_IDR_W_RADL:
      case NAL_IDR_N_LP:
      case NAL_CRA_NUT:
      case NAL_GDR_NUT:
      {
        std::shared_ptr<Slice_NAL> p(new Slice_NAL(header));
        processSlice(p, bs, info);
        pnalU = p;
        break;
      }
      case NAL_AUD:
      {
        std::shared_ptr<AUD_NAL> p(new AUD_NAL(header));
        processAUD(p, bs);
        pnalU = p;
        break;
      }
      case NAL_PREFIX_SEI:
      case NAL_SUFFIX_SEI:
      {
        std::shared_ptr<SEI_NAL> p(new SEI_NAL(header));
        processSEI(p, bs);
        pnalU = p;
        break;
      }
      default:
        pnalU = std::shared_ptr<NALUnit>(new NALUnit(header));
    }

    for(auto itr = m_consumers.begin(); itr != m_consumers.end(); itr++)
      (*itr) -> onNALUnit(pnalU, &info);
  }

  void VvcParserImpl::processNALUnitHeader(BitstreamReader &bs, NALHeader *header)
  {
    header->forbidden_zero_bit = bs.getBit();
    header->nuh_reserved_zero_bit = bs.getBit();
    header->nuh_layer_id = bs.getBits(6);
    header->nal_unit_type = (NALUnitType)bs.getBits(5);
    header->nuh_temporal_id_plus1 = bs.getBits(3);
  }

  void VvcParserImpl::processProfileTierLevel(ProfileTierLevel &ptl, bool profileTierPresentFlag, uint8_t maxNumSubLayersMinus1, BitstreamReader &bs)
  {
    ptl.toDefault();

    if(profileTierPresentFlag)
    {
      ptl.general_profile_idc = bs.getBits(7);
      ptl.general_tier_flag = bs.getBit();
      ptl.general_level_idc = bs.getBits(8);
      ptl.ptl_frame_only_constraint_flag = bs.getBit();
      ptl.ptl_multilayer_enabled_flag = bs.getBit();

      if(ptl.ptl_multilayer_enabled_flag)
      {
        uint8_t numSubProfilesMinus1 = bs.getBits(8);
        for(int k = 0; k <= numSubProfilesMinus1; k++)
        {
          ptl.general_sub_profile_idc[k] = bs.getBits(32);
          ptl.general_sub_profile_present_flag[k] = 1;
        }
      }
      else
      {
        // general_constraint_info
        bool gciPresent = bs.getBit();
        if(gciPresent)
        {
          bs.getBit();     // gci_intra_only_constraint_flag
          bs.getBit();     // gci_all_layers_independent_constraint_flag
          bs.getBit();     // gci_one_au_only_constraint_flag
          bs.getBits(4);   // gci_sixteen_minus_max_bitdepth_constraint_idc
          bs.getBits(2);   // gci_three_minus_max_chroma_format_constraint_idc
          for(int i = 0; i < 16; i++) bs.getBit(); // no_mixed ... no_subpic_info
          bs.getBits(2);   // gci_three_minus_max_log2_ctu_size_constraint_idc
          for(int i = 0; i < 44; i++) bs.getBit(); // no_partition_override ... no_virtual_boundaries
          uint8_t numReserved = bs.getBits(8);      // gci_num_reserved_constraint_bytes
          for(int i = 0; i < numReserved; i++) bs.getBits(8);
        }
        bs.byteAlign(); // gci_alignment_zero_bit
      }
    }

    // sublayer level present flags
    for(int i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
      ptl.sublayer_level_present_flag[i] = bs.getBit();

    bs.byteAlign(); // ptl_reserved_zero_bit 直到字节对齐

    for(int i = maxNumSubLayersMinus1 - 1; i >= 0; i--)
    {
      if(ptl.sublayer_level_present_flag[i])
        ptl.sublayer_level_idc[i] = bs.getBits(8);
    }

    if(profileTierPresentFlag)
    {
      ptl.ptl_num_sub_profiles = bs.getBits(8);
      for(int j = 0; j < ptl.ptl_num_sub_profiles; j++)
        bs.getBits(32); // general_sub_profile_idc
    }
  }

  void VvcParserImpl::processVPS(std::shared_ptr<VPS_NAL> p, BitstreamReader &bs)
  {
    VPS &v = p->vps;
    v.toDefault();

    v.vps_video_parameter_set_id = bs.getBits(4);
    v.vps_max_layers_minus1 = bs.getBits(6);
    v.vps_max_sublayers_minus1 = bs.getBits(3);

    if(v.vps_max_layers_minus1 > 0 && v.vps_max_sublayers_minus1 > 0)
      v.vps_all_layers_same_num_sublayers_flag = bs.getBit();

    if(v.vps_max_layers_minus1 > 0)
      v.vps_all_independent_layers_flag = bs.getBit();

    for(int i = 0; i <= v.vps_max_layers_minus1; i++)
    {
      v.vps_layer_id[i] = bs.getBits(6);
      if(i > 0 && !v.vps_all_independent_layers_flag)
        v.vps_independent_layer_flag[i] = bs.getBit();

      if(!v.vps_independent_layer_flag[i])
      {
        for(int j = 0; j < i; j++)
        {
          if(v.vps_independent_layer_flag[j])
          {
            // 简化的直接依赖解析
            v.vps_direct_ref_layer_flag[i][j] = bs.getBit();
          }
        }
        if(v.vps_max_tid_ref_present_flag[i])
          v.vps_max_tid_ref_present_flag[i] = bs.getBit();
      }
    }

    if(v.vps_max_layers_minus1 > 0)
    {
      if(v.vps_all_independent_layers_flag)
        v.vps_each_layer_is_an_ols_flag = bs.getBit();
      if(v.vps_each_layer_is_an_ols_flag)
      {
        if(!v.vps_all_independent_layers_flag)
          v.vps_ols_mode_idc = bs.getBits(2);
        if(v.vps_ols_mode_idc == 2)
        {
          uint8_t numOutputLayerSetsMinus1 = bs.getBits(8);
          v.vps_num_output_layer_sets_minus1 = numOutputLayerSetsMinus1;
          for(int i = 0; i <= numOutputLayerSetsMinus1; i++)
          {
            for(int j = 0; j <= v.vps_max_layers_minus1; j++)
              bs.getBit(); // ols_output_layer_flag
          }
        }
      }
      v.vps_num_ptls_minus1 = bs.getBits(8);
    }

    for(int i = 0; i <= v.vps_num_ptls_minus1; i++)
    {
      bool ptPresent = true;
      if(i > 0)
        ptPresent = bs.getBit();
      if(v.vps_max_sublayers_minus1 > 0 && !v.vps_all_layers_same_num_sublayers_flag)
        bs.getBits(3); // ptl_max_temporal_id
      bs.byteAlign();
      ProfileTierLevel ptl;
      processProfileTierLevel(ptl, ptPresent, v.vps_max_sublayers_minus1, bs);
      v.ptl.push_back(ptl);
    }

    v.vps_extension_flag = bs.getBit();
  }

  void VvcParserImpl::processSPS(std::shared_ptr<SPS_NAL> p, BitstreamReader &bs)
  {
    SPS &s = p->sps;
    s.toDefault();

    s.sps_seq_parameter_set_id = bs.getBits(4);
    s.sps_video_parameter_set_id = bs.getBits(4);
    s.sps_max_sublayers_minus1 = bs.getBits(3);
    s.sps_chroma_format_idc = bs.getBits(2);
    s.sps_log2_ctu_size_minus5 = bs.getBits(2);
    s.sps_ptl_dpb_hrd_params_present_flag = bs.getBit();

    if(s.sps_ptl_dpb_hrd_params_present_flag)
      processProfileTierLevel(s.profile_tier_level, true, s.sps_max_sublayers_minus1, bs);

    s.sps_gdr_enabled_flag = bs.getBit();
    s.sps_ref_pic_resampling_enabled_flag = bs.getBit();
    if(s.sps_ref_pic_resampling_enabled_flag)
      s.sps_res_change_in_clvs_allowed_flag = bs.getBit();

    s.sps_pic_width_max_in_luma_samples = bs.getGolombU();
    s.sps_pic_height_max_in_luma_samples = bs.getGolombU();

    s.sps_conformance_window_flag = bs.getBit();
    if(s.sps_conformance_window_flag)
    {
      s.sps_conf_win_left_offset = bs.getGolombU();
      s.sps_conf_win_right_offset = bs.getGolombU();
      s.sps_conf_win_top_offset = bs.getGolombU();
      s.sps_conf_win_bottom_offset = bs.getGolombU();
    }

    s.sps_subpic_info_present_flag = bs.getBit();
    if(s.sps_subpic_info_present_flag)
    {
      s.sps_num_subpics_minus1 = bs.getGolombU();
      if(s.sps_num_subpics_minus1 > 0)
      {
        s.sps_independent_subpics_flag = bs.getBit();
        bs.getBit(); // sps_subpic_same_size_flag
      }
      uint32_t ctuSize = 1u << (s.sps_log2_ctu_size_minus5 + 5);
      uint32_t maxSubpics = s.sps_num_subpics_minus1 + 1;
      for(uint32_t i = 0; i < maxSubpics; i++)
      {
        if(s.sps_pic_width_max_in_luma_samples > ctuSize)
          s.sps_subpic_ctu_top_left_x.push_back(bs.getBits(ceilLog2((s.sps_pic_width_max_in_luma_samples + ctuSize - 1) / ctuSize)));
        if(s.sps_pic_height_max_in_luma_samples > ctuSize)
          s.sps_subpic_ctu_top_left_y.push_back(bs.getBits(ceilLog2((s.sps_pic_height_max_in_luma_samples + ctuSize - 1) / ctuSize)));
        if(s.sps_pic_width_max_in_luma_samples > ctuSize)
          s.sps_subpic_width_minus1.push_back(bs.getBits(ceilLog2((s.sps_pic_width_max_in_luma_samples + ctuSize - 1) / ctuSize)));
        if(s.sps_pic_height_max_in_luma_samples > ctuSize)
          s.sps_subpic_height_minus1.push_back(bs.getBits(ceilLog2((s.sps_pic_height_max_in_luma_samples + ctuSize - 1) / ctuSize)));
        if(!s.sps_independent_subpics_flag)
        {
          if(i < 64) s.sps_subpic_treated_as_pic_flag[i] = bs.getBit();
          if(i < 64) s.sps_loop_filter_across_subpic_enabled_flag[i] = bs.getBit();
        }
      }

      s.sps_subpic_id_len_minus1 = bs.getGolombU();
      bs.getBit();     // sps_subpic_id_mapping_explicitly_signalled_flag
      // 简化：跳过 subpic id mapping（极少使用）
    }

    s.sps_bitdepth_minus8 = bs.getGolombU();
    s.sps_entropy_coding_sync_enabled_flag = bs.getBit();
    s.sps_entry_point_offsets_present_flag = bs.getBit();
    s.sps_log2_max_pic_order_cnt_lsb_minus4 = bs.getBits(4);
    s.sps_poc_msb_cycle_flag = bs.getBit();
    if(s.sps_poc_msb_cycle_flag)
      s.sps_poc_msb_cycle_len_minus1 = bs.getGolombU();

    s.sps_num_extra_ph_bytes = bs.getBits(2);
    s.sps_num_extra_sh_bytes = bs.getBits(2);

    if(s.sps_ptl_dpb_hrd_params_present_flag)
    {
      // dpb_parameters
      bool subLayerInfoFlag = false;
      if(s.sps_max_sublayers_minus1 > 0)
      {
        s.sps_sublayer_dpb_params_flag = bs.getBit();
        subLayerInfoFlag = s.sps_sublayer_dpb_params_flag;
      }
      int start = subLayerInfoFlag ? 0 : (int)s.sps_max_sublayers_minus1;
      for(int i = start; i <= (int)s.sps_max_sublayers_minus1; i++)
      {
        bs.getGolombU(); // dpb_max_dec_pic_buffering_minus1
        bs.getGolombU(); // dpb_max_num_reorder_pics
        bs.getGolombU(); // dpb_max_latency_increase_plus1
      }
    }

    s.sps_log2_min_luma_coding_block_size_minus2 = bs.getGolombU();
    s.sps_partition_constraints_override_enabled_flag = bs.getBit();
    s.sps_log2_diff_min_qt_min_cb_intra_slice_luma = bs.getGolombU();
    s.sps_max_mtt_hierarchy_depth_intra_slice_luma = bs.getGolombU();
    if(s.sps_max_mtt_hierarchy_depth_intra_slice_luma != 0)
    {
      s.sps_log2_diff_max_bt_min_qt_intra_slice_luma = bs.getGolombU();
      s.sps_log2_diff_max_tt_min_qt_intra_slice_luma = bs.getGolombU();
    }
    if(s.sps_chroma_format_idc != 0)
      s.sps_qtbtt_dual_tree_intra_flag = bs.getBit();
    if(s.sps_qtbtt_dual_tree_intra_flag)
    {
      s.sps_log2_diff_min_qt_min_cb_intra_slice_chroma = bs.getGolombU();
      s.sps_max_mtt_hierarchy_depth_intra_slice_chroma = bs.getGolombU();
      if(s.sps_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
      {
        s.sps_log2_diff_max_bt_min_qt_intra_slice_chroma = bs.getGolombU();
        s.sps_log2_diff_max_tt_min_qt_intra_slice_chroma = bs.getGolombU();
      }
    }
    s.sps_log2_diff_min_qt_min_cb_inter_slice = bs.getGolombU();
    s.sps_max_mtt_hierarchy_depth_inter_slice = bs.getGolombU();
    if(s.sps_max_mtt_hierarchy_depth_inter_slice != 0)
    {
      s.sps_log2_diff_max_bt_min_qt_inter_slice = bs.getGolombU();
      s.sps_log2_diff_max_tt_min_qt_inter_slice = bs.getGolombU();
    }

    s.sps_max_luma_transform_size_64_flag = bs.getBit();
    s.sps_transform_skip_enabled_flag = bs.getBit();
    if(s.sps_transform_skip_enabled_flag)
    {
      s.sps_log2_transform_skip_max_size_minus2 = bs.getGolombU();
      s.sps_bdpcm_enabled_flag = bs.getBit();
    }
    s.sps_mts_enabled_flag = bs.getBit();
    if(s.sps_mts_enabled_flag)
    {
      s.sps_explicit_mts_intra_enabled_flag = bs.getBit();
      s.sps_explicit_mts_inter_enabled_flag = bs.getBit();
    }
    s.sps_lfnst_enabled_flag = bs.getBit();

    if(s.sps_chroma_format_idc != 0)
    {
      s.sps_joint_cbcr_enabled_flag = bs.getBit();
      s.sps_same_qp_table_for_chroma_flag = bs.getBit();
      uint32_t numQpTables = s.sps_same_qp_table_for_chroma_flag ? 1 : (s.sps_joint_cbcr_enabled_flag ? 3 : 2);
      for(uint32_t i = 0; i < numQpTables; i++)
      {
        s.sps_qp_table_start_minus26[i] = bs.getGolombS();
        s.sps_num_points_in_qp_table_minus1[i] = bs.getGolombU();
        for(uint32_t j = 0; j <= s.sps_num_points_in_qp_table_minus1[i]; j++)
        {
          s.sps_delta_qp_in_val_minus1[i].push_back(bs.getGolombU());
          s.sps_delta_qp_diff_val[i].push_back(bs.getGolombU());
        }
      }
    }

    s.sps_sao_enabled_flag = bs.getBit();
    s.sps_alf_enabled_flag = bs.getBit();
    if(s.sps_alf_enabled_flag && s.sps_chroma_format_idc != 0)
      s.sps_ccalf_enabled_flag = bs.getBit();
    s.sps_lmcs_enabled_flag = bs.getBit();
    s.sps_weighted_pred_flag = bs.getBit();
    s.sps_weighted_bipred_flag = bs.getBit();
    s.sps_long_term_ref_pics_flag = bs.getBit();
    if(s.sps_video_parameter_set_id > 0)
      s.sps_inter_layer_prediction_enabled_flag = bs.getBit();
    s.sps_idr_rpl_present_flag = bs.getBit();
    s.sps_rpl1_same_as_rpl0_flag = bs.getBit();

    for(int i = 0; i < (s.sps_rpl1_same_as_rpl0_flag ? 1 : 2); i++)
    {
      s.sps_num_ref_pic_lists[i] = bs.getGolombU();
      for(int j = 0; j < s.sps_num_ref_pic_lists[i]; j++)
      {
        // ref_pic_list_struct
        uint32_t numRefEntries = bs.getGolombU();
        bool ltrpInSliceHeader = false;
        if(s.sps_long_term_ref_pics_flag && numRefEntries > 0)
          ltrpInSliceHeader = bs.getBit();
        for(uint32_t k = 0; k < numRefEntries; k++)
        {
          bool interLayer = false;
          if(s.sps_inter_layer_prediction_enabled_flag)
            interLayer = bs.getBit();
          if(!interLayer)
          {
            bool stRef = true; // 默认 true
            if(s.sps_long_term_ref_pics_flag)
              stRef = bs.getBit();
            if(stRef)
            {
              uint32_t absDeltaPocSt = bs.getGolombU(); // abs_delta_poc_st
              bool readSign;
              if((s.sps_weighted_pred_flag || s.sps_weighted_bipred_flag) && k != 0)
                readSign = (absDeltaPocSt != 0);
              else
                readSign = (absDeltaPocSt + 1 != 0);
              if(readSign)
                bs.getBit(); // strp_entry_sign_flag
            }
            else
            {
              if(!ltrpInSliceHeader)
                bs.getBits(s.sps_log2_max_pic_order_cnt_lsb_minus4 + 4); // rpls_poc_lsb_lt
            }
          }
          else
          {
            bs.getGolombU(); // ilrp_idx
          }
        }
      }
    }

    s.sps_ref_wraparound_enabled_flag = bs.getBit();
    s.sps_temporal_mvp_enabled_flag = bs.getBit();
    if(s.sps_temporal_mvp_enabled_flag)
      s.sps_sbtmvp_enabled_flag = bs.getBit();
    s.sps_amvr_enabled_flag = bs.getBit();
    s.sps_bdof_enabled_flag = bs.getBit();
    if(s.sps_bdof_enabled_flag)
      s.sps_bdof_control_present_in_ph_flag = bs.getBit();
    s.sps_smvd_enabled_flag = bs.getBit();
    s.sps_dmvr_enabled_flag = bs.getBit();
    if(s.sps_dmvr_enabled_flag)
      s.sps_dmvr_control_present_in_ph_flag = bs.getBit();
    s.sps_mmvd_enabled_flag = bs.getBit();
    if(s.sps_mmvd_enabled_flag)
      s.sps_mmvd_fullpel_only_enabled_flag = bs.getBit();
    s.sps_six_minus_max_num_merge_cand = bs.getGolombU();
    s.sps_sbt_enabled_flag = bs.getBit();
    s.sps_affine_enabled_flag = bs.getBit();
    if(s.sps_affine_enabled_flag)
      s.sps_affine_type_flag = bs.getBit();
    s.sps_affine_amvr_enabled_flag = bs.getBit();
    s.sps_affine_prof_enabled_flag = bs.getBit();
    if(s.sps_affine_prof_enabled_flag)
      s.sps_prof_control_present_in_ph_flag = bs.getBit();
    s.sps_bcw_enabled_flag = bs.getBit();
    s.sps_ciip_enabled_flag = bs.getBit();
    s.sps_gpm_enabled_flag = bs.getBit();
    s.sps_max_num_merge_cand_minus_max_num_gpm_cand = bs.getGolombU();
    s.sps_log2_parallel_merge_level_minus2 = bs.getGolombU();
    s.sps_isp_enabled_flag = bs.getBit();
    s.sps_mrl_enabled_flag = bs.getBit();
    s.sps_mip_enabled_flag = bs.getBit();
    if(s.sps_chroma_format_idc != 0)
    {
      s.sps_cclm_enabled_flag = bs.getBit();
      if(s.sps_cclm_enabled_flag)
      {
        s.sps_chroma_horizontal_collocated_flag = bs.getBit();
        s.sps_chroma_vertical_collocated_flag = bs.getBit();
      }
    }
    s.sps_palette_enabled_flag = bs.getBit();
    if(s.sps_chroma_format_idc == 3)
      s.sps_act_enabled_flag = bs.getBit();
    s.sps_min_qp_prime_ts = bs.getGolombU();
    s.sps_ibc_enabled_flag = bs.getBit();
    s.sps_ladf_enabled_flag = bs.getBit();
    s.sps_explicit_scaling_list_enabled_flag = bs.getBit();
    if(s.sps_lfnst_enabled_flag && s.sps_explicit_scaling_list_enabled_flag)
      bs.getBit(); // sps_scaling_matrix_for_lfnst_disabled_flag
    if(s.sps_act_enabled_flag && s.sps_explicit_scaling_list_enabled_flag)
    {
      bs.getBit(); // sps_scaling_matrix_for_alternative_colour_space_disabled_flag
      bs.getBit(); // sps_scaling_matrix_designated_colour_space_flag
    }
    s.sps_dep_quant_enabled_flag = bs.getBit();
    s.sps_sign_data_hiding_enabled_flag = bs.getBit();
    s.sps_virtual_boundaries_enabled_flag = bs.getBit();
    if(s.sps_virtual_boundaries_enabled_flag)
    {
      s.sps_virtual_boundaries_present_flag = bs.getBit();
      if(s.sps_virtual_boundaries_present_flag)
      {
        s.sps_num_ver_virtual_boundaries = bs.getGolombU();
        for(uint32_t i = 0; i < s.sps_num_ver_virtual_boundaries; i++)
          s.sps_virtual_boundary_pos_x_minus1.push_back(bs.getGolombU());
        s.sps_num_hor_virtual_boundaries = bs.getGolombU();
        for(uint32_t i = 0; i < s.sps_num_hor_virtual_boundaries; i++)
          s.sps_virtual_boundary_pos_y_minus1.push_back(bs.getGolombU());
      }
    }

    if(s.sps_ptl_dpb_hrd_params_present_flag)
    {
      s.sps_general_hrd_params_present_flag = bs.getBit();
      if(s.sps_general_hrd_params_present_flag)
      {
        // general_hrd_parameters（简化）
        bs.getGolombU(); // num_units_in_tick
        bs.getBits(32);  // time_scale
        bs.getBit();     // general_nal_hrd_params_present_flag
        bs.getBit();     // general_vcl_hrd_params_present_flag
        bs.getBit();     // general_same_pic_timing_in_all_ols_flag
        bs.getGolombU(); // general_du_hrd_params_present_flag (实际是 u(1))
        bs.getBits(8);   // tick_divisor_minus2
        bs.getBits(5);   // bit_rate_scale
        bs.getBits(5);   // cpb_size_scale
        bs.getBits(5);   // cpb_size_du_scale
        bs.getGolombU(); // hrd_cpb_cnt_minus1
      }
    }

    s.sps_field_seq_flag = bs.getBit();
    s.sps_vui_parameters_present_flag = bs.getBit();
    if(s.sps_vui_parameters_present_flag)
    {
      uint32_t vuiPayloadSize = bs.getGolombU();
      s.sps_vui_payload_size_minus1 = vuiPayloadSize;
      // 跳过 VUI payload（字节对齐）
      bs.skipBits((vuiPayloadSize + 1) * 8);
    }
    s.sps_extension_flag = bs.getBit();
  }

  void VvcParserImpl::processPPS(std::shared_ptr<PPS_NAL> p, BitstreamReader &bs, const Parser::Info &info)
  {
    PPS &p_ = p->pps;
    p_.toDefault();

    p_.pps_pic_parameter_set_id = bs.getBits(6);
    p_.pps_seq_parameter_set_id = bs.getBits(4);
    p_.pps_mixed_nalu_types_in_pic_flag = bs.getBit();
    p_.pps_pic_width_in_luma_samples = bs.getGolombU();
    p_.pps_pic_height_in_luma_samples = bs.getGolombU();

    p_.pps_conformance_window_flag = bs.getBit();
    if(p_.pps_conformance_window_flag)
    {
      p_.pps_conf_win_left_offset = bs.getGolombU();
      p_.pps_conf_win_right_offset = bs.getGolombU();
      p_.pps_conf_win_top_offset = bs.getGolombU();
      p_.pps_conf_win_bottom_offset = bs.getGolombU();
    }
    p_.pps_scaling_window_explicit_signalling_flag = bs.getBit();
    p_.pps_output_flag_present_flag = bs.getBit();
    p_.pps_no_pic_partition_flag = bs.getBit();
    p_.pps_subpic_id_mapping_present_flag = bs.getBit();

    if(p_.pps_no_pic_partition_flag)
    {
      p_.pps_num_exp_tile_columns_minus1 = 0;
      p_.pps_num_exp_tile_rows_minus1 = 0;
    }
    else
    {
      p_.pps_log2_ctu_size_minus5 = bs.getBits(2);
      p_.pps_num_exp_tile_columns_minus1 = bs.getGolombU();
      p_.pps_num_exp_tile_rows_minus1 = bs.getGolombU();
      for(uint32_t i = 0; i <= p_.pps_num_exp_tile_columns_minus1; i++)
        p_.pps_tile_column_width_minus1.push_back(bs.getGolombU());
      for(uint32_t i = 0; i <= p_.pps_num_exp_tile_rows_minus1; i++)
        p_.pps_tile_row_height_minus1.push_back(bs.getGolombU());
      p_.pps_loop_filter_across_tiles_enabled_flag = bs.getBit();
      p_.pps_rect_slice_flag = bs.getBit();
      if(p_.pps_rect_slice_flag)
        p_.pps_single_slice_per_subpic_flag = bs.getBit();
      if(p_.pps_rect_slice_flag && !p_.pps_single_slice_per_subpic_flag)
      {
        p_.pps_num_slices_in_pic_minus1 = bs.getGolombU();
        if(p_.pps_num_slices_in_pic_minus1 > 1)
          p_.pps_tile_idx_delta_present_flag = bs.getBit();
        for(uint32_t i = 0; i < p_.pps_num_slices_in_pic_minus1; i++)
        {
          p_.pps_slice_width_in_tiles_minus1.push_back(bs.getGolombU());
          p_.pps_slice_height_in_tiles_minus1.push_back(bs.getGolombU());
        }
      }
      if(!p_.pps_rect_slice_flag || p_.pps_single_slice_per_subpic_flag || p_.pps_num_slices_in_pic_minus1 > 0)
        p_.pps_loop_filter_across_slices_enabled_flag = bs.getBit();
    }

    p_.pps_cabac_init_present_flag = bs.getBit();
    p_.pps_num_ref_idx_default_active_minus1[0] = bs.getGolombU();
    p_.pps_num_ref_idx_default_active_minus1[1] = bs.getGolombU();
    p_.pps_rpl1_idx_present_flag = bs.getBit();
    p_.pps_weighted_pred_flag = bs.getBit();
    p_.pps_weighted_bipred_flag = bs.getBit();
    {
      uint8_t ppsRefWraparound = bs.getBit(); // pps_ref_wraparound_enabled_flag
      if(ppsRefWraparound)
        bs.getGolombU(); // pps_pic_width_minus_wraparound_offset
    }
    p_.pps_init_qp_minus26 = bs.getGolombS();
    p_.pps_cu_qp_delta_enabled_flag = bs.getBit();
    p_.pps_chroma_tool_offsets_present_flag = bs.getBit();
    if(p_.pps_chroma_tool_offsets_present_flag)
    {
      p_.pps_cb_qp_offset = bs.getGolombS();
      p_.pps_cr_qp_offset = bs.getGolombS();
      p_.pps_joint_cbcr_qp_offset_present_flag = bs.getBit();
      if(p_.pps_joint_cbcr_qp_offset_present_flag)
        p_.pps_joint_cbcr_qp_offset_value = bs.getGolombS();
      p_.pps_slice_chroma_qp_offsets_present_flag = bs.getBit();
      p_.pps_cu_chroma_qp_offset_list_enabled_flag = bs.getBit();
      if(p_.pps_cu_chroma_qp_offset_list_enabled_flag)
      {
        p_.pps_chroma_qp_offset_list_len_minus1 = bs.getGolombU();
        for(uint32_t i = 0; i <= p_.pps_chroma_qp_offset_list_len_minus1; i++)
        {
          p_.pps_cb_qp_offset_list.push_back(bs.getGolombS());
          p_.pps_cr_qp_offset_list.push_back(bs.getGolombS());
          if(p_.pps_joint_cbcr_qp_offset_present_flag)
            p_.pps_joint_cbcr_qp_offset_list.push_back(bs.getGolombS());
        }
      }
    }
    p_.pps_deblocking_filter_control_present_flag = bs.getBit();
    if(p_.pps_deblocking_filter_control_present_flag)
    {
      p_.pps_deblocking_filter_override_enabled_flag = bs.getBit();
      p_.pps_deblocking_filter_disabled_flag = bs.getBit();
      if(!p_.pps_deblocking_filter_disabled_flag)
      {
        p_.pps_dbf_info_in_ph_flag = bs.getBit();
        p_.pps_luma_beta_offset_div2 = bs.getGolombS();
        p_.pps_luma_tc_offset_div2 = bs.getGolombS();
        if(p_.pps_chroma_tool_offsets_present_flag)
        {
          p_.pps_cb_beta_offset_div2 = bs.getGolombS();
          p_.pps_cb_tc_offset_div2 = bs.getGolombS();
          p_.pps_cr_beta_offset_div2 = bs.getGolombS();
          p_.pps_cr_tc_offset_div2 = bs.getGolombS();
        }
      }
    }
    p_.pps_rpl_info_in_ph_flag = bs.getBit();
    p_.pps_sao_info_in_ph_flag = bs.getBit();
    p_.pps_alf_info_in_ph_flag = bs.getBit();
    p_.pps_wp_info_in_ph_flag = bs.getBit();
    p_.pps_qp_delta_info_in_ph_flag = bs.getBit();
    p_.pps_picture_header_extension_present_flag = bs.getBit();
    p_.pps_slice_header_extension_present_flag = bs.getBit();
    p_.pps_extension_flag = bs.getBit();
  }

  void VvcParserImpl::processPictureHeader(PH &ph, BitstreamReader &bs, const Parser::Info &info)
  {
    (void)info;
    ph.toDefault();

    ph.ph_gdr_or_irap_pic_flag = bs.getBit();
    ph.ph_non_ref_pic_flag = bs.getBit();
    if(ph.ph_gdr_or_irap_pic_flag)
      ph.ph_gdr_pic_flag = bs.getBit();
    ph.ph_inter_slice_allowed_flag = bs.getBit();
    if(ph.ph_inter_slice_allowed_flag)
      ph.ph_intra_slice_allowed_flag = bs.getBit();
    else
      ph.ph_intra_slice_allowed_flag = 1; // inter 不允许时隐含为 1
    ph.ph_pic_parameter_set_id = bs.getGolombU();

    std::shared_ptr<PPS_NAL> ppps = m_ppsMap[ph.ph_pic_parameter_set_id];
    std::shared_ptr<SPS_NAL> psps;
    if(ppps)
      psps = m_spsMap[ppps->pps.pps_seq_parameter_set_id];

    uint32_t pocBits = 4;
    uint8_t spsPocMsbCycleFlag = 0;
    uint32_t spsPocMsbLen = 0;
    uint8_t spsExtraPhBitsBytes = 0;
    if(psps)
    {
      pocBits = psps->sps.sps_log2_max_pic_order_cnt_lsb_minus4 + 4;
      spsPocMsbCycleFlag = psps->sps.sps_poc_msb_cycle_flag;
      spsPocMsbLen = psps->sps.sps_poc_msb_cycle_len_minus1 + 1;
      spsExtraPhBitsBytes = psps->sps.sps_num_extra_ph_bytes;
    }
    ph.ph_pic_order_cnt_lsb = bs.getBits(pocBits);

    if(ph.ph_gdr_pic_flag)
      ph.ph_recovery_poc_cnt = bs.getGolombU();

    for(uint32_t i = 0; i < spsExtraPhBitsBytes * 8; i++)
      bs.getBit();

    if(spsPocMsbCycleFlag)
    {
      ph.ph_poc_msb_cycle_present_flag = bs.getBit();
      if(ph.ph_poc_msb_cycle_present_flag)
        ph.ph_poc_msb_cycle_val = bs.getBits(spsPocMsbLen);
    }

    uint8_t spsAlf = psps ? psps->sps.sps_alf_enabled_flag : 0;
    uint8_t ppsAlfInPh = ppps ? ppps->pps.pps_alf_info_in_ph_flag : 0;
    uint8_t chromaIdc = psps ? psps->sps.sps_chroma_format_idc : 0;
    uint8_t spsLmcs = psps ? psps->sps.sps_lmcs_enabled_flag : 0;
    uint8_t spsExplicitSl = psps ? psps->sps.sps_explicit_scaling_list_enabled_flag : 0;

    if(spsAlf && ppsAlfInPh)
    {
      ph.ph_alf_enabled_flag = bs.getBit();
      if(ph.ph_alf_enabled_flag)
      {
        ph.ph_num_alf_aps_ids_luma = bs.getBits(3);
        for(uint32_t i = 0; i < ph.ph_num_alf_aps_ids_luma; i++)
          ph.ph_alf_aps_id_luma.push_back(bs.getBits(3));
        if(chromaIdc != 0)
        {
          ph.ph_alf_cb_flag = bs.getBit();
          ph.ph_alf_cr_flag = bs.getBit();
          if(ph.ph_alf_cb_flag || ph.ph_alf_cr_flag)
            ph.ph_alf_aps_id_chroma = bs.getBits(3);
        }
      }
    }

    if(spsLmcs)
    {
      ph.ph_lmcs_enabled_flag = bs.getBit();
      if(ph.ph_lmcs_enabled_flag)
      {
        ph.ph_lmcs_aps_id = bs.getBits(2);
        if(chromaIdc != 0)
          ph.ph_chroma_residual_scale_flag = bs.getBit();
      }
    }

    if(spsExplicitSl)
    {
      ph.ph_explicit_scaling_list_enabled_flag = bs.getBit();
      if(ph.ph_explicit_scaling_list_enabled_flag)
        ph.ph_scaling_list_aps_id = bs.getBits(3);
    }

    uint8_t spsVB = psps ? psps->sps.sps_virtual_boundaries_enabled_flag : 0;
    uint8_t spsVBPresent = psps ? psps->sps.sps_virtual_boundaries_present_flag : 0;
    if(spsVB && !spsVBPresent)
    {
      ph.ph_virtual_boundaries_present_flag = bs.getBit();
      if(ph.ph_virtual_boundaries_present_flag)
      {
        uint32_t picW = psps ? psps->sps.sps_pic_width_max_in_luma_samples : 0;
        uint32_t picH = psps ? psps->sps.sps_pic_height_max_in_luma_samples : 0;
        uint32_t xBits = ceilLog2(picW / 8);
        uint32_t yBits = ceilLog2(picH / 8);
        ph.ph_num_ver_virtual_boundaries = bs.getGolombU();
        for(uint32_t i = 0; i < ph.ph_num_ver_virtual_boundaries; i++)
          ph.ph_virtual_boundary_pos_x_minus1.push_back(bs.getBits(xBits));
        ph.ph_num_hor_virtual_boundaries = bs.getGolombU();
        for(uint32_t i = 0; i < ph.ph_num_hor_virtual_boundaries; i++)
          ph.ph_virtual_boundary_pos_y_minus1.push_back(bs.getBits(yBits));
      }
    }

    uint8_t ppsOutputFlag = ppps ? ppps->pps.pps_output_flag_present_flag : 0;
    if(ppsOutputFlag && !ph.ph_non_ref_pic_flag)
      ph.ph_pic_output_flag = bs.getBit();

    uint8_t spsPartitionOverride = psps ? psps->sps.sps_partition_constraints_override_enabled_flag : 0;
    if(spsPartitionOverride)
      ph.ph_partition_constraints_override_flag = bs.getBit();

    if(ph.ph_partition_constraints_override_flag)
    {
      ph.ph_log2_diff_min_qt_min_cb_intra_slice_luma = bs.getGolombU();
      ph.ph_max_mtt_hierarchy_depth_intra_slice_luma = bs.getGolombU();
      if(ph.ph_max_mtt_hierarchy_depth_intra_slice_luma != 0)
      {
        ph.ph_log2_diff_max_bt_min_qt_intra_slice_luma = bs.getGolombU();
        ph.ph_log2_diff_max_tt_min_qt_intra_slice_luma = bs.getGolombU();
      }
      if(chromaIdc != 0)
      {
        ph.ph_log2_diff_min_qt_min_cb_intra_slice_chroma = bs.getGolombU();
        ph.ph_max_mtt_hierarchy_depth_intra_slice_chroma = bs.getGolombU();
        if(ph.ph_max_mtt_hierarchy_depth_intra_slice_chroma != 0)
        {
          ph.ph_log2_diff_max_bt_min_qt_intra_slice_chroma = bs.getGolombU();
          ph.ph_log2_diff_max_tt_min_qt_intra_slice_chroma = bs.getGolombU();
        }
      }
      ph.ph_log2_diff_min_qt_min_cb_inter_slice = bs.getGolombU();
      ph.ph_max_mtt_hierarchy_depth_inter_slice = bs.getGolombU();
      if(ph.ph_max_mtt_hierarchy_depth_inter_slice != 0)
      {
        ph.ph_log2_diff_max_bt_min_qt_inter_slice = bs.getGolombU();
        ph.ph_log2_diff_max_tt_min_qt_inter_slice = bs.getGolombU();
      }
    }

    uint8_t ppsCuQpDelta = ppps ? ppps->pps.pps_cu_qp_delta_enabled_flag : 0;
    uint8_t ppsCuChromaQpOffset = ppps ? ppps->pps.pps_cu_chroma_qp_offset_list_enabled_flag : 0;
    uint8_t spsTemporalMvp = psps ? psps->sps.sps_temporal_mvp_enabled_flag : 0;
    uint8_t spsMmvdFullpelOnly = psps ? psps->sps.sps_mmvd_fullpel_only_enabled_flag : 0;
    uint8_t spsProfControlInPh = psps ? psps->sps.sps_prof_control_present_in_ph_flag : 0;
    uint8_t spsBdofControlInPh = psps ? psps->sps.sps_bdof_control_present_in_ph_flag : 0;
    uint8_t spsDmvrControlInPh = psps ? psps->sps.sps_dmvr_control_present_in_ph_flag : 0;
    uint8_t ppsRplInPh = ppps ? ppps->pps.pps_rpl_info_in_ph_flag : 0;

    if(ph.ph_intra_slice_allowed_flag)
    {
      if(ppsCuQpDelta)
        ph.ph_cu_qp_delta_subdiv_intra_slice = bs.getGolombU();
      if(ppsCuChromaQpOffset)
        ph.ph_cu_chroma_qp_offset_subdiv_intra_slice = bs.getGolombU();
    }
    if(ph.ph_inter_slice_allowed_flag)
    {
      if(ppsCuQpDelta)
        ph.ph_cu_qp_delta_subdiv_inter_slice = bs.getGolombU();
      if(ppsCuChromaQpOffset)
        ph.ph_cu_chroma_qp_offset_subdiv_inter_slice = bs.getGolombU();
      if(spsTemporalMvp)
        ph.ph_temporal_mvp_enabled_flag = bs.getBit();
      if(spsMmvdFullpelOnly)
        ph.ph_mmvd_fullpel_only_flag = bs.getBit();
      bool presenceFlag = !ppsRplInPh; // 简化：pps_rpl_info_in_ph_flag 时视为无 ref 列表
      if(presenceFlag)
      {
        bs.getBit(); // ph_mvd_l1_zero_flag
        if(spsBdofControlInPh)
          bs.getBit(); // ph_bdof_disabled_flag
        if(spsDmvrControlInPh)
          bs.getBit(); // ph_dmvr_disabled_flag
      }
      if(spsProfControlInPh)
        ph.ph_prof_disabled_flag = bs.getBit();
    }

    if(ppsRplInPh)
    {
      // ref_pic_lists（简化：读 rpl_sps_flag / rpl_idx / ref_pic_list_struct）
      for(int i = 0; i < 2; i++)
      {
        uint32_t numRefEntries = 0;
        if(ppps)
          numRefEntries = ppps->pps.pps_num_ref_idx_default_active_minus1[i] + 1;
        if(!ph.ph_inter_slice_allowed_flag)
          numRefEntries = 0;
        if(i == 1 && !(ppps && ppps->pps.pps_rpl1_idx_present_flag))
          continue;

        if(numRefEntries > 0)
        {
          uint32_t spsNumRpl = psps ? psps->sps.sps_num_ref_pic_lists[i] : 0;
          bool rplSpsFlag = bs.getBit();
          if(rplSpsFlag)
          {
            if(spsNumRpl > 1)
              bs.getBits(ceilLog2(spsNumRpl));
          }
          else
          {
            // ref_pic_list_struct 简化
            uint32_t numEntries = bs.getGolombU();
            for(uint32_t k = 0; k < numEntries; k++)
              bs.getGolombU(); // 简化跳过
          }
        }
      }
    }

    uint8_t ppsDbfInPh = ppps ? ppps->pps.pps_dbf_info_in_ph_flag : 0;
    if(ppsDbfInPh)
    {
      uint8_t present = bs.getBit();
      if(present)
      {
        bs.getBit(); // deblocking_filter_disabled_flag
        bs.getGolombS(); // luma_beta_offset_div2
        bs.getGolombS(); // luma_tc_offset_div2
        uint8_t chromaTool = ppps ? ppps->pps.pps_chroma_tool_offsets_present_flag : 0;
        if(chromaTool)
        {
          bs.getGolombS();
          bs.getGolombS();
          bs.getGolombS();
          bs.getGolombS();
        }
      }
    }

    uint8_t ppsSaoInPh = ppps ? ppps->pps.pps_sao_info_in_ph_flag : 0;
    if(ppsSaoInPh)
    {
      bs.getBit(); // ph_sao_luma_flag
      if(ph.ph_inter_slice_allowed_flag)
        ph.ph_slice_sao_chroma_flag = bs.getBit();
    }

    uint8_t ppsWpInPh = ppps ? ppps->pps.pps_wp_info_in_ph_flag : 0;
    uint8_t spsWeightedPred = psps ? psps->sps.sps_weighted_pred_flag : 0;
    uint8_t spsWeightedBipred = psps ? psps->sps.sps_weighted_bipred_flag : 0;
    if(ppsWpInPh && (spsWeightedPred || spsWeightedBipred))
    {
      // pred_weight_table 简化（读 luma_log2_weight_denom 等）
      bs.getGolombU(); // luma_log2_weight_denom
    }

    uint8_t ppsQpDeltaInPh = ppps ? ppps->pps.pps_qp_delta_info_in_ph_flag : 0;
    if(ppsQpDeltaInPh)
      bs.getGolombS(); // ph_qp_delta

    uint8_t spsJointCbcr = psps ? psps->sps.sps_joint_cbcr_enabled_flag : 0;
    if(spsJointCbcr)
      ph.ph_joint_cbcr_sign_flag = bs.getBit();
  }

  void VvcParserImpl::processPH(std::shared_ptr<PH_NAL> p, BitstreamReader &bs, const Parser::Info &info)
  {
    processPictureHeader(p->ph, bs, info);
  }

  void VvcParserImpl::processSlice(std::shared_ptr<Slice_NAL> p, BitstreamReader &bs, const Parser::Info &info)
  {
    Slice &s = p->slice;
    s.toDefault();

    s.picture_header_in_slice_header_flag = bs.getBit();
    if(s.picture_header_in_slice_header_flag)
    {
      // picture header 内联
      PH ph;
      processPictureHeader(ph, bs, info);
      s.slice_pic_parameter_set_id = ph.ph_pic_parameter_set_id;
      s.slice_pic_order_cnt_lsb = ph.ph_pic_order_cnt_lsb;
      p->ph = ph;
      p->hasPH = true;
      m_lastPH = std::make_shared<PH_NAL>(p->m_nalHeader);
      m_lastPH->ph = ph;
    }
    else
    {
      if(m_lastPH)
      {
        s.slice_pic_parameter_set_id = m_lastPH->ph.ph_pic_parameter_set_id;
        s.slice_pic_order_cnt_lsb = m_lastPH->ph.ph_pic_order_cnt_lsb;
      }
    }

    std::shared_ptr<PPS_NAL> ppps = m_ppsMap[s.slice_pic_parameter_set_id];
    std::shared_ptr<SPS_NAL> psps;
    if(ppps)
      psps = m_spsMap[ppps->pps.pps_seq_parameter_set_id];

    PH *phPtr = m_lastPH ? &m_lastPH->ph : NULL;
    uint8_t nalType = p->m_nalHeader.nal_unit_type;

    // subpic id
    if(psps && psps->sps.sps_subpic_info_present_flag)
      s.slice_subpic_id = bs.getBits(psps->sps.sps_subpic_id_len_minus1 + 1);

    // slice_address（多 slice 时读，位数 = Ceil(Log2(numSlicesInSubpic))）
    bool rectSlice = ppps && ppps->pps.pps_rect_slice_flag;
    uint32_t numSlicesInSubpic = 1;
    if(ppps)
    {
      if(ppps->pps.pps_single_slice_per_subpic_flag)
        numSlicesInSubpic = 1;
      else
        numSlicesInSubpic = ppps->pps.pps_num_slices_in_pic_minus1 + 1;
    }
    if(rectSlice && numSlicesInSubpic > 1)
      s.slice_address = bs.getBits(ceilLog2(numSlicesInSubpic));

    // sh_extra_bit（NumExtraShBits = 8 * sps_num_extra_sh_bytes）
    if(psps)
    {
      for(uint32_t i = 0; i < 8 * psps->sps.sps_num_extra_sh_bytes; i++)
        bs.getBit();
    }

    // sh_num_tiles_in_slice_minus1（非 rect slice 且多 tile 时读）
    if(!rectSlice)
      bs.getGolombU();

    // sh_slice_type
    bool interAllowed = m_lastPH ? m_lastPH->ph.ph_inter_slice_allowed_flag : true;
    if(interAllowed)
      s.slice_type = bs.getGolombU();
    else
      s.slice_type = 2; // I

    bool isI = (s.slice_type == 2);
    bool isP = (s.slice_type == 1);
    bool isB = (s.slice_type == 0);

    uint8_t chromaIdc = psps ? psps->sps.sps_chroma_format_idc : 0;

    // (a) sh_no_output_of_prior_pics_flag
    if(nalType == NAL_IDR_W_RADL || nalType == NAL_IDR_N_LP || nalType == NAL_CRA_NUT || nalType == NAL_GDR_NUT)
      s.sh_no_output_of_prior_pics_flag = bs.getBit();

    // (b) ALF 参数
    uint8_t spsAlf = psps ? psps->sps.sps_alf_enabled_flag : 0;
    uint8_t ppsAlfInPh = ppps ? ppps->pps.pps_alf_info_in_ph_flag : 0;
    uint8_t spsCcAlf = psps ? psps->sps.sps_ccalf_enabled_flag : 0;
    if(spsAlf && !ppsAlfInPh)
    {
      s.sh_alf_enabled_flag = bs.getBit();
      if(s.sh_alf_enabled_flag)
      {
        s.sh_num_alf_aps_ids_luma = bs.getBits(3);
        for(uint32_t i = 0; i < s.sh_num_alf_aps_ids_luma; i++)
          s.sh_alf_aps_id_luma.push_back(bs.getBits(3));
        if(chromaIdc != 0)
        {
          s.sh_alf_cb_enabled_flag = bs.getBit();
          s.sh_alf_cr_enabled_flag = bs.getBit();
        }
        if(s.sh_alf_cb_enabled_flag || s.sh_alf_cr_enabled_flag)
          s.sh_alf_aps_id_chroma = bs.getBits(3);
        if(spsCcAlf)
        {
          if(bs.getBit()) bs.getBits(3); // sh_alf_cc_cb_enabled_flag + sh_alf_cc_cb_aps_id
          if(bs.getBit()) bs.getBits(3); // sh_alf_cc_cr_enabled_flag + sh_alf_cc_cr_aps_id
        }
      }
    }

    // (c) sh_lmcs_used_flag
    uint8_t phLmcs = phPtr ? phPtr->ph_lmcs_enabled_flag : 0;
    if(phLmcs && !s.picture_header_in_slice_header_flag)
      s.sh_lmcs_used_flag = bs.getBit();

    // (d) sh_explicit_scaling_list_used_flag
    uint8_t phExplicitSl = phPtr ? phPtr->ph_explicit_scaling_list_enabled_flag : 0;
    if(phExplicitSl && !s.picture_header_in_slice_header_flag)
      s.sh_explicit_scaling_list_used_flag = bs.getBit();

    // (e) ref_pic_lists
    uint8_t ppsRplInPh = ppps ? ppps->pps.pps_rpl_info_in_ph_flag : 0;
    uint8_t spsIdrRpl = psps ? psps->sps.sps_idr_rpl_present_flag : 0;
    uint32_t numRefEntries[2] = {0, 0};
    if(ppps)
    {
      numRefEntries[0] = isI ? 0 : ppps->pps.pps_num_ref_idx_default_active_minus1[0] + 1;
      numRefEntries[1] = isB ? ppps->pps.pps_num_ref_idx_default_active_minus1[1] + 1 : 0;
    }
    bool rplPresent = !ppsRplInPh && ((nalType != NAL_IDR_W_RADL && nalType != NAL_IDR_N_LP) || spsIdrRpl);
    if(rplPresent && psps)
    {
      uint8_t ppsRpl1IdxPresent = ppps->pps.pps_rpl1_idx_present_flag;
      for(uint32_t i = 0; i < 2; i++)
      {
        if(i == 1 && !ppsRpl1IdxPresent)
          continue;
        uint32_t spsNumRpl = psps->sps.sps_num_ref_pic_lists[i];
        bool rplSpsFlag = bs.getBit();
        if(rplSpsFlag)
        {
          if(spsNumRpl > 1)
            bs.getBits(ceilLog2(spsNumRpl)); // rpl_idx
        }
        else
        {
          readRefPicListStruct(bs, psps->sps);
        }
      }
    }

    // (f) sh_num_ref_idx_active_override_flag
    if((!isI && numRefEntries[0] > 1) || (isB && numRefEntries[1] > 1))
    {
      s.sh_num_ref_idx_active_override_flag = bs.getBit();
      if(s.sh_num_ref_idx_active_override_flag)
      {
        for(uint32_t i = 0; i < (isB ? 2u : 1u); i++)
        {
          if(numRefEntries[i] > 1)
            s.sh_num_ref_idx_active_minus1[i] = bs.getGolombU();
        }
      }
    }

    // NumRefIdxActive（用于 collocated_ref_idx / pred_weight_table）
    uint32_t NumRefIdxActive[2] = {0, 0};
    for(uint32_t i = 0; i < 2; i++)
    {
      if(isB || (isP && i == 0))
      {
        if(s.sh_num_ref_idx_active_override_flag)
          NumRefIdxActive[i] = s.sh_num_ref_idx_active_minus1[i] + 1;
        else
          NumRefIdxActive[i] = ppps ? ppps->pps.pps_num_ref_idx_default_active_minus1[i] + 1 : 1;
      }
      else
        NumRefIdxActive[i] = 0;
    }

    // (g) sh_cabac_init_flag + (h) sh_collocated_from_l0_flag / sh_collocated_ref_idx
    uint8_t phTemporalMvp = phPtr ? phPtr->ph_temporal_mvp_enabled_flag : 0;
    if(!isI)
    {
      uint8_t ppsCabacInit = ppps ? ppps->pps.pps_cabac_init_present_flag : 0;
      if(ppsCabacInit)
        s.sh_cabac_init_flag = bs.getBit();

      if(phTemporalMvp && !ppsRplInPh)
      {
        if(isB)
          s.sh_collocated_from_l0_flag = bs.getBit();
        else
          s.sh_collocated_from_l0_flag = 1;
        if((s.sh_collocated_from_l0_flag && NumRefIdxActive[0] > 1) ||
           (!s.sh_collocated_from_l0_flag && NumRefIdxActive[1] > 1))
          s.sh_collocated_ref_idx = bs.getGolombU();
      }
    }

    // (i) pred_weight_table
    uint8_t ppsWpInPh = ppps ? ppps->pps.pps_wp_info_in_ph_flag : 0;
    uint8_t ppsWeightedPred = ppps ? ppps->pps.pps_weighted_pred_flag : 0;
    uint8_t ppsWeightedBipred = ppps ? ppps->pps.pps_weighted_bipred_flag : 0;
    if(!ppsWpInPh && ((ppsWeightedPred && isP) || (ppsWeightedBipred && isB)))
    {
      bs.getGolombU(); // luma_log2_weight_denom
      if(chromaIdc != 0)
        bs.getGolombS(); // delta_chroma_log2_weight_denom
      for(uint32_t l = 0; l < (isB ? 2u : 1u); l++)
      {
        uint32_t n = NumRefIdxActive[l];
        std::vector<bool> lumaFlag(n, false);
        std::vector<bool> chromaFlag(n, false);
        for(uint32_t i = 0; i < n; i++)
          lumaFlag[i] = bs.getBit();
        if(chromaIdc != 0)
          for(uint32_t i = 0; i < n; i++)
            chromaFlag[i] = bs.getBit();
        for(uint32_t i = 0; i < n; i++)
          if(lumaFlag[i]) { bs.getGolombS(); bs.getGolombS(); }
        if(chromaIdc != 0)
          for(uint32_t i = 0; i < n; i++)
            if(chromaFlag[i])
              for(uint32_t j = 0; j < 2; j++) { bs.getGolombS(); bs.getGolombS(); }
      }
    }

    // (j) sh_qp_delta
    uint8_t ppsQpDeltaInPh = ppps ? ppps->pps.pps_qp_delta_info_in_ph_flag : 0;
    if(!ppsQpDeltaInPh)
      s.slice_qp_delta = bs.getGolombS();

    // (k) chroma QP offset
    uint8_t ppsSliceChromaQpOffsets = ppps ? ppps->pps.pps_slice_chroma_qp_offsets_present_flag : 0;
    uint8_t spsJointCbcr = psps ? psps->sps.sps_joint_cbcr_enabled_flag : 0;
    if(ppsSliceChromaQpOffsets)
    {
      s.slice_cb_qp_offset = bs.getGolombS();
      s.slice_cr_qp_offset = bs.getGolombS();
      if(spsJointCbcr)
        s.slice_joint_cbcr_qp_offset = bs.getGolombS();
    }

    // (l) sh_cu_chroma_qp_offset_enabled_flag
    uint8_t ppsCuChromaQpOffsetList = ppps ? ppps->pps.pps_cu_chroma_qp_offset_list_enabled_flag : 0;
    if(ppsCuChromaQpOffsetList)
      s.sh_cu_chroma_qp_offset_enabled_flag = bs.getBit();

    // (m) SAO
    uint8_t spsSao = psps ? psps->sps.sps_sao_enabled_flag : 0;
    uint8_t ppsSaoInPh = ppps ? ppps->pps.pps_sao_info_in_ph_flag : 0;
    if(spsSao && !ppsSaoInPh)
    {
      s.sh_sao_luma_used_flag = bs.getBit();
      if(chromaIdc != 0)
        s.sh_sao_chroma_used_flag = bs.getBit();
    }

    // (n) deblocking
    uint8_t ppsDeblockOverride = ppps ? ppps->pps.pps_deblocking_filter_override_enabled_flag : 0;
    uint8_t ppsDbfInPh = ppps ? ppps->pps.pps_dbf_info_in_ph_flag : 0;
    if(ppsDeblockOverride && !ppsDbfInPh)
    {
      if(bs.getBit()) // sh_deblocking_params_present_flag
      {
        uint8_t ppsDeblockDisabled = ppps ? ppps->pps.pps_deblocking_filter_disabled_flag : 0;
        bool deblockDisabled = ppsDeblockDisabled;
        if(!ppsDeblockDisabled)
          deblockDisabled = bs.getBit();
        if(!deblockDisabled)
        {
          bs.getGolombS(); // sh_luma_beta_offset_div2
          bs.getGolombS(); // sh_luma_tc_offset_div2
          uint8_t ppsChromaTool = ppps ? ppps->pps.pps_chroma_tool_offsets_present_flag : 0;
          if(ppsChromaTool)
          {
            bs.getGolombS(); bs.getGolombS();
            bs.getGolombS(); bs.getGolombS();
          }
        }
      }
    }

    // (o) sh_dep_quant_used_flag
    uint8_t spsDepQuant = psps ? psps->sps.sps_dep_quant_enabled_flag : 0;
    if(spsDepQuant)
      s.sh_dep_quant_used_flag = bs.getBit();

    // (p) sh_sign_data_hiding_used_flag
    uint8_t spsSignDataHiding = psps ? psps->sps.sps_sign_data_hiding_enabled_flag : 0;
    if(spsSignDataHiding && !s.sh_dep_quant_used_flag)
      s.sh_sign_data_hiding_used_flag = bs.getBit();

    // (q) sh_ts_residual_coding_disabled_flag
    uint8_t spsTransformSkip = psps ? psps->sps.sps_transform_skip_enabled_flag : 0;
    if(spsTransformSkip && !s.sh_dep_quant_used_flag && !s.sh_sign_data_hiding_used_flag)
      s.sh_ts_residual_coding_disabled_flag = bs.getBit();

    // (r) sh_slice_header_extension
    uint8_t ppsShExtension = ppps ? ppps->pps.pps_slice_header_extension_present_flag : 0;
    if(ppsShExtension)
    {
      uint32_t len = bs.getGolombU();
      for(uint32_t i = 0; i < len; i++)
        bs.getBits(8);
    }
  }

  void VvcParserImpl::processAUD(std::shared_ptr<AUD_NAL> p, BitstreamReader &bs)
  {
    p->aud.toDefault();
    p->aud.aud_irap_or_gdr_flag = bs.getBit();
    p->aud.aud_pic_type = bs.getBits(3);
  }

  void VvcParserImpl::processSEI(std::shared_ptr<SEI_NAL> p, BitstreamReader &bs)
  {
    while(bs.availableInNalU() > 8 && bs.showBits(8) != 0x80)
    {
      SeiMessage msg;
      msg.toDefault();

      uint32_t payloadType = 0;
      uint8_t b = bs.getBits(8);
      while(b == 0xFF) { payloadType += 255; b = bs.getBits(8); }
      payloadType += b;
      msg.payload_type = payloadType;

      uint32_t payloadSize = 0;
      b = bs.getBits(8);
      while(b == 0xFF) { payloadSize += 255; b = bs.getBits(8); }
      payloadSize += b;
      msg.payload_size = payloadSize;

      if(payloadSize > bs.availableInNalU() / 8 + 2)
        break;

      for(uint32_t i = 0; i < payloadSize; i++)
        msg.payload_data.push_back(bs.getBits(8));

      p->messages.push_back(msg);
    }
  }

}
