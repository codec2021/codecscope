#ifndef WEB_JSON_H_
#define WEB_JSON_H_

#include <string>
#include <vector>

namespace web
{

  std::string jsonEscape(const std::string &s);

  struct SyntaxNode
  {
    std::string            name;
    std::vector<SyntaxNode> children;

    SyntaxNode() {}
    SyntaxNode(const std::string &n): name(n) {}

    SyntaxNode &add(const std::string &n)
    {
      children.push_back(SyntaxNode(n));
      return children.back();
    }

    void toJson(std::string &out) const;
  };

}

#endif
