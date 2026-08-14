#include "VvcParser.h"

#include "VvcParserImpl.h"

namespace VVC
{

  Parser::~Parser()
  {
  }

  Parser *Parser::create()
  {
    return new VvcParserImpl;
  }

  void Parser::release(Parser *pparser)
  {
    delete pparser;
  }

}
