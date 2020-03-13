/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "script-tools.hh"

#include "config.hh"
#include "hash-cache_impl.hh"
#include "master.hh"
#include "script-syntax.hh"
#include "script-travelers.hh"
#include "script.hh"
#include "utils/ansi.hh"
#include "utils/exec.hh"
#include "utils/string.hh"

#include "json/single_include/nlohmann/json.hpp"

#include <iostream>
#include <sstream>

#include <unistd.h>

using json = nlohmann::json;

//

class conjure_path : public Unit::Visitor {
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
    unit.apply(travelers::Path(cp));
    return cp.path();
}

std::string tools::conjureExec(Unit& unit)
{
    conjure_path cp(false);
    unit.apply(travelers::Path(cp));
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

    class load_basics : public Unit::Visitor {
    public:
        load_basics(Master& master)
            : m_master(master) {}

        void operator() (Group& group) const override
        {
            const std::string groupName = tools::conjurePath(group);
            const bool isThisRootGroup = groupName.empty();
            const std::string groupFileName = (isThisRootGroup
                                               ? Config::instance().root + "/group.swd"
                                               : Config::instance().root + '/' + groupName + "/group.swd");

            if (access(groupFileName.c_str(), F_OK) == 0)
            {
                if (access(groupFileName.c_str(), X_OK) != 0) {
                    throw std::runtime_error(groupFileName + " found but it is not executable");
                }

                try {
                    const json j = execSwdInfo(groupFileName);

                    syntax::checkGroupFile(j);

                    // artifacts

                    parseArtifacts(j, groupName);

                    //

                    group.applyChildren(*this);

                    // rules

                    parseRules(group, groupName, j);
                }
                catch (std::exception& e) {
                    throw std::runtime_error(({
                                std::ostringstream oss;
                                oss << (isThisRootGroup ? "(root)" : groupName) << " : " << e.what();
                                oss.str();
                            }));
                }
            }
            else {
                group.applyChildren(*this);
            }
        }

        void operator() (Script& script) const override
        {
            const std::string scriptName = tools::conjurePath(script);
            const std::string scriptExec = Config::instance().root + '/' + scriptName + Script::FileExt;

            try {
                json j = execSwdInfo(scriptExec);

                syntax::checkScriptFile(j);

                // artifacts

                parseArtifacts(j, scriptName);

                // steps

                if (j.count("steps")) {
                    for (auto& j_step : j["steps"])
                    {
                        // flags

                        Step::Flags flags;

                        if (j_step.count("flags")) {
                            for (auto& j_flag : j_step["flags"])
                            {
                                const std::string flagName = utils::tolower( j_flag.get<std::string>() );

                                if (flagName == "always")    flags |= Step::Flag::Always;
                                else if (flagName == "sudo") flags |= Step::Flag::Sudo;
                            }
                        }

                        //

                        Step& newStep = *new Step(j_step["name"],
                                                  script,
                                                  flags);

                        script.add(unique_step_t(&newStep));

                        //

                        // artifact links

                        if (j_step.count("artifacts") > 0)
                        {
                            for (const auto& artIter : j_step["artifacts"].items())
                            {
                                std::string artifactName = artIter.key();
                                const json& j_artLinkType = artIter.value();

                                if (artifactName[0] == '/') {
                                    artifactName.erase(0, 1);
                                }
                                else {
                                    artifactName = scriptName + '/' + artifactName;
                                }

                                //

                                newStep.addArtifactLink(artifactName,
                                                        Step::ArtifactLink::parse(j_artLinkType.get<std::string>()));
                            }
                        }

                        // dependencies

                        loadDependencies(m_master, j_step, newStep, scriptName);
                    }
                }
            }
            catch (std::exception& e) {
                throw std::runtime_error(({
                            std::ostringstream oss;
                            oss << scriptName << " : " << e.what();
                            oss.str();
                        }));
            }
        }

        static void loadDependencies(Master& master, const json& j, Step& step, const std::string& baseName)
        {
            if (j.count("dependencies") <= 0) {
                return;
            }

            for (auto& j_dep : j["dependencies"])
            {
                const std::string type = j_dep["type"].get<std::string>();

                if (type == "artifact") {
                    std::string artifactName = j_dep["id"].get<std::string>();

                    if (artifactName[0] == '/') {
                        artifactName.erase(0, 1);
                    }
                    else {
                        artifactName = baseName + '/' + artifactName;
                    }

                    step.addDependency(std::make_unique<DependencyArtifact>(master,
                                                                            artifactName));
                }
                else if (type == "data") {
                    step.addDependency(std::make_unique<DependencyData>(j_dep["id"].get<std::string>(),
                                                                        j_dep["data"].get<std::string>()));
                }
                else if (type == "file") {
                    step.addDependency(std::make_unique<DependencyFile>(j_dep["id"].get<std::string>(),
                                                                        j_dep["path"].get<std::string>()));
                }
            }
        }

    private:
        Master& m_master;

        //

        void parseArtifacts(const json& j,
                            const std::string& unitName) const
        {
            if (j.count("artifacts") <= 0)
                return;

            for (auto& j_art : j["artifacts"].items())
            {
                const std::string& key = j_art.key();
                const json& value = j_art.value();

                //

                const std::string artifactName = ( !unitName.empty()
                                                   ? unitName + '/' + key
                                                   : key );

                const std::string type = value["type"].get<std::string>();
                const std::string path = value["path"].get<std::string>();

                Artifact* artifact = nullptr;

                if (type == "file")
                {
                    artifact = new ArtifactFile(artifactName,
                                                unitName,
                                                path);
                }
                else if (type == "directory")
                {
                    std::vector<std::string> excludeDirs;

                    if (value.count("exclude") > 0) {
                        for (const auto& ex : value["exclude"]) {
                            excludeDirs.push_back(ex.get<std::string>());
                        }
                    }

                    artifact = new ArtifactDir(artifactName,
                                               unitName,
                                               path,
                                               std::move( excludeDirs ));
                }

                m_master.artifacts.emplace(artifactName,
                                           unique_artifact_t(artifact));
            }
        }

        void parseRules(Group& group, const std::string& groupName, const json& j) const
        {
            if (j.count("rules") <= 0) {
                return;
            }

            for (auto& j_rule : j["rules"].items())
            {
                const std::string& stepName = j_rule.key();
                const json& j_stepRules = j_rule.value();

                // "dependencies"

                if (j_stepRules.count("dependencies"))
                {
                    group.apply(travelers::ForEach(lambdaVisitor([&master = m_master, &stepName, &groupName, &j_stepRules]
                                                                 (Step& step)
                                                                 {
                                                                     if (step.name() == stepName) {
                                                                         loadDependencies(master, j_stepRules, step, groupName);
                                                                     }
                                                                 })));
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

    class load_step_cache : public Unit::Visitor {
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
                try {
                    step.complete();
                } catch (std::runtime_error& ex) {
                    std::cout << "Failing to complete step '" << stepName << "' because: " << ex.what() << std::endl;
                }
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
    unit.apply(load_basics(master));

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

    unit.apply(travelers::ForEach(load_step_cache(j)));
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
    class conjure_step_cache_json : public Unit::Visitor {
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

    unit.apply(travelers::ForEach(conjurer));

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
    class scoped_execute : public Unit::Visitor {
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
                group.applyChildren( *this );
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

        void operator() (Script& script) const override
        {
            if (m_iterationLimit == 0) {
                return;
            }

        restart_scope:
            try {
                script.applyChildren( *this );
            }
            catch (const invalidate_scope& scopeEx)
            {
                const std::string scriptName = tools::conjurePath(script);

                if (scopeEx.scope() == scriptName) {
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
                    std::cout << utils::ansi::Bold
                              << utils::ansi::Green << "exec '"
                              << utils::ansi::White << tools::conjurePath(step)
                              << utils::ansi::Green << "' ? [Y]: "
                              << utils::ansi::Normal << std::flush;

                    std::string token;

                    {
                        std::string line;

                        if (!getline(std::cin, line)) {
                            throw std::runtime_error("input reading failed");
                        }

                        std::istringstream issLine(line);

                        issLine >> token;   // ignore errors
                        utils::tolower(token);
                    }

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

            const std::string command = ({
                    std::ostringstream oss;

                    if (step.flag(Step::Flag::Sudo)) {
                        oss << "sudo --non-interactive --preserve-env ";
                    }

                    oss << tools::conjureExec(step);

                    oss.str();
                });

            utils::Exec process(command);

            std::cout << process.read().rdbuf();

            const bool success = (process.wait()
                                  && !conf.interrupted);

            step.recalculateHashes(master);

            if (success) {
                step.complete();
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

    master.root->apply( scoped_execute(master, stepCount, showNext, interactive) );
}

// ------------------------------------------------------------

namespace
{
    class list_steps : public Unit::Visitor {
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
    unit.apply(travelers::ForEach(list_steps(out)));
}

// ------------------------------------------------------------

namespace
{
    class undo_step : public Unit::Visitor {
    public:
        void operator() (Group& group) const override
        {
            std::cout << "Undoing group " << tools::conjurePath(group) << std::endl;
            group.apply(travelers::ForEach(*this));
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
}

void tools::undo(Master& master, const std::string& stepName)
{
    master.root->apply(travelers::FindUnit(stepName,
                                           undo_step()));
}

// ------------------------------------------------------------

namespace
{
    class rebuild_artifact : public Unit::Visitor {
    public:
        rebuild_artifact(const std::string& artifact)
            : m_artifact(artifact) {}

        void operator() (Step& step) const override
        {
            if (step.isCompleted()
                && step.hasArtifactLink(m_artifact))
            {
                if (m_count == 0) {
                    std::cout << "Rebuilding artifact " << m_artifact << std::endl;
                }

                std::cout << "-> Undoing step " << tools::conjurePath(step) << std::endl;

                //

                step.undo();

                ++m_count;
            }
        }

        unsigned int count() const
        {
            return m_count;
        }

    private:
        const std::string& m_artifact;
        mutable unsigned int m_count = 0;
    };
}

unsigned int tools::rebuildArtifact(Master& master, const std::string& artifactName)
{
    rebuild_artifact rebuilder(artifactName);

    master.root->apply(travelers::ForEach(rebuilder));

    return rebuilder.count();
}
