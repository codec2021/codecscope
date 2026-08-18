#include "AvcWebParser.h"

#include "AvcSyntaxWriter.h"
#include "ColorNames.h"
#include "Json.h"

#include <limits>
#include <sstream>
#include <algorithm>

namespace web
{

  namespace
  {
    std::string avcProfileName(uint32_t profileIdc)
    {
      switch(profileIdc)
      {
        case 66: return "Baseline";
        case 77: return "Main";
        case 88: return "Extended";
        case 100: return "High";
        case 110: return "High 10";
        case 122: return "High 4:2:2";
        case 244: return "High 4:4:4";
        case 44: return "CAVLC 4:4:4";
        case 83: return "Scalable Baseline";
        case 86: return "Scalable High";
        case 118: return "Stereo High";
        case 128: return "Multiview High";
        default:
        {
          std::stringstream ss;
          ss << profileIdc << " (UNKNOWN)";
          return ss.str();
        }
      }
    }
  }

  AvcWebParser::AvcWebParser():
    m_totalSize(0)
    ,m_nalusNumber(0)
    ,m_INumber(0)
    ,m_PNumber(0)
    ,m_BNumber(0)
    ,m_frameNum(0)
    ,m_profile(0)
    ,m_level(0)
    ,m_profilePresent(false)
    ,m_levelPresent(false)
    ,m_pocMsb(0)
    ,m_prevPicOrderCntLsb(0)
    ,m_prevFrameNum(0)
    ,m_pocInitialized(false)
    ,m_maxFrameNum(0)
  {
  }

  AvcWebParser::~AvcWebParser()
  {
  }

  void AvcWebParser::reset()
  {
    m_nalus.clear();
    m_warnings.clear();
    m_totalSize = 0;
    m_nalusNumber = 0;
    m_INumber = 0;
    m_PNumber = 0;
    m_BNumber = 0;
    m_frameNum = 0;
    m_profile = 0;
    m_level = 0;
    m_profilePresent = false;
    m_levelPresent = false;
    m_dpb.clear();
    m_pocMsb = 0;
    m_prevPicOrderCntLsb = 0;
    m_prevFrameNum = 0;
    m_pocInitialized = false;
    m_maxFrameNum = 0;
    m_lastSPS.reset();
    m_spsMap.clear();
    m_ppsMap.clear();
  }

  void AvcWebParser::setTotalSize(std::size_t size)
  {
    m_totalSize = size;
  }

