/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "script-tools.hh"

#include "config.hh"
#include "hash-cache_impl.hh"
#include "master.hh"
#include "script.hh"
#include "utils/exec.hh"
#include "utils/string.hh"

#include "json/single_include/nlohmann/json.hpp"

#include <iostream>
#include <sstream>

#include <unistd.h>

using json = nlohmann::json;

//

class conjure_path : public UnitVisitor {
public:
    conjure_path(bool porcelain)
        : m_porcelain(porcelain) {}

    void operator() (Group& group) const override
    {
        if (!m_porcelain
            || group.parent() != nullptr)
        {
            printDelim('/');
            m_oss << group.name();
        }
    }

    void operator() (Script& script) const override
    {
        printDelim('/');

        m_oss << script.name();

        if (!m_porcelain) {
            m_oss << Script::FileExt;
        }
    }

    void operator() (Step& step) const override
    {
        printDelim(' ');
        m_oss << step.name();
    }

    std::string path() const
    {
        return m_oss.str();
    }

private:
    bool m_porcelain;

    mutable bool m_needDelim = false;
    mutable std::ostringstream m_oss;

    //

    void printDelim(char delim) const
    {
        if (m_needDelim) {
            m_oss << delim;
        }
        else {
            m_needDelim = true;
        }
    }
};

//

std::string tools::conjurePath(Unit& unit)
{
    conjure_path cp(true);
    unit.travelPath(cp);
    return cp.path();
}

std::string tools::conjureExec(Unit& unit)
{
    conjure_path cp(false);
    unit.travelPath(cp);
    return cp.path();
}

// ------------------------------------------------------------

namespace
{
    std::string stepCacheFileName()
    {
        return Config::instance().cache_dir + "/steps.json";
    }

    //

    class load_basics : public UnitVisitor {
    public:
        load_basics(Master& master)
            : m_master(master) {}

        void operator() (Group& group) const override
        {
            const std::string groupName = tools::conjurePath(group);
            const bool rootGroup = groupName.empty();
            const std::string groupInfoFile = (rootGroup
                                               ? Config::instance().root + "/group.swd"
                                               : Config::instance().root + '/' + groupName + "/group.swd");

            if (access(groupInfoFile.c_str(), F_OK) != 0) {
                return;
            }

            if (access(groupInfoFile.c_str(), X_OK) != 0) {
                throw std::runtime_error(groupInfoFile + " found but it is not executable");
            }

            json j = execSwdInfo(groupInfoFile);

            // artifacts

            parseArtifacts(j,
                           groupName);
        }

        void operator() (Script& script) const override
        {
            const std::string scriptName = tools::conjurePath(script);
            const std::string scriptExec = Config::instance().root + '/' + scriptName + Script::FileExt;

            json j = execSwdInfo(scriptExec);

            // artifacts

            parseArtifacts(j,
                           scriptName);

            // steps

            if (j.count("steps"))
            {
                if (j["steps"].is_array() == false) {
                    throw std::runtime_error("steps should be defined inside an array");
                }

                for (auto& j_step : j["steps"])
                {
                    if (j_step.count("name") == 0) {
                        throw std::runtime_error("step with no name");
                    }

                    Step& newStep = *new Step(j_step["name"],
                                              script);

                    script.add(unique_step_t(&newStep));

                    //

                    // artifact links

                    checkArtifactLinkSanity(j_step);

                    if (j_step.count("artifacts"))
                    {
                        for (auto& j_artifact : j_step["artifacts"])
                        {
                            std::string artifactName = j_artifact.get<std::string>();

                            if (artifactName.empty()) {
                                throw std::runtime_error("artifact id must not be empty");
                            }

                            if (artifactName[0] == '/') {
                                artifactName.erase(0, 1);
                            }
                            else {
                                artifactName = scriptName + '/' + artifactName;
                            }

                            newStep.addArtifact(artifactName);
                        }
                    }

                    // dependencies

                    checkDependencySanity(j_step);

                    if (j_step.count("dependencies") > 0)
                    {
                        for (auto& j_dep : j_step["dependencies"])
                        {
                            const std::string type = j_dep["type"].get<std::string>();

                            if (type == "artifact")
                            {
                                std::string artifactName = j_dep["id"].get<std::string>();

                                if (artifactName.empty()) {
                                    throw std::runtime_error("artifact id must not be empty");
                                }

                                if (artifactName[0] == '/') {
                                    artifactName.erase(0, 1);
                                }
                                else {
                                    artifactName = scriptName + '/' + artifactName;
                                }

                                newStep.addDependency(std::make_unique<DependencyArtifact>(m_master,
                                                                                           artifactName));
                            }
                            else if (type == "data")
                            {
                                newStep.addDependency(std::make_unique<DependencyData>(j_dep["id"].get<std::string>(),
                                                                                       j_dep["data"].get<std::string>()));
                            }
                            else if (type == "file")
                            {
                                newStep.addDependency(std::make_unique<DependencyFile>(j_dep["id"].get<std::string>(),
                                                                                       j_dep["path"].get<std::string>()));
                            }
                        }
                    }
                }
            }
        }

