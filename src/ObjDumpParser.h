#pragma once

#include <fstream>

class ScParser;

class ObjDumpParser final
{
public:
    ObjDumpParser(ScParser &scParser);

    void parse(std::ifstream &input);

private:
    ScParser &m_scParser;
};
