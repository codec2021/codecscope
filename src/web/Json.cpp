#include "Json.h"

namespace web
{

  std::string jsonEscape(const std::string &s)
  {
    std::string out;
    out.reserve(s.size() + 8);
    for(std::size_t i = 0; i < s.size(); i++)
    {
      char c = s[i];
      switch(c)
      {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
          if((unsigned char)c < 0x20)
          {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
            out += buf;
          }
          else
            out += c;
      }
    }
    return out;
  }

  void SyntaxNode::toJson(std::string &out) const
  {
    out += "{\"n\":\"";
    out += jsonEscape(name);
    out += "\"";
    if(!children.empty())
    {
      out += ",\"c\":[";
      for(std::size_t i = 0; i < children.size(); i++)
      {
        if(i)
          out += ",";
        children[i].toJson(out);
      }
      out += "]";
    }
    out += "}";
  }

}
