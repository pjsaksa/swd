/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include "hash-cache.hh"

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

// forward declarations

class Group;
class Master;
class Script;
class Step;
class Unit;
class UnitVisitor;

// type aliases

using unique_group_t  = std::unique_ptr<Group>;
using unique_step_t   = std::unique_ptr<Step>;
using unique_unit_t   = std::unique_ptr<Unit>;

// class definitions

class Unit {
public:
    virtual ~Unit();

    const std::string& name() const;
    virtual Unit* parent() = 0;

    virtual void visit(const UnitVisitor& visitor) = 0;
    virtual void forEach(const UnitVisitor& visitor) = 0;

    void travelPath(const UnitVisitor& visitor);

protected:
    Unit(const std::string& name);

private:
    std::string m_name;

    //

    Unit(const Unit&) = delete;
};

// -----

class Group : public Unit {
public:
    Group(const std::string& name,
          Group& parent);

    Group* parent() override;

    void add(unique_unit_t&& unit);

    void visit(const UnitVisitor& visitor) override;
    void visitChildren(const UnitVisitor& visitor);

    void forEach(const UnitVisitor& visitor) override;
    void forEachChild(const UnitVisitor& visitor);
    Unit* findUnit(const std::string& name);

    //

    static unique_group_t newRoot(const std::string& name);

    //

    struct OrderUniqUnitByName {
        bool operator() (const unique_unit_t& left,
                         const unique_unit_t& right) const;
    };

    static bool OrderUniqUnitByName_string(const unique_unit_t& left,
                                           const std::string& right);

private:
    Group* m_parent = nullptr;
    std::set<unique_unit_t, OrderUniqUnitByName> m_units;

    //

    Group(const std::string& name);

    Group(const Group&) = delete;
};

// -----

class Script : public Unit {
public:
    static const std::string FileExt;

    //

    Script(const std::string& name,
           Group& parent);

    Group* parent() override;

    bool isCompleted(const std::string&) const;
    void completeStep(const std::string&);
    void undoStep(const std::string&);
    void undoAllSteps();

    void add(unique_step_t&& step);

    void visit(const UnitVisitor& visitor) override;
    void visitChildren(const UnitVisitor& visitor);
    void forEach(const UnitVisitor& visitor) override;
    Step* findStep(const std::string& name);

private:
    Group* m_parent = nullptr;
    std::vector<unique_step_t> m_steps;
};

// -----

class Step : public Unit {
public:
    Step(const std::string& name,
         Script& parent);

    Script* parent() override;

    bool isCompleted() const;
    void complete();
    void undo();

    bool hasArtifact(const std::string& artifact);
    void addArtifact(const std::string& artifact);
    void addDependency(unique_dependency_t&& dependency);

    void visit(const UnitVisitor& visitor) override;
    void forEach(const UnitVisitor& visitor) override;

    void forEachDependency(const std::function<void(Dependency&)>& callback);

    bool everythingUpToDate(Master& master);
    void recalculateHashes(Master& master);

private:
    Script* m_parent = nullptr;
    bool m_completed = false;

    std::vector<std::string> m_artifacts;
    std::vector<unique_dependency_t> m_dependencies;

    //

    friend bool Script::isCompleted(const std::string&) const;
    friend void Script::completeStep(const std::string&);
    friend void Script::undoStep(const std::string&);
    friend void Script::undoAllSteps();
};

// ------------------------------------------------------------

struct UnitVisitor {
    virtual void operator() (Group&) const  {}
    virtual void operator() (Script&) const {}
    virtual void operator() (Step&) const   {}
};
