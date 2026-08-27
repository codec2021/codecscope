#include "CodecDetector.h"

namespace web
{

  namespace
  {
    bool isAvcProfile(uint8_t profileIdc)
    {
      switch(profileIdc)
      {
        case 66: case 77: case 88: case 100: case 110: case 122:
        case 244: case 44: case 83: case 86: case 118: case 128:
        case 138: case 139: case 134: case 135:
          return true;
        default:
          return false;
      }
    }
  }

  std::string detectCodec(const uint8_t *data, std::size_t size)
  {
    std::size_t nalCount = 0;

    for(std::size_t pos = 0; pos + 3 < size && nalCount < 50;)
    {
      std::size_t startOffset = 3;
      bool found = data[pos] == 0 && data[pos+1] == 0 && data[pos+2] == 1;

      if(!found)
      {
        if(size - pos >= 4 && data[pos] == 0 && data[pos+1] == 0 && data[pos+2] == 0 && data[pos+3] == 1)
        {
          found = true;
          startOffset = 4;
        }
      }

      if(found)
      {
        const uint8_t *hdr = data + pos + startOffset;
        std::size_t remaining = size - pos - startOffset;
        if(remaining >= 1)
        {
          uint8_t b0 = hdr[0];
          uint8_t b1 = remaining >= 2 ? hdr[1] : 0;

          // H.264 SPS：第一字节低 5 位 == 7，且第二字节是合法 profile_idc
          if((b0 & 0x1F) == 7 && remaining >= 2 && isAvcProfile(b1))
            return "avc";

          // HEVC VPS/SPS/PPS：第一字节高 6 位 in {32,33,34}
          // 先于 VVC 检测，避免 HEVC 的 nuh_layer_id ∈ {14,15,16} 被误判为 VVC
          uint8_t hevcType = (b0 >> 1) & 0x3F;
          if(hevcType == 32 || hevcType == 33 || hevcType == 34)
            return "hevc";

          // VVC VPS/SPS/PPS：第二字节高 5 位 in {14,15,16}
          if(remaining >= 2)
          {
            uint8_t vvcType = (b1 >> 3) & 0x1F;
            if(vvcType == 14 || vvcType == 15 || vvcType == 16)
              return "vvc";
          }
        }

        nalCount++;
        pos += startOffset + 1;
      }
      else
        pos++;
    }

    return "unknown";
  }

}
