#ifndef WEB_SYNTAX_WRITER_H_
#define WEB_SYNTAX_WRITER_H_

#include <Hevc.h>
#include <HevcParser.h>

#include "Json.h"

#include <memory>
#include <map>
#include <vector>
#include <string>

namespace web
{

  class SyntaxWriter
  {
    public:
      void setParameterSets(const std::map<uint32_t, std::shared_ptr<HEVC::VPS> > &vpsMap,
                            const std::map<uint32_t, std::shared_ptr<HEVC::SPS> > &spsMap,
                            const std::map<uint32_t, std::shared_ptr<HEVC::PPS> > &ppsMap);

      // 生成指定 NAL 单元的语法树 JSON 字符串
      std::string write(std::shared_ptr<HEVC::NALUnit> pNALUnit);

    private:
      void createVPS(std::shared_ptr<HEVC::VPS> pVPS, SyntaxNode &parent);
      void createSPS(std::shared_ptr<HEVC::SPS> pSPS, SyntaxNode &parent);
      void createPPS(std::shared_ptr<HEVC::PPS> pPPS, SyntaxNode &parent);
      void createSlice(std::shared_ptr<HEVC::Slice> pSlice, SyntaxNode &parent);
      void createAUD(std::shared_ptr<HEVC::AUD> pAUD, SyntaxNode &parent);
      void createSEI(std::shared_ptr<HEVC::SEI> pSEI, SyntaxNode &parent);

      void createProfileTierLevel(const HEVC::ProfileTierLevel &ptl, SyntaxNode &parent);
      void createVuiParameters(const HEVC::VuiParameters &vui, std::size_t maxNumSubLayersMinus1, SyntaxNode &parent);
      void createHrdParameters(const HEVC::HrdParameters &hrd, uint8_t commonInfPresentFlag, SyntaxNode &parent);
      void createSubLayerHrdParameters(const HEVC::SubLayerHrdParameters &slhrd, uint8_t sub_pic_hrd_params_present_flag, SyntaxNode &parent);
      void createShortTermRefPicSet(std::size_t stRpsIdx, const HEVC::ShortTermRefPicSet &rpset, std::size_t num_short_term_ref_pic_sets, const std::vector<HEVC::ShortTermRefPicSet> &refPicSets, SyntaxNode &parent);
      void createScalingListData(const HEVC::ScalingListData &scdata, SyntaxNode &parent);
      void createRefPicListModification(const HEVC::RefPicListModification &rplModif, SyntaxNode &parent);
      void createPredWeightTable(const HEVC::PredWeightTable &pwt, std::shared_ptr<HEVC::Slice> pSlice, SyntaxNode &parent);

      void createDecodedPictureHash(std::shared_ptr<HEVC::DecodedPictureHash> pDecPictHash, SyntaxNode &parent);
      void createUserDataUnregistered(std::shared_ptr<HEVC::UserDataUnregistered> pSei, SyntaxNode &parent);
      void createReserved(std::shared_ptr<HEVC::SeiReservedInfo> pSei, SyntaxNode &parent);
      void createSceneInfo(std::shared_ptr<HEVC::SceneInfo> pSei, SyntaxNode &parent);
      void createFullFrameSnapshot(std::shared_ptr<HEVC::FullFrameSnapshot> pSei, SyntaxNode &parent);
      void createProgressiveRefinementSegmentStart(std::shared_ptr<HEVC::ProgressiveRefinementSegmentStart> pSei, SyntaxNode &parent);
      void createProgressiveRefinementSegmentEnd(std::shared_ptr<HEVC::ProgressiveRefinementSegmentEnd> pSei, SyntaxNode &parent);
      void createBufferingPeriod(std::shared_ptr<HEVC::BufferingPeriod> pSei, SyntaxNode &parent);
      void createPicTiming(std::shared_ptr<HEVC::PicTiming> pSei, SyntaxNode &parent);
      void createRecoveryPoint(std::shared_ptr<HEVC::RecoveryPoint> pSei, SyntaxNode &parent);
      void createToneMapping(std::shared_ptr<HEVC::ToneMapping> pSei, SyntaxNode &parent);
      void createFramePacking(std::shared_ptr<HEVC::FramePacking> pSei, SyntaxNode &parent);
      void createDisplayOrientation(std::shared_ptr<HEVC::DisplayOrientation> pSei, SyntaxNode &parent);
      void createSOPDescription(std::shared_ptr<HEVC::SOPDescription> pSei, SyntaxNode &parent);
      void createActiveParameterSets(std::shared_ptr<HEVC::ActiveParameterSets> pSei, SyntaxNode &parent);
      void createTemporalLevel0Index(std::shared_ptr<HEVC::TemporalLevel0Index> pSei, SyntaxNode &parent);
      void createRegionRefreshInfo(std::shared_ptr<HEVC::RegionRefreshInfo> pSei, SyntaxNode &parent);
      void createTimeCode(std::shared_ptr<HEVC::TimeCode> pSei, SyntaxNode &parent);
      void createMasteringDisplayInfo(std::shared_ptr<HEVC::MasteringDisplayInfo> pSei, SyntaxNode &parent);
      void createSegmRectFramePacking(std::shared_ptr<HEVC::SegmRectFramePacking> pSei, SyntaxNode &parent);
      void createKneeFunctionInfo(std::shared_ptr<HEVC::KneeFunctionInfo> pSei, SyntaxNode &parent);
      void createChromaResamplingFilterHint(std::shared_ptr<HEVC::ChromaResamplingFilterHint> pSei, SyntaxNode &parent);
      void createColourRemappingInfo(std::shared_ptr<HEVC::ColourRemappingInfo> pSei, SyntaxNode &parent);
      void createContentLightLevelInfo(std::shared_ptr<HEVC::ContentLightLevelInfo> pSei, SyntaxNode &parent);
      void createAlternativeTransferCharacteristics(std::shared_ptr<HEVC::AlternativeTransferCharacteristics> pSei, SyntaxNode &parent);

      std::map<uint32_t, std::shared_ptr<HEVC::VPS> >   m_vpsMap;
      std::map<uint32_t, std::shared_ptr<HEVC::SPS> >   m_spsMap;
      std::map<uint32_t, std::shared_ptr<HEVC::PPS> >   m_ppsMap;
  };

}

#endif
