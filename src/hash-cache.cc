/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "hash-cache.hh"

#include "master.hh"
#include "script-tools.hh"
#include "script.hh"
#include "utils/string.hh"

#include <algorithm>
#include <map>
#include <stdexcept>

const std::string HashCache::TargetDoesNotExist = "<does-not-exist>";

//

HashCache::~HashCache() = default;

void HashCache::storeHash(const std::string& hashSum)
{
    m_storedHashSum = hashSum;
}

bool HashCache::compareHash(const std::string& hashSum, bool notExistOk) const
{
    if (m_storedHashSum.empty()
        && (!notExistOk
            && m_storedHashSum == TargetDoesNotExist))
    {
        return false;
    }

    return m_storedHashSum == hashSum;
}

std::string HashCache::getHashSum() const
{
    return m_storedHashSum;
}

// ------------------------------------------------------------

class Artifact::Manager {
public:
    void touch(const std::string& stepName,
               const Link::Type type)
    {
        m_marks[stepName] = type;
    }

    bool stepFound(const std::string& stepName,
                   const Link::Type type) const
    {
        const auto iter = m_marks.find(stepName);

        return iter != m_marks.end()
            && iter->second == type;
    }

    void invalidate(Master& master,
                    const Link::Type type)
    {
        // sanity check

        if (type != Link::Type::Aggregate
            && type != Link::Type::Post)
        {
            throw std::logic_error("Artifact::Manager::invalidate(): function called with invalid link type parameter");
        }

        //

        for (auto iter = m_marks.begin();
             iter != m_marks.end();
             )
        {
            if ((type == Link::Type::Aggregate
                 && (iter->second == Link::Type::Aggregate
                     || iter->second == Link::Type::Post))
                || (type == Link::Type::Post
                    && iter->second == Link::Type::Post))
            {
                tools::undo(master, iter->first);
                m_marks.erase(iter++);
            }
            else {
                ++iter;
            }
        }
    }

    std::vector<Artifact::Link> getAsVector() const
    {
        std::vector<Link> marks;

        for (const auto& pair : m_marks)
        {
            marks.emplace_back( pair.first,
                                pair.second );
        }

        return marks;
    }

private:
    std::map<std::string, Link::Type> m_marks;
};

// ------------------------------------------------------------

Artifact::Link::Link(const std::string& name_,
                     Type type_)
    : name(name_),
      type(type_)
{
}

Artifact::Link::Type Artifact::Link::parse(std::string typeString)
{
    utils::tolower(typeString);

    if (typeString == ""
        || typeString == "simple")
    {
        return Type::Simple;
    }
    else if (typeString == "aggregate")
    {
        return Type::Aggregate;
    }
    else if (typeString == "post")
    {
        return Type::Post;
    }
    else {
        throw std::range_error("invalid artifact link type: " + typeString);
    }
}

std::string Artifact::Link::typeToString(Type type)
{
    switch (type) {
    case Type::Aggregate: return "aggregate";
    case Type::Post:      return "post";
        //
    default:
        return "";
    }
}

// ------------------------------------------------------------

Artifact::~Artifact() = default;

const std::string& Artifact::name() const
{
    return m_name;
}

void Artifact::recalculate()
{
    storeHash( calculateHash() );
}

void Artifact::completeStep(const std::string& stepName,
                            Link::Type linkType)
{
    recalculate();

    //

    switch (linkType) {
    case Link::Type::Aggregate:
    case Link::Type::Post:
        m_manager->touch(stepName,
                         linkType);
        break;

    default:
        break;
    }
}

void Artifact::restoreMark(const std::string& stepName,
                           Link::Type type)
{
    m_manager->touch(stepName,
                     type);
}

std::vector<Artifact::Link> Artifact::getMarks() const
{
    return m_manager->getAsVector();
}

void Artifact::checkInvalidation(Master& master,
                                 const std::string& stepName,
                                 Link::Type linkType)
{
    // maybe rebuild marked steps

    if (linkType == Link::Type::Aggregate)
    {
        if (m_manager->stepFound(stepName, Link::Type::Aggregate))
        {
            m_manager->invalidate(master,
                                  Link::Type::Aggregate);

            throw invalidate_scope(m_scope);
        }
        else {
            m_manager->invalidate(master,
                                  Link::Type::Post);
        }
    }

    // maybe rebuild *all* linked artifacts

    if (!compareHash(calculateHash(), true))
    {
        auto count = tools::rebuildArtifact(master, name());

        if (count > 0) {
            throw invalidate_scope(m_scope);
        }
    }
}

Artifact::Artifact(const std::string& name,
                   const std::string& scope)
    : m_name(name),
      m_scope(scope),
      m_manager(std::make_unique<Manager>())
{
}

// ------------------------------------------------------------

const std::string &Dependency::id() const
{
    return m_id;
}

bool Dependency::isUpToDate() const
{
    return compareHash(calculateHash());
}

Dependency::Dependency(const std::string& id)
    : m_id(id)
{
}