  void AvcWebParser::fillPocAndRefs(NALUEntry &e, const AVC::Slice_NAL *p)
  {
    const AVC::Slice &sl = p->slice;
    bool idr = (p->m_nalHeader.nal_unit_type == AVC::NAL_IDR_SLICE);
    bool isB = (sl.slice_type % 5 == 1);
    bool isI = (sl.slice_type % 5 == 2) || (sl.slice_type % 5 == 4);

    std::shared_ptr<AVC::SPS_NAL> sps;
    auto ppsIt = m_ppsMap.find(sl.pic_parameter_set_id);
    if(ppsIt != m_ppsMap.end())
    {
      auto spsIt = m_spsMap.find(ppsIt->second->pps.seq_parameter_set_id);
      if(spsIt != m_spsMap.end())
        sps = spsIt->second;
    }
    if(!sps)
      sps = m_lastSPS;

    int maxLsb = sps ? (1 << (int)(sps->sps.log2_max_pic_order_cnt_lsb_minus4 + 4)) : 1;
    int maxFrameNum = sps ? (1 << (int)(sps->sps.log2_max_frame_num_minus4 + 4)) : 1;
    m_maxFrameNum = maxFrameNum;

    // ---- POC（pic_order_cnt_type 0）----
    int poc = 0;
    if(idr)
    {
      m_pocMsb = 0;
      m_prevPicOrderCntLsb = 0;
      m_pocInitialized = true;
      poc = 0;
    }
    else
    {
      int picOrderCntLsb = (int)sl.pic_order_cnt_lsb;
      int msb = m_pocMsb;
      if(m_pocInitialized)
      {
        if(picOrderCntLsb < m_prevPicOrderCntLsb && (m_prevPicOrderCntLsb - picOrderCntLsb) >= maxLsb / 2)
          msb += maxLsb;
        else if(picOrderCntLsb > m_prevPicOrderCntLsb && (picOrderCntLsb - m_prevPicOrderCntLsb) > maxLsb / 2)
          msb -= maxLsb;
      }
      poc = msb + picOrderCntLsb;
      m_pocMsb = msb;
      m_prevPicOrderCntLsb = picOrderCntLsb;
      m_pocInitialized = true;
    }
    e.slicePoc = poc;

    // ---- frame_num 展开（PicNum）----
    int frameNum = (int)sl.frame_num;
    int frameNumWrap = frameNum;
    if(!idr && frameNum < m_prevFrameNum)
      frameNumWrap += maxFrameNum;

    // ---- 构建 ref_pic_list ----
    if(!idr && !isI)
    {
      std::vector<const RefPic*> shortTerm;
      for(const auto &r : m_dpb)
        if(r.longTermFrameIdx < 0)
          shortTerm.push_back(&r);

      std::vector<const RefPic*> list0, list1;

      if(isB)
      {
        std::vector<const RefPic*> less, more;
        for(const auto *r : shortTerm)
        {
          if(r->picNum < frameNumWrap) less.push_back(r);
          else if(r->picNum > frameNumWrap) more.push_back(r);
        }
        std::sort(less.begin(), less.end(), [](const RefPic *a, const RefPic *b){ return a->picNum > b->picNum; });
        std::sort(more.begin(), more.end(), [](const RefPic *a, const RefPic *b){ return a->picNum > b->picNum; });
        list0.insert(list0.end(), less.begin(), less.end());
        list0.insert(list0.end(), more.begin(), more.end());

        std::sort(more.begin(), more.end(), [](const RefPic *a, const RefPic *b){ return a->picNum < b->picNum; });
        std::sort(less.begin(), less.end(), [](const RefPic *a, const RefPic *b){ return a->picNum < b->picNum; });
        list1.insert(list1.end(), more.begin(), more.end());
        list1.insert(list1.end(), less.begin(), less.end());
      }
      else
      {
        list0 = shortTerm;
        std::sort(list0.begin(), list0.end(), [](const RefPic *a, const RefPic *b){ return a->picNum > b->picNum; });
      }

      // ---- ref_pic_list_reordering ----
      const AVC::RefPicListModification &mod = sl.ref_pic_list_modification;
      auto reorder = [&](std::vector<const RefPic*> &list, bool l0)
      {
        if(l0 ? !mod.ref_pic_list_modification_flag_l0 : !mod.ref_pic_list_modification_flag_l1)
          return;
        const std::vector<uint32_t> &idc   = l0 ? mod.modification_of_pic_nums_idc_l0 : mod.modification_of_pic_nums_idc_l1;
        const std::vector<uint32_t> &absdiff = l0 ? mod.abs_diff_pic_num_minus1_l0 : mod.abs_diff_pic_num_minus1_l1;
        int picNumPred = frameNumWrap;
        std::size_t refIdx = 0;
        for(std::size_t i = 0; i < idc.size() && refIdx < list.size(); i++)
        {
          if(idc[i] == 3)
            break;
          if(idc[i] == 0 || idc[i] == 1)
          {
            int target = (idc[i] == 0) ? picNumPred - (int)(absdiff[i] + 1) : picNumPred + (int)(absdiff[i] + 1);
            picNumPred = target;
            for(std::size_t k = refIdx; k < list.size(); k++)
              if(list[k]->picNum == target)
              {
                std::swap(list[refIdx], list[k]);
                break;
              }
            refIdx++;
          }
        }
      };
      reorder(list0, true);
      if(isB)
        reorder(list1, false);

      for(const auto *r : list0)
        e.refPocs.push_back(r->poc);
      if(isB)
        for(const auto *r : list1)
          e.refPocs.push_back(r->poc);

      std::sort(e.refPocs.begin(), e.refPocs.end());
      e.refPocs.erase(std::unique(e.refPocs.begin(), e.refPocs.end()), e.refPocs.end());
    }

    // ---- DPB 更新 ----
    if(idr)
    {
      m_dpb.clear();
      RefPic cur;
      cur.picNum = frameNumWrap;
      cur.poc = poc;
      cur.longTermFrameIdx = -1;
      m_dpb.push_back(cur);
      m_prevFrameNum = frameNum;
    }
    else if(sl.first_mb_in_slice == 0)
    {
      // 仅 reference 帧（nal_ref_idc != 0）进入 DPB
      if(p->m_nalHeader.nal_ref_idc != 0)
      {
        int maxRefs = sps ? (int)sps->sps.max_num_ref_frames : 16;
        if((int)m_dpb.size() >= maxRefs && maxRefs > 0)
        {
          std::size_t minIdx = m_dpb.size();
          for(std::size_t i = 0; i < m_dpb.size(); i++)
            if(m_dpb[i].longTermFrameIdx < 0)
              if(minIdx == m_dpb.size() || m_dpb[i].picNum < m_dpb[minIdx].picNum)
                minIdx = i;
          if(minIdx != m_dpb.size())
            m_dpb.erase(m_dpb.begin() + minIdx);
        }
        RefPic cur;
        cur.picNum = frameNumWrap;
        cur.poc = poc;
        cur.longTermFrameIdx = -1;
        m_dpb.push_back(cur);
      }
      m_prevFrameNum = frameNum;
    }
  }

