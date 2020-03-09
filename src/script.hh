/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include "hash-cache.hh"
#include "utils/flags.hh"

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

// type aliases

using unique_group_t  = std::unique_ptr<Group>;
using unique_step_t   = std::unique_ptr<Step>;
using unique_unit_t   = std::unique_ptr<Unit>;

// class definitions

class Unit {
public:
    struct Visitor {
        virtual ~Visitor() = default;

        virtual void operator() (Group& /*group*/) const {}
        virtual void operator() (Script& /*script*/) const {}
        virtual void operator() (Step& /*step*/) const {}
    };

    //

    struct Traveler : Visitor {
        Traveler(const Visitor& visitor);
    protected:
        const Visitor& m_visitor;
    };

    // -----

    virtual ~Unit() = default;

    const std::string& name() const;
    virtual Unit* parent() = 0;

    virtual void apply(const Visitor& visitor) = 0;
    virtual void applyChildren(const Visitor& visitor) = 0;
    void applyParent(const Visitor& visitor);

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
    struct OrderUniqUnitByName {
        bool operator() (const unique_unit_t& left,
                         const unique_unit_t& right) const;
    };

    static bool OrderUniqUnitByName_string(const unique_unit_t& left,
                                           const std::string& right);

    //

    using units_t = std::set<unique_unit_t, OrderUniqUnitByName>;

    //

    Group(const std::string& name,
          Group& parent);

    Group* parent() override;

    void add(unique_unit_t&& unit);

    void apply(const Visitor& visitor) override;
    void applyChildren(const Visitor& visitor) override;

    Unit* findUnit(const std::string& name);

    //

    static unique_group_t newRoot(const std::string& name);

private:
    Group* m_parent = nullptr;
    units_t m_units;

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

    void apply(const Visitor& visitor) override;
    void applyChildren(const Visitor& visitor) override;

    Step* findStep(const std::string& name);

private:
    Group* m_parent = nullptr;
    std::vector<unique_step_t> m_steps;
};

// -----

class Step : public Unit {
public:
    enum class Flag {
        Always,
        Sudo,
    };

    using Flags = utils::FlagsT<Flag>;

    //

    Step(const std::string& name,
         Script& parent,
         Flags flags);

    Script* parent() override;
    bool flag(Flag f) const;

    bool isCompleted() const;
    void complete();
    void undo();

    bool hasArtifact(const std::string& artifact);
    void addArtifact(const std::string& artifact);
    void addDependency(unique_dependency_t&& dependency);

    void apply(const Visitor& visitor) override;
    void applyChildren(const Visitor& visitor) override;

    void forEachDependency(const std::function<void(Dependency&)>& callback);

    bool everythingUpToDate(Master& master);
    void recalculateHashes(Master& master);

private:
    Script* m_parent = nullptr;
    const Flags m_flags;
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

namespace lambda_impl
{
    struct LambdaVisitorGroup : Unit::Visitor {
        LambdaVisitorGroup(const std::function<void(Group&)>& func);

        void operator() (Group& group) const override;

    private:
        std::function<void(Group&)> m_func;
    };

    //

    struct LambdaVisitorScript : Unit::Visitor {
        LambdaVisitorScript(const std::function<void(Script&)>& func);

        void operator() (Script& script) const override;

    private:
        std::function<void(Script&)> m_func;
    };

    //

    struct LambdaVisitorStep : Unit::Visitor {
        LambdaVisitorStep(const std::function<void(Step&)>& func);

        void operator() (Step& step) const override;

    private:
        std::function<void(Step&)> m_func;
    };
}

// ------------------------------------------------------------

lambda_impl::LambdaVisitorGroup  lambdaVisitor(const std::function<void(Group&)>& func);
lambda_impl::LambdaVisitorScript lambdaVisitor(const std::function<void(Script&)>& func);
lambda_impl::LambdaVisitorStep   lambdaVisitor(const std::function<void(Step&)>& func);
