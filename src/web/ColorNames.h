#ifndef WEB_COLOR_NAMES_H_
#define WEB_COLOR_NAMES_H_

#include <string>

namespace web
{

  inline std::string colourPrimariesToString(uint32_t value)
  {
    switch(value)
    {
      case 1: return "bt709";
      case 2: return "Unspecified";
      case 4: return "bt470m";
      case 5: return "bt470bg";
      case 6: return "smpte170m";
      case 7: return "smpte240m";
      case 8: return "film";
      case 9: return "bt2020";
      default: return "Reserved";
    }
  }

  inline std::string transferCharacteristicsToString(uint32_t value)
  {
    switch(value)
    {
      case 1: return "bt709";
      case 2: return "Unspecified";
      case 4: return "bt470m";
      case 5: return "bt470bg";
      case 6: return "smpte170m";
      case 7: return "smpte240m";
      case 8: return "linear";
      case 9: return "log100";
      case 10: return "log316";
      case 11: return "iec61966-2-4";
      case 12: return "bt1361e";
      case 13: return "iec61966-2-1";
      case 14: return "bt2020-10";
      case 15: return "bt2020-12";
      case 16: return "smpte-st-2084";
      case 17: return "smpte-st-428";
      case 18: return "arib-std-b67";
      default: return "Reserved";
    }
  }

  inline std::string matrixCoefficientsToString(uint32_t value)
  {
    switch(value)
    {
      case 0: return "GBR";
      case 1: return "bt709";
      case 2: return "Unspecified";
      case 4: return "fcc";
      case 5: return "bt470bg";
      case 6: return "smpte170m";
      case 7: return "smpte240m";
      case 8: return "YCgCo";
      case 9: return "bt2020nc";
      case 10: return "bt2020c";
      default: return "Reserved";
    }
  }

}

#endif
