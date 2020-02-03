/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "hash-cache.hh"

#include "master.hh"
#include "script.hh"

#include <set>
#include <stdexcept>

const std::string HashCache::TargetDoesNotExist = "<does-not-exist>";

//

HashCache::~HashCache() = default;

void HashCache::storeHash(const std::string& hashSum)
{
    m_storedHashSum = hashSum;
}

bool HashCache::compareHash(const std::string& hashSum) const
{
    if (m_storedHashSum.empty()
        || m_storedHashSum == TargetDoesNotExist)
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
    Manager(const std::string& s)
        : m_scope(s) {}

    const std::string& scope() const
    {
        return m_scope;
    }

    void touch(const std::string& stepName)
    {
        m_steps.insert(stepName);
    }

    bool stepFound(const std::string& stepName) const
    {
        return m_steps.count(stepName) > 0;
    }

    void invalidate()
    {
        m_steps.clear();
    }

    std::vector<std::string> getAsVector()
    {
        return std::vector<std::string>(m_steps.begin(),
                                        m_steps.end());
    }

private:
    std::string m_scope;
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

    if (m_manager) {
        if (m_manager->stepFound(stepName)) {
            throw std::runtime_error("Touching managed artifact '" + m_name + "' with step '" + stepName + "' which already exists in managed step list");
        }

        m_manager->touch(stepName);
    }
}

void Artifact::setManaged(const std::string& scope)
{
    if (!m_manager) {
        m_manager = std::make_unique<Manager>(scope);
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
    return m_manager
        ? m_manager->getAsVector()
        : std::vector<std::string>();
}

bool Artifact::checkInvalidation(const std::string& stepName,
                                 bool rebuildNeeded)
{
    if (m_manager)
    {
        const bool found = m_manager->stepFound(stepName);

        if (!found)
        {
            // rebuild step

            return false;
        }
        else if (rebuildNeeded)
        {
            // invalidate artifact and rebuild scope

            m_manager->invalidate();

            throw invalidate_scope(m_manager->scope());
        }
    }

    return true;    // no effect: rebuildNeeded applies
}

Artifact::Artifact(const std::string& name)
    : m_name(name)
{
}

// ------------------------------------------------------------

const std::string &Dependency::id() const
{
    return m_id;
}

Dependency::Dependency(const std::string& id)
    : m_id(id)
{
}