  void AvcWebParser::onNALUnit(std::shared_ptr<AVC::NALUnit> pNALUnit, const AVC::Parser::Info *pInfo)
  {
    NALUEntry e;
    e.offset = pInfo ? pInfo->m_position : 0;
    e.length = 0;
    e.type = (uint32_t)pNALUnit->m_nalHeader.nal_unit_type;
    e.typeName = avcNalTypeName(pNALUnit->m_nalHeader.nal_unit_type);
    e.sliceType = -1;
    e.sliceQp = -1;
    e.slicePoc = -1;
    e.frameNum = -1;
    e.nal = pNALUnit;

    m_nalusNumber++;

    switch(pNALUnit->m_nalHeader.nal_unit_type)
    {
      case AVC::NAL_SPS:
      case AVC::NAL_SUBSET_SPS:
      {
        std::shared_ptr<AVC::SPS_NAL> p = std::dynamic_pointer_cast<AVC::SPS_NAL>(pNALUnit);
        e.info = "Sequence Parameter Set";
        e.color = "#FF8888";
        m_profile = p->sps.profile_idc;
        m_level = p->sps.level_idc;
        m_profilePresent = true;
        m_levelPresent = true;
        m_lastSPS = p;
        m_spsMap[p->sps.seq_parameter_set_id] = p;
        break;
      }

      case AVC::NAL_PPS:
      {
        std::shared_ptr<AVC::PPS_NAL> p = std::dynamic_pointer_cast<AVC::PPS_NAL>(pNALUnit);
        e.info = "Picture Parameter Set";
        e.color = "#FF8888";
        m_ppsMap[p->pps.pic_parameter_set_id] = p;
        break;
      }

      case AVC::NAL_IDR_SLICE:
      {
        std::shared_ptr<AVC::Slice_NAL> p = std::dynamic_pointer_cast<AVC::Slice_NAL>(pNALUnit);
        e.info = "IDR Slice #" + std::to_string(m_frameNum);
        e.color = "red";
        e.sliceType = 2; // I
        fillPocAndRefs(e, p.get());
        e.frameNum = p->slice.frame_num;
        e.sliceQp = 26 + p->slice.slice_qp_delta;
        auto itPps = m_ppsMap.find(p->slice.pic_parameter_set_id);
        if(itPps != m_ppsMap.end())
          e.sliceQp += itPps->second->pps.pic_init_qp_minus26;
        m_INumber++;
        m_frameNum++;
        break;
      }

      case AVC::NAL_SLICE:
      {
        std::shared_ptr<AVC::Slice_NAL> p = std::dynamic_pointer_cast<AVC::Slice_NAL>(pNALUnit);
        uint32_t st = p->slice.slice_type % 5;
        fillPocAndRefs(e, p.get());
        e.frameNum = p->slice.frame_num;
        e.sliceQp = 26 + p->slice.slice_qp_delta;
        auto itPps = m_ppsMap.find(p->slice.pic_parameter_set_id);
        if(itPps != m_ppsMap.end())
          e.sliceQp += itPps->second->pps.pic_init_qp_minus26;

        switch(st)
        {
          case 2: case 4: // I / SI
            e.info = "I Slice #" + std::to_string(m_frameNum);
            e.color = "#CD9B1D";
            e.sliceType = 2;
            m_INumber++;
            break;
          case 0: case 3: // P / SP
            e.info = (st == 0 ? "P Slice #" : "SP Slice #") + std::to_string(m_frameNum);
            e.color = "#0000FF";
            e.sliceType = 0;
            m_PNumber++;
            break;
          case 1: // B
            e.info = "B Slice #" + std::to_string(m_frameNum);
            e.color = "#FFB90F";
            e.sliceType = 1;
            m_BNumber++;
            break;
        }
        m_frameNum++;
        break;
      }

      case AVC::NAL_AUD:
        e.info = "Access unit delimiter";
        break;

      case AVC::NAL_END_SEQUENCE:
        e.info = "End of sequence";
        break;

      case AVC::NAL_END_STREAM:
        e.info = "End of stream";
        break;

      case AVC::NAL_FILLER:
        e.info = "Filler data";
        break;

      case AVC::NAL_SEI:
        e.info = "Supplemental Enhancement Information";
        e.color = "#BCEE68";
        break;

      default:
        break;
    }

    m_nalus.push_back(e);
  }

