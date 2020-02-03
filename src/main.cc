/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

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
        const std::string force_L       = "--force";
        const std::string force_S       = "-f";
        const std::string interactive_L = "--interactive";
        const std::string interactive_S = "-i";
        const std::string next_L        = "--next";
        const std::string next_S        = "-n";
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
            "        swd (--help | -?)\n"
            "        swd [-C <path>] (--list-steps | --list-artifacts | " << Args::next_L << " | " << Args::next_S << ")\n"
            "        swd [-C <path>] (" << Args::undo_L << "=<step> | " << Args::undo_S << " <step>)\n"
            "        swd [-C <path>] (" << Args::force_L << "=<step> | " << Args::force_S << " <step>)\n"
            "        swd [-C <path>] [" << Args::step_L << "=<n> | " << Args::step_S << " <n>]\n"
            "        swd [-C <path>] [" << Args::interactive_L << "=<n> | " << Args::interactive_S << " <n>]\n"
            "\n"
            "OPTIONS\n"
            "        --help | -?\n"
            "            Print this help text (and exit).\n"
            "\n"
            "        -C <path>\n"
            "            Run as if swd was started in <path> instead of the current working directory.\n"
            "\n"
            "        --list-steps\n"
            "            List all known steps (and exit).\n"
            "\n"
            "        --list-artifacts\n"
            "            List all known artifacts (and exit).\n"
            "\n"
            "        " << Args::next_L << " | " << Args::next_S << "\n"
            "            Print which step would be run next (and exit).\n"
            "\n"
            "        " << Args::undo_L << "=<step> | " << Args::undo_S << " <step>\n"
            "            Mark <step> as incomplete.\n"
            "\n"
            "        " << Args::force_L << "=<step> | " << Args::force_S << " <step>\n"
            "            Forcibly run <step> without any regards for dependencies. User Knows Best[TM].\n"
            "\n"
            "        " << Args::step_L << "=<n> | " << Args::step_S << " <n>\n"
            "            Stop after executing <n> steps.\n"
            "\n"
            "        " << Args::interactive_L << " | " << Args::interactive_S << "\n"
            "            Execute steps interactively.\n";
    }

    // -----

    class OperationMode {
    public:
        virtual ~OperationMode() = default;
        virtual void execute(Master& master) = 0;
    };

    //

    namespace Oper
    {
        class ListArtifacts : public OperationMode {
        public:
            void execute(Master& master) override
            {
                tools::listArtifacts(master, std::cout);
            }
        };

        //

        class ListSteps : public OperationMode {
        public:
            void execute(Master& master) override
            {
                tools::listSteps(*master.root,
                                 std::cout);
            }
        };

        //

        class ExecuteSteps : public OperationMode {
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

        class ShowNext : public OperationMode {
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

        class Undo : public OperationMode {
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

        class Force : public OperationMode {
        public:
            Force(const std::string& stepName)
                : m_stepName(stepName) {}

            void execute(Master& master) override
            {
                tools::force(master,
                             m_stepName);
            }

        private:
            std::string m_stepName;
        };

        //

        class Interactive : public OperationMode {
        public:
            void execute(Master& master) override
            {
                tools::execute(master,
                               -1,
                               false,
                               true);
            }
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

    std::unique_ptr<OperationMode> parseArguments(const std::vector<std::string>& args)
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

        std::unique_ptr<OperationMode> operationMode;

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
                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                operationMode = std::make_unique<Oper::ListSteps>();
            }
            else if (longArgMatches(*iter, "--list-artifacts", false))
            {
                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                operationMode = std::make_unique<Oper::ListArtifacts>();
            }
            else if (*iter == Args::next_S
                     || longArgMatches(*iter, Args::next_L, false))
            {
                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                operationMode = std::make_unique<Oper::ShowNext>();
            }
            else if (*iter == Args::interactive_S
                     || longArgMatches(*iter, Args::interactive_L, false))
            {
                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + *iter);
                }

                operationMode = std::make_unique<Oper::Interactive>();
            }
            else if (*iter == Args::step_S
                     || longArgMatches(*iter, Args::step_L, true))
            {
                const std::string argumentInfo = Args::step_L + "/" + Args::step_S;

                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + argumentInfo);
                }

                //

                std::istringstream iss( extractArgumentValue(args, iter, Args::step_L, Args::step_S, argumentInfo) );
                int n;

                if (iss >> n
                    && n > 0)
                {
                    operationMode = std::make_unique<Oper::ExecuteSteps>( n );
                }
                else {
                    throw std::runtime_error("value for " + argumentInfo + " must be a positive number");
                }
            }
            else if (*iter == Args::undo_S
                     || longArgMatches(*iter, Args::undo_L, true))
            {
                const std::string argumentInfo = Args::undo_L + "/" + Args::undo_S;

                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + argumentInfo);
                }

                //

                const std::string unitName = extractArgumentValue(args, iter, Args::undo_L, Args::undo_S, argumentInfo);

                operationMode = std::make_unique<Oper::Undo>( unitName );
            }
            else if (*iter == Args::force_S
                     || longArgMatches(*iter, Args::force_L, true))
            {
                const std::string argumentInfo = Args::force_L + "/" + Args::force_S;

                if (operationMode) {
                    throw std::runtime_error("second argument declaring main function: " + argumentInfo);
                }

                //

                const std::string unitName = extractArgumentValue(args, iter, Args::force_L, Args::force_S, argumentInfo);

                operationMode = std::make_unique<Oper::Force>( unitName );
            }
            else {
                throw std::runtime_error("invalid argument '" + *iter + "'");
            }
        }

        return (operationMode
                ? std::move(operationMode)
                : std::make_unique<Oper::ExecuteSteps>());
    }
}

// ------------------------------------------------------------

int main(const int argc,
         const char* argv[])
{
    const std::vector<std::string> args(argv + 1, argv + argc);

    try {
        const auto operMode = parseArguments(args);

        //

        Master& master = Master::instance();

        operMode->execute(master);
    }
    catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}
