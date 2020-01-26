#include "ScParser.h"
#include "ScModel.h"
#include "ParserHelpers.h"

#include <regex>
#include <iostream>

ScParser::ScParser(ScModel &model)
    : m_model(model)
    , m_hasCurrentState(false)
{
}

template<std::size_t N>
inline bool expectStartsWith(char *&data, const char (&prefix)[N])
{
    return expectString(data, prefix);
}

template<std::size_t N>
inline bool expectEndsWith(const char *start, char *end, const char (&suffix)[N])
{
    const std::size_t dataLen = end - start;
    const std::size_t suffixLen = N - 1;
    if ( dataLen >= suffixLen )
    {
        if ( eqString(end - suffixLen, suffix) )
        {
            end[-suffixLen] = '\0';
            return true;
        }
    }
    return false;
}

inline char *matchAngleArgument(char *&data, bool skipToEnd = false)
{
    char *start = data;
    int nrAngles = 1;

    for(;;) {
        switch ( *data ) {
        case ',':
            if ( !skipToEnd && (nrAngles == 1) ) {
                *(data++) = '\0';
                if ( *data == ' ' ) {
                    ++data;
                }
                return start;
            }
            break;
        case '>':
            if ( !--nrAngles ) {
                *(data++) = '\0';
                return start;
            }
            break;
        case '<':
            ++nrAngles;
            break;
        case '\0':
            return nullptr;
        }
        ++data;
    }
}

void ScParser::parseFunctionDecl(char *&data)
{
    m_hasCurrentState = false;

    // boost::statechart::detail::reaction_result
    //     boost::statechart::simple_state< *state-spec* >::
    //         local_react<boost::mpl::list<boost::statechart::custom_reaction< *event-name* >,
    //                     boost::statechart::deferral< * event-name * >,
    //                     mpl_::na, ..., mpl_::na> >(boost::statechart::event_base const&, void const*)>

    if ( expectStartsWith(data, "boost::statechart::") ) {
        expectStartsWith(data, "detail::reaction_result boost::statechart::");
        // boost::statechart::state_machine<fsm::StateMachine, fsm::NotStarted, ...
        if ( expectStartsWith(data, "state_machine<") ) {
            parseStateMachine(data);
        } else
        // boost::statechart::simple_state<fsm::NotStarted, fsm::StateMachine, ...
        if ( expectStartsWith(data, "simple_state<") ) {
            parseSimpleState(data);
        }
    } else {
        parseReactMethod(data);
    }
}

void ScParser::parseFunctionCall(char *&data)
{
    if ( !m_hasCurrentState ) {
        return;
    }

    // boost::statechart::detail::safe_reaction_result
    //     boost::statechart::simple_state<...>::transit<*state-name*>()

    expectStartsWith(data, "boost::statechart::detail::safe_reaction_result ");
    if ( expectStartsWith(data, "boost::statechart::simple_state<") ) {
        matchAngleArgument(data, true);
        if ( expectStartsWith(data, "::transit<") ) {
            QByteArray target = matchAngleArgument(data);
            m_model.addTransition(target, m_currentEvent);
        } else if ( expectStartsWith(data, "::discard_event()") ) {
            m_model.addTransition(m_currentState, m_currentEvent);
        } else if ( expectStartsWith(data, "::defer_event()") ) {
            m_model.addDeferral(m_currentEvent);
        }
    }
}

void parseFunctionDecl(const std::string &data)
{
    static const std::regex expr("^boost::statechart::(|detail::reaction_result boost::statechart::)(state_machine|simple_state)<(.*)");
    m_hasCurrentState = false;
    std::smatch match;
    if( std::regex_match(data, match, expr) ) {
        if( "state_machine" == match.str(2) ) {
            // state_machine
        } else {
            // simple_state
        }
    } else {
        // React Method
    }

}

std::string matchAngleArgument(const std::string& data)
{
    uint32_t angles = 1;
    for(std::string::const_iterator pos = data.begin(); pos != data.end(); ++pos) {
        if( '<' == *pos )
            ++angles;
        else if( '>' == *pos )
            --angles;

        if(angles == 0)
            return std::string(data.begin(), pos);
    }
    return std::string();
}

void parseFunctionCall(const std::string &data)
{
    static const std::regex expr("(|boost::statechart::detail::safe_reaction_result )boost::statechart::simple_state<.*::(transit<|discard_event\(\)|defer_event\(\))(.*)");

    if ( !m_hasCurrentState ) {
        return;
    }
    std::smatch match;
    if( std::regex_match(data, match, expr) ) {
        if( "transit<" == match.str(2) ) {
            QByteArray target = matchAngleArgument(match.str(3)).c_str();
            m_model.addTransition(target, m_currentEvent);
        } else if( "discard_event()" == match.str(2) ) {
            m_model.addTransition(m_currentState, m_currentEvent);
        } else if( "defer_event()" == match.str(2) ) {
            m_model.addDeferral(m_currentEvent);
        }
    }
}