  void AvcWebParser::onWarning(const std::string &warning, const AVC::Parser::Info *pInfo, AVC::Parser::WarningType type)
  {
    WarningEntry w;
    w.position = pInfo ? pInfo->m_position : 0;
    w.message = warning;
    w.type = (int)type;
    m_warnings.push_back(w);
  }

  std::string AvcWebParser::serializeSummary() const
  {
    std::string out;
    out.reserve(4096);

    std::vector<std::size_t> lengths(m_nalus.size(), 0);
    for(std::size_t i = 0; i < m_nalus.size(); i++)
    {
      std::size_t next = (i + 1 < m_nalus.size()) ? m_nalus[i + 1].offset : m_totalSize;
      if(next >= m_nalus[i].offset)
        lengths[i] = next - m_nalus[i].offset;
    }

    out += "{\"nalus\":[";
    for(std::size_t i = 0; i < m_nalus.size(); i++)
    {
      if(i) out += ",";
      out += "{\"offset\":" + std::to_string(m_nalus[i].offset);
      out += ",\"length\":" + std::to_string(lengths[i]);
      out += ",\"type\":" + std::to_string(m_nalus[i].type);
      out += ",\"typeName\":\"" + jsonEscape(m_nalus[i].typeName) + "\"";
      out += ",\"info\":\"" + jsonEscape(m_nalus[i].info) + "\"";
      out += ",\"color\":\"" + jsonEscape(m_nalus[i].color) + "\"";
      out += ",\"sliceType\":" + std::to_string(m_nalus[i].sliceType);
      out += ",\"sliceQp\":" + std::to_string(m_nalus[i].sliceQp);
      out += ",\"slicePoc\":" + std::to_string(m_nalus[i].slicePoc);
      out += ",\"frameNum\":" + std::to_string(m_nalus[i].frameNum);
      out += ",\"refPocs\":[";
      for(std::size_t k = 0; k < m_nalus[i].refPocs.size(); k++)
      {
        if(k) out += ",";
        out += std::to_string(m_nalus[i].refPocs[k]);
      }
      out += "]";
      out += "}";
    }
    out += "]";

    std::size_t slicesNumber = m_INumber + m_PNumber + m_BNumber;
    out += ",\"streamInfo\":{";
    out += "\"nalus\":" + std::to_string(m_nalusNumber);
    out += ",\"slices\":" + std::to_string(slicesNumber);
    out += ",\"i\":" + std::to_string(m_INumber);
    out += ",\"p\":" + std::to_string(m_PNumber);
    out += ",\"b\":" + std::to_string(m_BNumber);
    if(slicesNumber)
    {
      std::stringstream ssI; ssI << (double)m_INumber * 100.0 / slicesNumber;
      std::stringstream ssP; ssP << (double)m_PNumber * 100.0 / slicesNumber;
      std::stringstream ssB; ssB << (double)m_BNumber * 100.0 / slicesNumber;
      out += ",\"iPct\":\"" + ssI.str() + "\"";
      out += ",\"pPct\":\"" + ssP.str() + "\"";
      out += ",\"bPct\":\"" + ssB.str() + "\"";
    }
    if(m_profilePresent)
    {
      out += ",\"profile\":\"" + jsonEscape(avcProfileName(m_profile)) + "\"";
      out += ",\"profileIdc\":" + std::to_string(m_profile);
    }
    else
      out += ",\"profile\":\"NOT PRESENT\"";

    if(m_levelPresent)
    {
      std::stringstream ss; ss << (double)m_level / 10.0;
      out += ",\"level\":\"" + ss.str() + "\"";
    }
    else
      out += ",\"level\":\"NOT PRESENT\"";

    out += ",\"tier\":\"\"";
    out += "}";

    // HDR
    out += ",\"hdr\":{";
    int cp = 2, tc = 2, mc = 2;
    int fullRange = 0;
    bool hasColor = false;
    if(m_lastSPS && m_lastSPS->sps.vui_parameters_present_flag &&
       m_lastSPS->sps.vui_parameters.video_signal_type_present_flag)
    {
      fullRange = m_lastSPS->sps.vui_parameters.video_full_range_flag;
      if(m_lastSPS->sps.vui_parameters.colour_description_present_flag)
      {
        cp = m_lastSPS->sps.vui_parameters.colour_primaries;
        tc = m_lastSPS->sps.vui_parameters.transfer_characteristics;
        mc = m_lastSPS->sps.vui_parameters.matrix_coefficients;
        hasColor = true;
      }
    }
    out += "\"colourPrimaries\":\"" + jsonEscape(colourPrimariesToString(cp)) + "\"";
    out += ",\"transferCharacteristics\":\"" + jsonEscape(transferCharacteristicsToString(tc)) + "\"";
    out += ",\"matrixCoefficients\":\"" + jsonEscape(matrixCoefficientsToString(mc)) + "\"";
    out += ",\"chromaLocTop\":0";
    out += ",\"chromaLocBottom\":0";
    out += ",\"fullRange\":" + std::to_string(fullRange);
    out += ",\"hasCll\":false";
    out += ",\"hasMdi\":false";

    // 分辨率
    int picWidth = 0, picHeight = 0;
    if(m_lastSPS)
    {
      const AVC::SPS &s = m_lastSPS->sps;
      picWidth = (s.pic_width_in_mbs_minus1 + 1) * 16;
      picHeight = (2 - s.frame_mbs_only_flag) * (s.pic_height_in_map_units_minus1 + 1) * 16;
    }
    out += ",\"picWidth\":" + std::to_string(picWidth);
    out += ",\"picHeight\":" + std::to_string(picHeight);
    out += "}";

    // warnings
    out += ",\"warnings\":[";
    for(std::size_t i = 0; i < m_warnings.size(); i++)
    {
      if(i) out += ",";
      out += "{\"position\":" + std::to_string(m_warnings[i].position);
      out += ",\"message\":\"" + jsonEscape(m_warnings[i].message) + "\"";
      out += ",\"type\":" + std::to_string(m_warnings[i].type) + "}";
    }
    out += "]";

    out += "}";
    return out;
  }

  std::string AvcWebParser::serializeNalSyntax(std::size_t index) const
  {
    if(index >= m_nalus.size())
      return "{\"n\":\"Invalid index\"}";

    AvcSyntaxWriter writer;
    writer.setParameterSets(m_spsMap, m_ppsMap);
    return writer.write(m_nalus[index].nal);
  }

}