    private:
        Master& m_master;

        //

        void parseArtifacts(json& j,
                            const std::string& unitName) const
        {
            if (j.count("artifacts") > 0)
            {
                if (j["artifacts"].is_object() == false) {
                    throw std::runtime_error("artifacts should be defined inside an object");
                }

                for (auto& j_art : j["artifacts"].items())
                {
                    const std::string& key = j_art.key();
                    const json& value = j_art.value();

                    //

                    const std::string artifactName = ( !unitName.empty()
                                                       ? unitName + '/' + key
                                                       : key );

                    checkArtifactSanity(artifactName,
                                        key,
                                        value);

                    //

                    const std::string type = value["type"].get<std::string>();
                    const std::string path = value["path"].get<std::string>();

                    Artifact* artifact = nullptr;

                    if (type == "file")
                    {
                        artifact = new ArtifactFile(artifactName,
                                                    path);
                    }
                    else if (type == "directory")
                    {
                        std::vector<std::string> excludeDirs;

                        if (value.count("exclude") > 0)
                        {
                            for (const auto& ex : value["exclude"]) {
                                excludeDirs.push_back(ex.get<std::string>());
                            }
                        }

                        artifact = new ArtifactDir(artifactName,
                                                   path,
                                                   std::move( excludeDirs ));
                    }

                    m_master.artifacts.emplace(artifactName,
                                               unique_artifact_t(artifact));

                    //

                    if (value.count("managed") > 0
                        && value["managed"].get<bool>() == true)
                    {
                        artifact->setManaged(unitName);
                    }
                }
            }
        }

        void checkArtifactSanity(const std::string& name,
                                 const std::string& key,
                                 const json& value) const
        {
            if (key.empty()) {
                throw std::runtime_error("artifact name is empty");
            }

            if (m_master.artifacts.count(name)) {
                throw std::runtime_error("artifact " + name + " defined more than once");
            }

            // string "type"

            if (value.count("type") == 0) {
                throw std::runtime_error("artifact with no type");
            }

            if (value["type"].is_string() == false) {
                throw std::runtime_error("artifact with non-string type");
            }

            const std::string type = value["type"].get<std::string>();

            if (type != "file"
                && type != "directory")
            {
                throw std::runtime_error("artifact '" + name + "' with unknown type '" + type + "'");
            }

            // string "path"

            if (value.count("path") == 0) {
                throw std::runtime_error(type + "-artifact with no path");
            }

            if (value["path"].is_string() == false) {
                throw std::runtime_error(type + "-artifact with non-string path");
            }

            // array "exclude" (optional)

            if (type == "directory")
            {
                if (value.count("exclude") > 0) {
                    if (value["exclude"].is_array() == false) {
                        throw std::runtime_error(type + "-artifact with non-array exclude-element");
                    }

                    for (const auto& ex : value["exclude"]) {
                        if (ex.is_string() == false)
                        {
                            throw std::runtime_error(type + "-artifact with non-string value inside exclude");
                        }
                    }
                }
            }

            // boolean "managed" (optional)

            if (value.count("managed") > 0) {
                if (value["managed"].is_boolean() == false) {
                    throw std::runtime_error(type + "-artifact with non-boolean managed-element");
                }
            }
        }

