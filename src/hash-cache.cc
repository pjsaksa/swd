/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "hash-cache.hh"

#include "master.hh"
#include "script-tools.hh"
#include "script.hh"

#include <algorithm>
#include <set>
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
    void touch(const std::string& stepName)
    {
        m_steps.insert(stepName);
    }

    bool stepFound(const std::string& stepName) const
    {
        return m_steps.count(stepName) > 0;
    }

    void invalidate(Master& master)
    {
        std::for_each(m_steps.begin(),
                      m_steps.end(),
                      [&master] (const std::string& stepName)
                      {
                          tools::undo(master, stepName);
                      });

        m_steps.clear();
    }

    std::vector<std::string> getAsVector()
    {
        return std::vector<std::string>(m_steps.begin(),
                                        m_steps.end());
    }

private:
    std::set<std::string> m_steps;
};

// ------------------------------------------------------------

Artifact::~Artifact() = default;

const std::string& Artifact::name() const
{
    return m_name;
}

void Artifact::recalculate(const std::string& stepName)
{
    storeHash( calculateHash() );

    if (!stepName.empty()
        && m_manager)
    {
        if (m_manager->stepFound(stepName)) {
            throw std::runtime_error("Touching managed artifact '" + m_name + "' with step '" + stepName + "' which already exists in managed step list");
        }

        m_manager->touch(stepName);
    }
}

void Artifact::setManaged()
{
    if (!m_manager) {
        m_manager = std::make_unique<Manager>();
    }
}

void Artifact::setTouched(const std::string& stepName)
{
    if (m_manager) {
        m_manager->touch(stepName);
    }
}

std::vector<std::string> Artifact::getManagedList() const
{
    return ( m_manager
             ? m_manager->getAsVector()
             : std::vector<std::string>() );
}

bool Artifact::checkInvalidation(Master& master,
                                 const std::string& stepName,
                                 bool rebuildNeeded)
{
    // for managed artifacts, maybe rebuild more stuff

    if (m_manager)
    {
        const bool found = m_manager->stepFound(stepName);

        if (found
            && rebuildNeeded)
        {
            // invalidate artifact and rebuild scope

            m_manager->invalidate(master);

            throw invalidate_scope(m_scope);
        }
        else if (!found &&
                 !rebuildNeeded)
        {
            return false;       // this should not happen, but it did
        }
    }

    // rebuild all linked steps if artifact is not 'up to date'

    if (rebuildNeeded
        && !compareHash(calculateHash(), true))
    {
        auto count = tools::rebuildArtifact(master, name());

        if (count > 0) {
            throw invalidate_scope(m_scope);
        }
    }

    return true;    // no effect: rebuildNeeded applies
}

Artifact::Artifact(const std::string& name,
                   const std::string& scope)
    : m_name(name),
      m_scope(scope)
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
