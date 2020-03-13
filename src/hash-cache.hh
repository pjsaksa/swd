/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

//

class Master;

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
    bool compareHash(const std::string& hashSum, bool notExistOk = false) const;

    std::string getHashSum() const;

private:
    std::string m_storedHashSum;
};

// -----

class Artifact : public HashCache {
    struct Manager;

public:
    struct Link {
        enum class Type {
            Aggregate,
            Post,
            Simple,
        };

        //

        const std::string name;
        const Type type;

        Link(const std::string& name_,
             Type type_);

        //

        static Type parse(std::string typeString);
        static std::string typeToString(Type type);
    };



    //

    ~Artifact() override;

    const std::string& name() const;

    void recalculate();
    void completeStep(const std::string& stepName,
                      Link::Type linkType);

    void restoreMark(const std::string& stepName,
                     Link::Type type);
    std::vector<Link> getMarks() const;

    void checkInvalidation(Master& master,
                           const std::string& stepName,
                           Link::Type linkType);

protected:
    Artifact(const std::string& name,
             const std::string& scope);

private:
    std::string m_name;
    std::string m_scope;
    std::unique_ptr<Manager> m_manager;
};

// -----

class Dependency : public HashCache {
public:
    const std::string& id() const;

    bool isUpToDate() const;

    virtual std::string type() const = 0;

protected:
    std::string m_id;

    //

    Dependency(const std::string& id);
};

// -----

using unique_artifact_t = std::unique_ptr<Artifact>;
using unique_dependency_t = std::unique_ptr<Dependency>;
