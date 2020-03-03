/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "script-tools.hh"
#include "script-travelers.hh"

//

void travelers::ForEach::travel(Unit& unit) const
{
    unit.apply(m_visitor);
    unit.applyChildren(*this);
}

void travelers::ForEach::operator() (Group& group) const
{
    travel(group);
}

void travelers::ForEach::operator() (Script& script) const
{
    travel(script);
}

void travelers::ForEach::operator() (Step& step) const
{
    travel(step);
}

// ------------------------------------------------------------

void travelers::Path::travel(Unit& unit) const
{
    unit.applyParent(*this);
    unit.apply(m_visitor);
}

void travelers::Path::operator() (Group& group) const
{
    travel(group);
}

void travelers::Path::operator() (Script& script) const
{
    travel(script);
}

void travelers::Path::operator() (Step& step) const
{
    travel(step);
}

// ------------------------------------------------------------

travelers::FindUnit::FindUnit(const std::string& path,
                              const Visitor& visitor)
    : Traveler(visitor),
      m_path(path),
      m_pathLeft(path)
{
}

void travelers::FindUnit::operator() (Group& group) const
{
    if (m_pathLeft.empty())
    {
        callIfMatch(group);
    }
    else {
        std::string::size_type dash  = m_pathLeft.find('/');
        std::string::size_type space = m_pathLeft.find(' ');

        std::string unitName;

        if (dash != std::string::npos)
        {
            if (space != std::string::npos
                && space < dash)
            {
                throw std::runtime_error("unknown unit: " + m_path);
            }
            else {
                unitName = m_pathLeft.substr(0, dash);
                m_pathLeft.erase(0, dash + 1);
            }
        }
        else if (space != std::string::npos)
        {
            unitName = m_pathLeft.substr(0, space);
            m_pathLeft.erase(0, space + 1);
        }
        else {
            unitName = m_pathLeft;
            m_pathLeft.clear();
        }

        Unit* unit = group.findUnit( unitName );

        if (!unit) {
            throw std::runtime_error("unknown unit: " + m_path);
        }

        //

        unit->apply(*this);
    }
}

void travelers::FindUnit::operator() (Script& script) const
{
    if (m_pathLeft.empty())
    {
        callIfMatch(script);
    }
    else if (m_pathLeft.find('/') != std::string::npos
             || m_pathLeft.find(' ') != std::string::npos)
    {
        throw std::runtime_error("unknown unit: " + m_path);
    }
    else {
        Step* step = script.findStep(m_pathLeft);
        m_pathLeft.clear();

        if (!step) {
            throw std::runtime_error("unknown unit: " + m_path);
        }

        step->apply(*this);
    }
}

void travelers::FindUnit::operator() (Step& step) const
{
    if (m_pathLeft.empty())
    {
        callIfMatch(step);
    }
    else {
        throw std::runtime_error("unknown unit: " + m_path);
    }
}

void travelers::FindUnit::callIfMatch(Unit& unit) const
{
    if (tools::conjurePath(unit) == m_path) {
        unit.apply(m_visitor);
    }
    else {
        throw std::runtime_error("unknown unit: " + m_path);
    }
}
