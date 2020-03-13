/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "script-syntax.hh"

#include "script.hh"
#include "utils/string.hh"

using namespace std;

//

#define ASSERT(value, message)                                          \
    do {                                                                \
        if (!(value)) {                                                 \
            throw syntax::syntax_error(message);                        \
        }                                                               \
    } while(0)

// ------------------------------------------------------------

void syntax::checkArtifact(const string& name,
                           const json& j)
{
    ASSERT(name.empty() == false, "artifact key must not be empty");
    ASSERT(j.is_object(),         "artifact value must be an object");

    // member existence

    ASSERT(j.count("type") > 0,   "artifact/type must exist");
    ASSERT(j.count("path") > 0,   "artifact/path must exist");

    // member type

    ASSERT(j["type"].is_string(), "artifact/type must be a string");
    ASSERT(j["path"].is_string(), "artifact/path must be a string");

    // member value

    const string type = j["type"].get<string>();

    ASSERT(type == "directory"
           || type == "file", "artifact/type has an unknown value '" + type + "'");

    // [type=directory]

    if (type == "directory")
    {
        // [type=directory]/exclude

        if (j.count("exclude") > 0) {
            ASSERT(j["exclude"].is_array(), "artifact[type=directory]/exclude must be an array");

            for (const auto& ex : j["exclude"]) {
                ASSERT(ex.is_string(), "artifact[type=directory]/exclude/* must be strings");
            }
        }
    }
}

void syntax::checkDependencyArray(const json& j)
{
    ASSERT(j.is_array(), "dependencies must be an array");

    for (const auto& j_dep : j)
    {
        checkDependency(j_dep);
    }
}

void syntax::checkDependency(const json& j)
{
    ASSERT(j.is_object(), "dependency must be an object");

    // dependency/type

    ASSERT(j.count("type") > 0,   "dependency must have a type-element");
    ASSERT(j["type"].is_string(), "dependency with non-string type-element");

    const string type = j["type"].get<string>();

    ASSERT(type == "artifact"
           || type == "data"
           || type == "file", "dependency/type has an unknown value '" + type + "'");

    // dependency/id

    ASSERT(j.count("id") > 0,   "dependency/id must exist");
    ASSERT(j["id"].is_string(), "dependency/id must be a string");

    if (type == "data")
    {
        // dependency[type=data]/data

        ASSERT(j.count("data") > 0,   "dependency[type=data]/data must exist");
        ASSERT(j["data"].is_string(), "dependency[type=data]/data must be a string");
    }
    else if (type == "file")
    {
        // dependency[type=file]/path

        ASSERT(j.count("path") > 0,   "dependency[type=file]/path must exist");
        ASSERT(j["path"].is_string(), "dependency[type=file]/path must be a string");
    }
}

void syntax::checkRule(const string& name,
                       const json& j)
{
    ASSERT(name.empty() == false, "rule's step-indicator key must not be empty");
    ASSERT(j.is_object(),         "rule's step-indicator value must be an object");

    for (const auto& ruleIter : j.items())
    {
        const string& ruleType = ruleIter.key();
        const json& j_rule = ruleIter.value();

        ASSERT(ruleType.empty() == false, "rule key must not be empty");
        ASSERT(ruleType == "dependencies", "rule type is unknown");

        if (ruleType == "dependencies")
        {
            ASSERT(j_rule.is_array(), "rule value must be an object");
            checkDependencyArray(j_rule);
        }
    }
}

void syntax::checkStep(const json& j)
{
    // name

    ASSERT(j.count("name") > 0,   "step must have a name");
    ASSERT(j["name"].is_string(), "step name must be a string");

    // flags

    if (j.count("flags") > 0) {
        ASSERT(j["flags"].is_array(), "step's flags-element must be an array");

        for (const auto& j_flag : j["flags"]) {
            ASSERT(j_flag.is_string(), "step's flags must be strings");

            const string flagName = utils::tolower( j_flag.get<string>() );

            ASSERT(flagName == "always"
                   || flagName == "sudo", "step has an unknown flag '" + flagName + "'");

        }
    }

    // artifacts (links)

    if (j.count("artifacts") > 0) {
        ASSERT(j["artifacts"].is_object(), "step's artifact link container must be an object");

        for (const auto& artIter : j["artifacts"].items())
        {
            const std::string& linkId = artIter.key();
            const json& j_linkType = artIter.value();

            ASSERT(linkId.empty() == false, "step's artifact link id must not be empty");
            ASSERT(j_linkType.is_string(),  "step's artifact link type must be a string");

            const std::string linkType = j_linkType.get<string>();

            try {
                Step::ArtifactLink::parse(linkType);
            }
            catch (std::range_error&) {
                throw syntax::syntax_error("step's artifact link type is unknown: " + linkType);
            }
        }
    }

    // dependencies

    if (j.count("dependencies") > 0) {
        checkDependencyArray(j["dependencies"]);
    }
}

void syntax::checkGroupFile(const json& j)
{
    ASSERT(j.is_object(), "group-json must be an object");

    if (j.count("artifacts") > 0) {
        ASSERT(j["artifacts"].is_object(), "artifact container must be an object");

        for (const auto& artifactIter : j["artifacts"].items())
        {
            checkArtifact(artifactIter.key(),
                          artifactIter.value());
        }
    }

    if (j.count("rules") > 0) {
        ASSERT (j["rules"].is_object(), "rules container must be an object");

        for (const auto& ruleIter : j["rules"].items())
        {
            checkRule(ruleIter.key(),
                      ruleIter.value());
        }
    }
}

void syntax::checkScriptFile(const json& j)
{
    ASSERT(j.is_object(), "script-json must be an object");

    // artifacts

    if (j.count("artifacts") > 0) {
        ASSERT(j["artifacts"].is_object(), "artifact container must be an object");

        for (const auto& artifactIter : j["artifacts"].items())
        {
            checkArtifact(artifactIter.key(),
                          artifactIter.value());
        }
    }

    // steps

    if (j.count("steps")> 0) {
        ASSERT(j["steps"].is_array(), "step container must be an array");

        for (const auto& j_step : j["steps"])
        {
            checkStep(j_step);
        }
    }
}
