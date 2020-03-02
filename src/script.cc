/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "script.hh"

#include "master.hh"
#include "script-tools.hh"

#include <algorithm>

Unit::~Unit() = default;

const std::string& Unit::name() const
{
    return m_name;
}

Unit::Unit(const std::string& name)
    : m_name(name)
{
}

void Unit::travelPath(const UnitVisitor& visitor)
{
    Unit* p = parent();

    if (p) {
        p->travelPath(visitor);
    }

    visit(visitor);
}

// ------------------------------------------------------------

Group::Group(const std::string& name,
             Group& parent)
    : Unit(name),
      m_parent(&parent)
{
}

Group* Group::parent()
{
    return m_parent;
}

void Group::add(unique_unit_t&& unit)
{
    m_units.emplace(std::move(unit));
}

void Group::visit(const UnitVisitor& visitor)
{
    visitor(*this);
}

void Group::visitChildren(const UnitVisitor& visitor)
{
    for (auto& u : m_units)
    {
        u->visit(visitor);
    }
}

void Group::forEach(const UnitVisitor& visitor)
{
    visit(visitor);

    forEachChild(visitor);
}

void Group::forEachChild(const UnitVisitor& visitor)
{
    for (auto& u : m_units)
    {
        u->forEach(visitor);
    }
}

Unit* Group::findUnit(const std::string& name)
{
    auto iter = std::lower_bound(m_units.begin(),
                                 m_units.end(),
                                 name,
                                 OrderUniqUnitByName_string);

    if (iter != m_units.end()
        && (*iter)->name() == name)
    {
        return (*iter).get();
    }
    return nullptr;
}

unique_group_t Group::newRoot(const std::string& name)
{
    return unique_group_t(new Group(name));
}

Group::Group(const std::string& name)
    : Unit(name)
{
}

// ------------------------------------------------------------

bool Group::OrderUniqUnitByName::operator() (const unique_unit_t& left,
                                             const unique_unit_t& right) const
{
    return left->name() < right->name();
}

bool Group::OrderUniqUnitByName_string(const unique_unit_t& left,
                                       const std::string& right)
{
    return left->name() < right;
}

// ------------------------------------------------------------

const std::string Script::FileExt = ".swd";

//

Script::Script(const std::string& name,
               Group& parent)
    : Unit(name),
      m_parent(&parent)
{
}

Group* Script::parent()
{
    return m_parent;
}

bool Script::isCompleted(const std::string& stepName) const
{
    for (auto& step : m_steps)
    {
        if (!step->m_completed) {
            return false;
        }

        if (step->name() == stepName) {
            return true;
        }
    }

    return false;
}

void Script::completeStep(const std::string& stepName)
{
    bool matchFound = false;

    for (auto& step : m_steps)
    {
        if (step->name() == stepName)
        {
            step->m_completed = true;
            matchFound = true;
            continue;
        }

        if (!matchFound) {
            if (!step->m_completed) {
                throw std::runtime_error("completing a step out-of-order");
            }
        }
        else {
            step->m_completed = false;
        }
    }
}

void Script::undoStep(const std::string& stepName)
{
    bool matchFound = false;

    for (auto& step : m_steps)
    {
        if (step->name() == stepName) {
            matchFound = true;
        }

        if (matchFound) {
            step->m_completed = false;
        }
    }
}

void Script::undoAllSteps()
{
    for (auto& step : m_steps)
    {
        step->m_completed = false;
    }
}

void Script::add(unique_step_t&& step)
{
    m_steps.emplace_back(std::move(step));
}

void Script::visit(const UnitVisitor& visitor)
{
    visitor(*this);
}

void Script::visitChildren(const UnitVisitor& visitor)
{
    for (auto& step : m_steps)
    {
        step->visit(visitor);
    }
}

void Script::forEach(const UnitVisitor& visitor)
{
    visit(visitor);

    for (auto& step : m_steps)
    {
        step->forEach(visitor);
    }
}

Step* Script::findStep(const std::string& name)
{
    for (auto& step : m_steps)
    {
        if (step->name() == name)
            return step.get();
    }

    return nullptr;
}

// ------------------------------------------------------------

Step::Step(const std::string& name,
           Script& parent,
           Flags flags)
    : Unit(name),
      m_parent(&parent),
      m_flags(flags)
{
}

Script* Step::parent()
{
    return m_parent;
}

bool Step::flag(Flag f) const
{
    return m_flags.flag(f);
}

bool Step::isCompleted() const
{
    return m_parent->isCompleted(name());
}

void Step::complete()
{
    m_parent->completeStep(name());
}

void Step::undo()
{
    m_parent->undoStep(name());
}

bool Step::hasArtifact(const std::string& artifact)
{
    return std::find(m_artifacts.begin(),
                     m_artifacts.end(),
                     artifact) != m_artifacts.end();
}

void Step::addArtifact(const std::string& artifact)
{
    m_artifacts.push_back(artifact);
}

void Step::addDependency(unique_dependency_t&& dependency)
{
    m_dependencies.push_back(std::move(dependency));
}

void Step::visit(const UnitVisitor& visitor)
{
    visitor(*this);
}

void Step::forEach(const UnitVisitor& visitor)
{
    visit(visitor);
}

void Step::forEachDependency(const std::function<void(Dependency&)>& callback)
{
    for (auto& d : m_dependencies) {
        callback(*d);
    }
}

bool Step::everythingUpToDate(Master& master)
{
    bool upToDateSoFar = isCompleted();

    if (upToDateSoFar) {
        for (auto& d : m_dependencies) {
            if (!d->isUpToDate()) {
                upToDateSoFar = false;
                break;
            }
        }
    }

    const std::string stepName = tools::conjurePath(*this);

    for (const std::string& artName : m_artifacts)
    {
        auto& artifact = master.artifact(artName);

        if (!artifact.checkInvalidation(master,
                                        stepName,
                                        !upToDateSoFar))        // !upToDateSoFar == rebuildNeeded
        {
            upToDateSoFar = false;
        }
    }

    return upToDateSoFar;
}

void Step::recalculateHashes(Master& master)
{
    const std::string stepName = tools::conjurePath(*this);

    for (const std::string& artName : m_artifacts)
    {
        auto& artifact = master.artifact(artName);

        artifact.recalculate(stepName);
    }

    for (auto& d : m_dependencies)
    {
        d->storeHash( d->calculateHash() );
    }
}
