/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

// forward declarations

class Master;
class Step;

//

class invalidate_scope : public std::runtime_error {
public:
    invalidate_scope(const std::string& scope)
        : std::runtime_error("Invalidating execution scope " + scope),
          m_scope(scope) {}

    const std::string& scope() const
    {
        return m_scope;
    }

private:
    const std::string m_scope;
};

// ------------------------------------------------------------

class HashCache {
public:
    static const std::string TargetDoesNotExist;

    //

    virtual ~HashCache();

    virtual std::string calculateHash() const = 0;

    void storeHash(const std::string& hashSum);
    bool compareHash(const std::string& hashSum) const;

    std::string getHashSum() const;

private:
    std::string m_storedHashSum;
};

// -----

class Artifact : public HashCache {
    struct Manager;

public:
    ~Artifact() override;

    const std::string& name() const;

    void recalculate(const std::string& stepName);

    void setManaged(const std::string& scope);
    void setTouched(const std::string& stepName);
    std::vector<std::string> getManagedList() const;

    bool checkInvalidation(const std::string& stepName,
                           bool rebuildNeeded);

protected:
    Artifact(const std::string& name);

private:
    std::string m_name;
    std::unique_ptr<Manager> m_manager;
};

// -----

class Dependency : public HashCache {
public:
    const std::string& id() const;

    virtual std::string type() const = 0;

    virtual bool isUpToDate() const = 0;

protected:
    std::string m_id;

    //

    Dependency(const std::string& id);
};

// -----

using unique_artifact_t = std::unique_ptr<Artifact>;
using unique_dependency_t = std::unique_ptr<Dependency>;
