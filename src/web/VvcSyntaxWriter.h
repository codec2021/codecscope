#ifndef WEB_VVC_SYNTAX_WRITER_H_
#define WEB_VVC_SYNTAX_WRITER_H_

#include <Vvc.h>
#include <VvcParser.h>

#include "Json.h"

#include <memory>
#include <map>
#include <string>

namespace web
{

  std::string vvcNalTypeName(VVC::NALUnitType type);

  class VvcSyntaxWriter
  {
    public:
      void setParameterSets(const std::map<uint8_t, std::shared_ptr<VVC::SPS_NAL> > &spsMap,
                            const std::map<uint8_t, std::shared_ptr<VVC::PPS_NAL> > &ppsMap);

      std::string write(std::shared_ptr<VVC::NALUnit> pNALUnit);

    private:
      void createVPS(const VVC::VPS &v, SyntaxNode &p);
      void createSPS(const VVC::SPS &s, SyntaxNode &p);
      void createPPS(const VVC::PPS &p, SyntaxNode &parent);
      void createPH(const VVC::PH &ph, SyntaxNode &p);
      void createSlice(const VVC::Slice &s, SyntaxNode &p);
      void createAUD(const VVC::AUD &a, SyntaxNode &p);
      void createSEI(const VVC::SEI_NAL &sei, SyntaxNode &p);
      void createPTL(const VVC::ProfileTierLevel &ptl, SyntaxNode &p);

      std::map<uint8_t, std::shared_ptr<VVC::SPS_NAL> > m_spsMap;
      std::map<uint8_t, std::shared_ptr<VVC::PPS_NAL> > m_ppsMap;
  };

}

#endif
