#include "AvcParser.h"

#include "AvcParserImpl.h"

namespace AVC
{

  Parser::~Parser()
  {
  }

  Parser *Parser::create()
  {
    return new AvcParserImpl;
  }

  void Parser::release(Parser *pparser)
  {
    delete pparser;
  }

}
