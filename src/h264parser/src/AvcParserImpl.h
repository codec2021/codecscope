#ifndef AVC_PARSER_IMPL_H_
#define AVC_PARSER_IMPL_H_

#include "AvcParser.h"
#include "BitstreamReader.h"

#include <map>
#include <list>
#include <memory>

namespace AVC
{
  class AvcParserImpl: public Parser
  {
    public:
      std::size_t process(const uint8_t *pdata, std::size_t size, std::size_t offset = 0) override;
      void addConsumer(Consumer *pconsumer) override;
      void releaseConsumer(Consumer *pconsumer) override;

    protected:
      void processNALUnit(const uint8_t *pdata, std::size_t size, const Parser::Info &info);
      void processNALUnitHeader(BitstreamReader &bs, NALHeader *header);
      void processSPS(std::shared_ptr<SPS_NAL> psps, BitstreamReader &bs, const Parser::Info &info);
      void processPPS(std::shared_ptr<PPS_NAL> ppps, BitstreamReader &bs, const Parser::Info &info);
      void processSlice(std::shared_ptr<Slice_NAL> pslice, BitstreamReader &bs, const Parser::Info &info);
      void processSliceHeader(std::shared_ptr<Slice_NAL> pslice, BitstreamReader &bs, const Parser::Info &info);
      void processAUD(std::shared_ptr<AUD_NAL> paud, BitstreamReader &bs);
      void processSEI(std::shared_ptr<SEI_NAL> psei, BitstreamReader &bs);

      void processScalingMatrix(ScalingMatrix &sm, BitstreamReader &bs);
      void processVuiParameters(VuiParameters &vui, BitstreamReader &bs);
      void processRefPicListModification(RefPicListModification &r, BitstreamReader &bs, bool isB, const Parser::Info &info);
      void processDecRefPicMarking(Slice &slice, BitstreamReader &bs, uint32_t st, bool idrPic, const Parser::Info &info);
      void processPredWeightTable(PredWeightTable &p, uint32_t sliceType, uint32_t numL0, uint32_t numL1, uint32_t chromaArrayType, BitstreamReader &bs);

      void onWarning(const std::string &warning, const Info *pInfo, WarningType type);

      std::map<uint32_t, std::shared_ptr<SPS_NAL> >   m_spsMap;
      std::map<uint32_t, std::shared_ptr<PPS_NAL> >   m_ppsMap;

      std::list<Consumer *>   m_consumers;
  };
}

#endif