        static void checkArtifactLinkSanity(const json& j)
        {
            if (j.count("artifacts") == 0) {
                return;
            }

            if (j["artifacts"].is_array() == false) {
                throw std::runtime_error("artifact-links should be defined inside an array");
            }

            for (auto& j_artifact : j["artifacts"])
            {
                if (j_artifact.is_string() == false) {
                    throw std::runtime_error("artifact-link should be a string");
                }
            }
        }

        static void checkDependencySanity(const json& j)
        {
            if (j.count("dependencies") == 0) {
                return;
            }

            if (j["dependencies"].is_array() == false) {
                throw std::runtime_error("dependencies should be defined inside an array");
            }

            for (auto& j_dep : j["dependencies"])
            {
                if (j_dep.is_object() == false) {
                    throw std::runtime_error("dependency-element should be an object");
                }

                // string "type"

                if (j_dep.count("type") == 0) {
                    throw std::runtime_error("dependency should have a type-element");
                }

                if (j_dep["type"].is_string() == false) {
                    throw std::runtime_error("dependency with non-string type-element");
                }

                const std::string type = j_dep["type"].get<std::string>();

                if (type == "artifact")
                {
                    if (j_dep.count("id") == 0) {
                        throw std::runtime_error("artifact-dependency should have an id-element");
                    }

                    if (j_dep["id"].is_string() == false) {
                        throw std::runtime_error("artifact-dependency with non-string id-element");
                    }
                }
                else if (type == "data")
                {
                    // string "id"

                    if (j_dep.count("id") == 0) {
                        throw std::runtime_error("data-dependency with no id-element");
                    }

                    if (j_dep["id"].is_string() == false) {
                        throw std::runtime_error("data-dependency with non-string id-element");
                    }

                    // string "data"

                    if (j_dep.count("data") == 0) {
                        throw std::runtime_error("data-dependency with no data-element");
                    }

                    if (j_dep["data"].is_string() == false) {
                        throw std::runtime_error("data-dependency with non-string data-element");
                    }
                }
                else if (type == "file")
                {
                    // string "id"

                    if (j_dep.count("id") == 0) {
                        throw std::runtime_error("file-dependency with no id-element");
                    }

                    if (j_dep["id"].is_string() == false) {
                        throw std::runtime_error("file-dependency with non-string id-element");
                    }

                    // string "path"

                    if (j_dep.count("path") == 0) {
                        throw std::runtime_error("file-dependency with no path-element");
                    }

                    if (j_dep["path"].is_string() == false) {
                        throw std::runtime_error("file-dependency with non-string path-element");
                    }
                }
                else {
                    throw std::runtime_error("dependency with unknown type '" + type + "'");
                }
            }
        }

        static json execSwdInfo(const std::string& execFile)
        {
            const std::string execCommand = execFile + " swd_info";
            utils::Exec execSwdInfo(execCommand);

            std::stringstream ssSwdInfo;
            ssSwdInfo << execSwdInfo.read().rdbuf();

            if (!execSwdInfo.wait()) {
                throw std::runtime_error("exec failed: " + execCommand);
            }

            json j;
            ssSwdInfo >> j;
            return j;
        }
    };

    // -----

    class load_step_cache : public UnitVisitor {
    public:
        load_step_cache(const json& j)
            : m_json(j) {}

        void operator() (Step& step) const override
        {
            const std::string stepName = tools::conjurePath(step);

            const json& j_step = ({
                    auto iter = m_json.find(stepName);
                    if (iter == m_json.end()) {
                        return;
                    }
                    *iter;
                });

            // completed

            if (j_step.count("completed") > 0
                && j_step["completed"].is_boolean()
                && j_step["completed"])
            {
                step.complete();
            }

            // dependencies

            if (j_step.count("dependencies") > 0
                && j_step["dependencies"].is_array())
            {
                const json& j_deps = j_step["dependencies"];

                step.forEachDependency([&j_deps] (Dependency& dep)
                                       {
                                           for (auto j_iter = j_deps.begin();
                                                j_iter != j_deps.end();
                                                ++j_iter)
                                           {
                                               if (!j_iter->is_object()) {
                                                   continue;
                                               }

                                               if (dep.id() == (*j_iter)["id"]
                                                   && dep.type() == (*j_iter)["type"]
                                                   && j_iter->count("hash"))
                                               {
                                                   dep.storeHash((*j_iter)["hash"]);
                                               }
                                           }
                                       });
            }
        }

