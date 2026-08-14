#ifndef WEB_AVC_SYNTAX_WRITER_H_
#define WEB_AVC_SYNTAX_WRITER_H_

#include <Avc.h>
#include <AvcParser.h>

#include "Json.h"

#include <memory>
#include <map>
#include <string>

namespace web
{

  std::string avcNalTypeName(AVC::NALUnitType type);

  class AvcSyntaxWriter
  {
    public:
      void setParameterSets(const std::map<uint32_t, std::shared_ptr<AVC::SPS_NAL> > &spsMap,
                            const std::map<uint32_t, std::shared_ptr<AVC::PPS_NAL> > &ppsMap);

      std::string write(std::shared_ptr<AVC::NALUnit> pNALUnit);

    private:
      void createSPS(const AVC::SPS &sps, SyntaxNode &parent);
      void createPPS(const AVC::PPS &pps, SyntaxNode &parent);
      void createSlice(const AVC::Slice &slice, SyntaxNode &parent);
      void createAUD(const AVC::AUD &aud, SyntaxNode &parent);
      void createSEI(const AVC::SEI_NAL &sei, SyntaxNode &parent);
      void createScalingMatrix(const AVC::ScalingMatrix &sm, SyntaxNode &parent);
      void createVui(const AVC::VuiParameters &vui, SyntaxNode &parent);
      void createRefPicListModification(const AVC::RefPicListModification &r, bool isB, SyntaxNode &parent);
      void createDecRefPicMarking(const AVC::Slice &slice, SyntaxNode &parent);
      void createPredWeightTable(const AVC::PredWeightTable &p, uint32_t sliceType, SyntaxNode &parent);

      std::map<uint32_t, std::shared_ptr<AVC::SPS_NAL> > m_spsMap;
      std::map<uint32_t, std::shared_ptr<AVC::PPS_NAL> > m_ppsMap;
  };

}

#endif
