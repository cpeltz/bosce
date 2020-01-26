#pragma once

#include "ScModel.h"

class ScParser final
{
public:
    ScParser(ScModel &model);

public:
    void parseFunctionDecl(char *&data);
    void parseFunctionCall(char *&data);

    void parseFunctionDecl(const std::string &data);
    void parseFunctionCall(const std::string &data);

private:
    void parseStateMachine(char *&data);
    void parseSimpleState(char *&data);
    void parseReactMethod(char *&data);
    void parseReactionList(char *mplList);

private:
    ScModel &m_model;
    ScName m_currentState;
    ScName m_currentEvent;
    bool m_hasCurrentState;
};