    private:
        const json& m_json;
    };
}

//

void tools::loadScriptConfig(Master& master, Unit& unit)
{
    unit.forEach(load_basics(master));

    //

    std::ifstream ifs(stepCacheFileName());

    if (!ifs) {
        std::cout << "Step cache file does not exist." << std::endl;
        return;
    }

    json j;

    if (!(ifs >> j)) {
        throw std::runtime_error("failed to parse steps cache JSON");
    }

    //

    unit.forEach(load_step_cache(j));
}

// ------------------------------------------------------------

void to_json(json& j, const Dependency& dep)
{
    j = json{
        { "hash", dep.getHashSum() },
        { "id",   dep.id()         },
        { "type", dep.type()       }
    };
}

// -----

namespace
{
    class conjure_step_cache_json : public UnitVisitor {
    public:
        void operator() (Step& step) const override
        {
            const std::string stepName = tools::conjurePath(step);

            // completed

            if (step.isCompleted()) {
                m_json[stepName]["completed"] = true;
            }

            // dependencies

            json j_deps = json::array();
            bool didSomething = false;

            step.forEachDependency([&j_deps, &didSomething] (Dependency& dep)
                                   {
                                       j_deps.push_back(dep);
                                       didSomething = true;
                                   });

            if (didSomething) {
                m_json[stepName]["dependencies"] = j_deps;
            }
        }

        const json& getJson() const
        {
            return m_json;
        }

    private:
        mutable json m_json;
    };
}

//

void tools::saveScriptCache(Unit& unit)
{
    conjure_step_cache_json conjurer;
    unit.forEach(conjurer);

    //

    const std::string fileName = stepCacheFileName();
    const std::string tmpName = fileName + ".tmp";

    std::ofstream ofs(tmpName);

    if (!ofs) {
        throw std::runtime_error("failed to open step cache file: " + tmpName);
    }

    if (!(ofs << conjurer.getJson().dump(1, '\t') << std::endl)) {
        throw std::runtime_error("failed to save step JSON data");
    }

    if (rename(tmpName.c_str(), fileName.c_str()) != 0) {
        throw std::runtime_error("failed to rename '" + tmpName + "' over '" + fileName + "'");
    }
}

// ------------------------------------------------------------

namespace
{
    class scoped_execute : public UnitVisitor {
    public:

        // TODO(pjsaksa): separate operation modes ... derived classes maybe?

        scoped_execute(Master& master,
                       int iterationLimit,
                       bool showNext,
                       bool interactive)
            : m_master(master),
              m_iterationLimit(iterationLimit),
              m_showNext(showNext),
              m_interactive(interactive) {}

        void operator() (Group& group) const override
        {
            if (m_iterationLimit == 0) {
                return;
            }

        restart_scope:
            try {
                group.forEachChild( *this );
            }
            catch (const invalidate_scope& scopeEx)
            {
                const std::string groupName = tools::conjurePath(group);

                if (scopeEx.scope() == groupName) {
                    goto restart_scope;
                }
                else {
                    throw;
                }
            }
        }

        void operator() (Step& step) const override
        {
            if (m_iterationLimit == 0) {
                return;
            }

            const bool rebuild = !step.everythingUpToDate(m_master);

            if (rebuild)
            {
                if (m_showNext) {
                    m_iterationLimit = 0;
                    std::cout << "next '" << tools::conjurePath(step) << "'" << std::endl;
                    return;
                }

                if (m_interactive) {
                    std::cout << "exec '" << tools::conjurePath(step) << "' ? [Y]: " << std::flush;

                    std::string token;

                    {
                        std::string line;

                        if (!getline(std::cin, line)) {
                            throw std::runtime_error("input reading failed");
                        }

                        std::istringstream issLine(line);

                        issLine >> token;   // ignore errors
                    }

                    utils::tolower(token);

                    if (!(token.empty()
                          || token == "y"
                          || token == "yes"))
                    {
                        m_iterationLimit = 0;
                        return;
                    }
                }

                doExecute(m_master,
                          step);

                //

                if (m_iterationLimit > 0) {
                    --m_iterationLimit;
                }
            }
        }