void ScParser::parseStateMachine(char *&data)
{
    ScName name = matchAngleArgument(data);
    ScName initialState = matchAngleArgument(data);

    m_model.addStateMachine(name, initialState);
}

static ScNameList parseInitialSubstateList(char *mplList)
{
    ScNameList list;

    if ( expectStartsWith(mplList, "boost::mpl::list") ) {
        expectAddress(mplList);
        if ( !expectString(mplList, "<") ) {
            return list;
        }

        forever {
            char *substate = matchAngleArgument(mplList);
            if ( !substate ) { // end of list
                break;
            }
            if ( expectStartsWith(substate, "mpl_::na") ) { // default values started
                break;
            }

            list.append(substate);
        }
    } else {
        list.append(mplList);
    }

    return list;
}

void ScParser::parseReactionList(char *mplList)
{
    /*
     * ::local_react<boost::mpl::list<boost::statechart::custom_reaction< *event-name* >,
     *                                boost::statechart::deferral< * event-name * >,
     *                                mpl_::na, ..., mpl_::na> >(boost::statechart::event_base const&, void const*)>
     */

    if ( expectStartsWith(mplList, "boost::mpl::list") ) {
        expectAddress(mplList);
        if ( !expectString(mplList, "<") ) {
            return;
        }

        forever {
            char *reaction = matchAngleArgument(mplList);
            if ( !reaction ) {
                break;
            }
            if ( expectStartsWith(reaction, "mpl_::na") ) { // default values started
                break;
            }

            if ( expectStartsWith(reaction, "boost::statechart::transition<") ) {
                QByteArray event = matchAngleArgument(reaction);
                QByteArray target = matchAngleArgument(reaction);
                m_model.addTransition(target, event);
            } else if ( expectStartsWith(reaction, "boost::statechart::deferral<") ) {
                QByteArray event = matchAngleArgument(reaction);
                m_model.addDeferral(event);
            }
        }
    }
}

void ScParser::parseSimpleState(char *&data)
{
    /*
     * Basic:
     *
     * simple_state<state-name, parent-state,
     *              boost::mpl::list<initial-substate, mpl_::na, ...>,
     *              (boost::statechart::history_mode)0>
     *
     * With orthogonal region in a parent state:
     *
     * simple_state<state-name,
     *              boost::statechart::simple_state<parent-state, grandparent-state,
     *                                              boost::mpl::list<initial-substate, mpl_::na, ...>,
     *                                              (boost::statechart::history_mode)0>::orthogonal<(unsigned char)0>,
     *              boost::mpl::list<initial-substate, mpl_::na, ...>,
     *              (boost::statechart::history_mode)0>
     */

    ScName name = matchAngleArgument(data);

    bool isOrthogonalState = expectStartsWith(data, "boost::statechart::simple_state<");
    ScName parent = matchAngleArgument(data);
    int orthRegion = 0;

    if ( isOrthogonalState ) {
        matchAngleArgument(data, true); // skip to ::orthogonal
        if ( !expectStartsWith(data, "::orthogonal<(unsigned char)") ) {
            return;
        }
        orthRegion = std::atoi(data);
        matchAngleArgument(data, true); // skip to ','
        matchAngleArgument(data); // skip to a list of initial substates
    }

    m_model.addState(name, parent, orthRegion,
                     parseInitialSubstateList(matchAngleArgument(data)));

    matchAngleArgument(data, true); // skip history_mode

    if ( expectStartsWith(data, "::local_react<") ) {
        parseReactionList(matchAngleArgument(data));
    }
}

inline char *matchName(char *&data, char terminus)
{
    char *start = data;

    for ( int nrAngles = 0; *data; ++data ) {
        if ( isalnum(*data) || (*data == ':') || (*data == '_') ) {
            continue;
        }
        if ( *data == '<' ) {
            ++nrAngles;
            continue;
        }
        if ( (*data == '>') && (nrAngles > 0) ){
            --nrAngles;
            continue;
        }
        if ( nrAngles == 0 ) {
            break;
        }
    }

    if ( *data != terminus ) {
        return nullptr;
    }
    if ( *data == '\0' ) {
        return start;
    }
    *(data++) = '\0';
    return start;
}

void ScParser::parseReactMethod(char *&data)
{
    /*
     * *state*::react(*event* const&)
     */

    char *function = matchName(data, '(');
    if ( !function ) {
        return;
    }
    if ( !expectEndsWith(function, data - 1, "::react") ) {
        return;
    }

    m_currentState = function;
    m_currentEvent = matchName(data, ' ');
    if ( m_currentEvent.isEmpty() ) {
        return;
    }

    m_hasCurrentState = true;
    m_model.setActiveState(m_currentState);
}
