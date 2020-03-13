/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "config.hh"
#include "hash-tools.hh"
#include "master.hh"
#include "script-tools.hh"

#include <cctype>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{
    namespace Args
    {
        const std::string interactive_L = "--interactive";
        const std::string interactive_S = "-i";
        const std::string next_L        = "--next";
        const std::string next_S        = "-n";
        const std::string rehash_L      = "--rehash";
        const std::string rehash_S      = "-r";
        const std::string step_L        = "--step";
        const std::string step_S        = "-s";
        const std::string undo_L        = "--undo";
        const std::string undo_S        = "-u";
    }

    //

    void printUsage()
    {
        std::cout <<
            "NAME\n"
            "        swd - scripts with dependencies\n"
            "\n"
            "SYNOPSIS\n"
            "        swd [ options... ] [ function ]\n"
            "\n"
            "FUNCTIONS\n"
            "        (default)\n"
            "            Execute all incomplete steps.\n"
            "\n"
            "        --help | -?\n"
            "            Print this help text.\n"
            "\n"
            "        --list-steps\n"
            "            List all known steps.\n"
            "\n"
            "        --list-artifacts\n"
            "            List all known artifacts.\n"
            "\n"
            "        " << Args::next_L << " | " << Args::next_S << "\n"
            "            Print the name of the next step, but do not execute it.\n"
            "\n"
            "        " << Args::undo_L << "=<step> | " << Args::undo_S << " <step>\n"
            "            Mark <step> as incomplete.\n"
            "\n"
            "        " << Args::step_L << "=<n> | " << Args::step_S << " <n>\n"
            "            Stop after executing <n> steps.\n"
            "\n"
            "        " << Args::interactive_L << " | " << Args::interactive_S << "\n"
            "            Execute steps interactively.\n"
            "\n"
            "        " << Args::rehash_L << "=<artifact> | " << Args::rehash_S << " <artifact>\n"
            "            Rehash (validate) artifact.\n"
            "\n"
            "OPTIONS\n"
            "        -C <path>\n"
            "            Run swd as if it was started in <path>.\n";
    }

    // -----

    class MainFunction {
    public:
        virtual ~MainFunction() = default;
        virtual void execute(Master& master) = 0;
    };

    //

    namespace Oper
    {
        class ListArtifacts : public MainFunction {
        public:
            void execute(Master& master) override
            {
                tools::listArtifacts(master, std::cout);
            }
        };

        //

        class ListSteps : public MainFunction {
        public:
            void execute(Master& master) override
            {
                tools::listSteps(*master.root,
                                 std::cout);
            }
        };

        //

        class ExecuteSteps : public MainFunction {
        public:
            ExecuteSteps() = default;
            ExecuteSteps(int steps)
                : m_steps(steps) {}

            void execute(Master& master) override
            {
                tools::execute(master,
                               m_steps,
                               false,
                               false);
            }

        private:
            int m_steps = -1;
        };

        //

        class ShowNext : public MainFunction {
        public:
            void execute(Master& master) override
            {
                tools::execute(master,
                               -1,
                               true,
                               false);
            }
        };

        //

        class Undo : public MainFunction {
        public:
            Undo(const std::string& stepName)
                : m_stepName(stepName) {}

            void execute(Master& master) override
            {
                tools::undo(master,
                            m_stepName);
            }

        private:
            std::string m_stepName;
        };

        //

        class Interactive : public MainFunction {
        public:
            void execute(Master& master) override
            {
                Config::setenv("SWD_INTERACTIVE", "yes");

                tools::execute(master,
                               -1,
                               false,
                               true);
            }
        };

        //

        class RehashArtifact : public MainFunction {
        public:
            RehashArtifact(const std::string& name)
                : m_artifactName(name) {}

            void execute(Master& master) override
            {
                auto& artifact = master.artifact(m_artifactName);

                artifact.recalculate();
            }

        private:
            std::string m_artifactName;
        };
    }

    //

    bool longArgMatches(const std::string& arg,
                        const std::string& spec,
                        const bool needsValue)
    {
        // basic match

        if (arg.empty()
            || spec.empty()
            || arg.compare(0, spec.size(), spec) != 0)
        {
            return false;
        }

        // match is exact; argument has no value

        if (arg.size() == spec.size()) {
            if (needsValue) {
                throw std::runtime_error("missing value for argument '" + spec + "'");
            }
            else {
                return true;        // <-- only possible success if (!needsValue)
            }
        }

        // inspect separating character (normally '=')

        const char sep = arg[spec.size()];

        if (isalnum(sep)
            || sep == '_'
            || sep == '-')
        {
            return false;       // this is really some other argument
        }

        if (sep != '=') {
            throw std::runtime_error("invalid use of argument '" + spec + "'");
        }

        // match ok, argument has value

        if (needsValue) {
            return true;        // <-- only possible success if (needsValue)
        }
        else {
            throw std::runtime_error("excess value for argument '" + spec + "'");
        }
    }

    //

    std::string extractArgumentValue(const std::vector<std::string>& args,
                                     std::vector<std::string>::const_iterator& iter,
                                     const std::string& arg_L,
                                     const std::string& arg_S,
                                     const std::string& argumentInfo)
    {
        std::string value;

        if (*iter == arg_S) {
            if (++iter == args.end()) {
                throw std::runtime_error("missing value for argument " + argumentInfo);
            }

            value = *iter;
        }
        else {
            value = iter->substr(arg_L.size() + 1);
        }

        if (value.empty()) {
            throw std::runtime_error("missing value for argument " + argumentInfo);
        }

        return value;
    }

    //

    std::unique_ptr<MainFunction> parseArguments(const std::vector<std::string>& args)
    {
        for (const auto& arg : args)
        {
            if (arg == "-?"
                || arg == "--help")
            {
                printUsage();
                exit(0);
            }
        }

        //

        std::unique_ptr<MainFunction> mainFunction;

        for (auto iter = args.begin();
             iter != args.end();
             ++iter)
        {
            if (*iter == "-C")
            {
                if (++iter == args.end()) {
                    throw std::runtime_error("missing argument after -C");
                }

                if (chdir(iter->c_str()) != 0) {
                    throw std::runtime_error("failed to chdir() to " + *iter);
                }
            }
            else if (longArgMatches(*iter, "--list-steps", false))
            {
                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                mainFunction = std::make_unique<Oper::ListSteps>();
            }
            else if (longArgMatches(*iter, "--list-artifacts", false))
            {
                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                mainFunction = std::make_unique<Oper::ListArtifacts>();
            }
            else if (*iter == Args::next_S
                     || longArgMatches(*iter, Args::next_L, false))
            {
                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                mainFunction = std::make_unique<Oper::ShowNext>();
            }
            else if (*iter == Args::interactive_S
                     || longArgMatches(*iter, Args::interactive_L, false))
            {
                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                mainFunction = std::make_unique<Oper::Interactive>();
            }
            else if (*iter == Args::step_S
                     || longArgMatches(*iter, Args::step_L, true))
            {
                const std::string argumentInfo = Args::step_L + "/" + Args::step_S;

                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + argumentInfo);
                }

                //

                std::istringstream iss( extractArgumentValue(args, iter, Args::step_L, Args::step_S, argumentInfo) );
                int n;

                if (iss >> n
                    && n > 0)
                {
                    mainFunction = std::make_unique<Oper::ExecuteSteps>( n );
                }
                else {
                    throw std::runtime_error("value for " + argumentInfo + " must be a positive number");
                }
            }
            else if (*iter == Args::undo_S
                     || longArgMatches(*iter, Args::undo_L, true))
            {
                const std::string argumentInfo = Args::undo_L + "/" + Args::undo_S;

                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + argumentInfo);
                }

                //

                const std::string unitName = extractArgumentValue(args, iter, Args::undo_L, Args::undo_S, argumentInfo);

                mainFunction = std::make_unique<Oper::Undo>( unitName );
            }
            else if (*iter == Args::rehash_S
                     || longArgMatches(*iter, Args::rehash_L, true))
            {
                const std::string argumentInfo = Args::rehash_L + "/" + Args::rehash_S;

                if (mainFunction) {
                    throw std::runtime_error("second argument declaring main function: " + argumentInfo);
                }

                //

                const std::string artifactName = extractArgumentValue(args, iter, Args::rehash_L, Args::rehash_S, argumentInfo);

                mainFunction = std::make_unique<Oper::RehashArtifact>( artifactName );
            }
            else {
                throw std::runtime_error("invalid argument '" + *iter + "'");
            }
        }

        return (mainFunction
                ? std::move(mainFunction)
                : std::make_unique<Oper::ExecuteSteps>());
    }
}

// ------------------------------------------------------------

int main(const int argc,
         const char* argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);

    try {
        const auto mainFunction = parseArguments(args);

        //

        Master& master = Master::instance();

        mainFunction->execute(master);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
