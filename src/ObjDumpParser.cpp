#include "ObjDumpParser.h"
#include "ScParser.h"
#include "ParserHelpers.h"

#include <iostream>
#include <regex>

ObjDumpParser::ObjDumpParser(ScParser &scParser)
    : m_scParser(scParser)
{
}

void ObjDumpParser::parse(std::ifstream &input)
{
    // 0000000005c5c5d2 <foo::bar<T>(void*)>:
    static const std::regex functionDecl("^\\s*[[:xdigit:]]{16}\\s+<(.*)>:$");
    // 5c5c5e5:       e8 34 21 00 00          callq  5c5e71e <void foo::bar<T>(void*)>
    static const std::regex functionCall("^\\s*[[:xdigit:]]+:.*callq\s+[[:xdigit:]]+\s+<(.*)>$");
    while( input ) {
        std::string line;
        std::getline(input, line);
        std::smatch match;
        if( std::regex_match(line, match, functionDecl) ) {
            m_scParser.parseFunctionDecl(match.str(1));
        } else if( std::regex_match(line, match, functionCall) ) {
            m_scParser.parseFunctionCall(match.str(1));
        }
    }
}