        //

        static void doExecute(Master& master,
                              Step& step)
        {
            const auto& conf = Config::instance();

            if (conf.interrupted) {
                throw std::runtime_error("INTERRUPTED");
            }

            utils::Exec process(tools::conjureExec(step));

            std::cout << process.read().rdbuf();

            if (process.wait()
                && !conf.interrupted)
            {
                step.complete();
                step.recalculateHashes(master);
            }
            else {
                throw std::runtime_error("step '" + tools::conjureExec(step) + "' failed");
            }
        }

    private:
        Master& m_master;
        mutable int m_iterationLimit;
        bool m_showNext;
        bool m_interactive;
    };
}

//

void tools::execute(Master& master,
                    int stepCount,
                    bool showNext,
                    bool interactive)
{
    const int modeCount = ((showNext         ? 1 : 0)
                           + (stepCount > -1 ? 1 : 0)
                           + (interactive    ? 1 : 0));

    if (modeCount >= 2) {
        throw std::runtime_error("tools::execute(): too many main functions");
    }

    master.root->visit( scoped_execute(master, stepCount, showNext, interactive) );
}

// ------------------------------------------------------------

namespace
{
    class list_steps : public UnitVisitor {
    public:
        list_steps(std::ostream& out)
            : m_out(out) {}

        void operator() (Step& step) const override
        {
            m_out << tools::conjurePath(step) << '\n';
        }

    private:
        std::ostream& m_out;
    };
}

//

void tools::listSteps(Unit& unit, std::ostream& out)
{
    unit.forEach(list_steps(out));
}

// ------------------------------------------------------------

namespace
{
    class find_unit : public UnitVisitor {
    public:
        find_unit(const std::string& path,
                  const UnitVisitor& operation)
            : m_path(path),
              m_pathLeft(path),
              m_operation(operation) {}

        void operator() (Group& group) const override
        {
            if (m_pathLeft.empty())
            {
                callOperation(group);
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

                unit->visit(*this);
            }
        }

        void operator() (Script& script) const override
        {
            if (m_pathLeft.empty())
            {
                callOperation(script);
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

                step->visit(*this);
            }
        }

        void operator() (Step& step) const override
        {
            if (m_pathLeft.empty())
            {
                callOperation(step);
            }
            else {
                throw std::runtime_error("unknown unit: " + m_path);
            }
        }

    private:
        const std::string& m_path;
        mutable std::string m_pathLeft;
        const UnitVisitor& m_operation;

        //

        void callOperation(Unit& unit) const
        {
            if (tools::conjurePath(unit) == m_path) {
                unit.visit(m_operation);
            }
            else {
                throw std::runtime_error("unknown unit: " + m_path);
            }
        }
    };

    // -----

    class undo_step : public UnitVisitor {
    public:
        void operator() (Group& group) const override
        {
            std::cout << "Undoing group " << tools::conjurePath(group) << std::endl;
            group.forEach(*this);
        }

        void operator() (Script& script) const override
        {
            std::cout << "Undoing script " << tools::conjurePath(script) << std::endl;
            script.undoAllSteps();
        }

        void operator() (Step& step) const override
        {
            std::cout << "Undoing step " << tools::conjurePath(step) << std::endl;
            step.undo();
        }
    };

    // -----

    class force_step : public UnitVisitor {
    public:
        force_step(Master& master)
            : m_master(master) {}

        void operator() (Group&) const override
        {
            std::cout << "Only single steps can be forced." << std::endl;
        }

        void operator() (Script&) const override
        {
            std::cout << "Only single steps can be forced." << std::endl;
        }

        void operator() (Step& step) const override
        {
            std::cout << "Forcing step " << tools::conjurePath(step) << std::endl;

            scoped_execute::doExecute(m_master,
                                      step);
        }

    private:
        Master& m_master;
    };
}

void tools::undo(Master& master, const std::string& stepName)
{
    master.root->visit(find_unit(stepName,
                                 undo_step()));
}

void tools::force(Master& master, const std::string& stepName)
{
    master.root->visit(find_unit(stepName,
                                 force_step(master)));
}
