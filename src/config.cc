/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "config.hh"

#include <csignal>
#include <cstring>
#include <fstream>
#include <sstream>

#include "unistd.h"

using namespace std;

//

namespace
{
    string getCWD()
    {
        enum { BufferSize = 500 };
        char buffer[BufferSize];

        if (getcwd(buffer, BufferSize) == nullptr) {
            throw runtime_error("getcwd: "s + strerror(errno));
        }

        return buffer;
    }

    string removeLastComponent(const string& path)
    {
        const string::size_type dash = path.rfind('/');

        if (dash == 0
            || dash == string::npos)
        {
            return "/";
        }
        else {
            return path.substr(0, dash);
        }
    }

    //

    volatile bool s_interrupted = false;

    void sigIntHandler(int)
    {
        s_interrupted = true;
    }

    void setupSignals()
    {
        using namespace std::string_literals;
        //

        // SIGINT

        struct sigaction act;

        memset(&act, 0, sizeof(act));
        sigemptyset(&act.sa_mask);
        act.sa_handler = &sigIntHandler;
        act.sa_flags   = SA_RESETHAND | SA_RESTART;

        if (sigaction(SIGINT, &act, nullptr) != 0) {
            throw std::runtime_error("sigaction(SIGINT) failed: "s + strerror(errno));
        }
    }
}

// ------------------------------------------------------------

const Config& Config::instance()
{
    static Config s_config(s_interrupted);
    return s_config;
}

Config::Config(volatile bool& i)
    : interrupted(i)
{
    setupSignals();

    //

    static const string ConfFile = "/.swd.conf";

    ifstream ifs;

    // find configuration file

    string basePath = getCWD();

    for (;
         ;
         basePath = removeLastComponent(basePath))
    {
        const string tryConf = (basePath == "/"
                                ? ConfFile
                                : basePath + ConfFile);

        ifs.open(tryConf);

        if (ifs) {
            if (chdir(basePath.c_str()) != 0) {
                throw runtime_error("chdir: " + basePath);
            }

            break;
        }

        if (basePath == "/") {
            throw runtime_error("No configuration found!");
        }
    }

    // ifs is open

    string line;

    while (getline(ifs, line))
    {
        {
            const std::string::size_type hash = line.find('#');
            if (hash != std::string::npos) {
                line.erase(hash);
            }
        }

        //

        istringstream iss(line);
        string token;

        if (!(iss >> token)) {
            continue;
        }

        if (token == "add_path")
        {
            string add;
            if (iss >> add)
            {
                string envPath = getenv("PATH");

                if (envPath.empty()) {
                    envPath = basePath + '/' + add;
                }
                else {
                    envPath += ':' + basePath + '/' + add;
                }

                setenv("PATH", envPath.c_str(), 1);
            }
            else {
                throw runtime_error("configuration error: invalid 'add_path'");
            }
        }
        else if (token == "bash_bin")
        {
            if (!(iss >> bash_bin)) {
                throw runtime_error("configuration error: invalid 'bash_bin'");
            }
        }
        else if (token == "cache_dir")
        {
            if (!(iss >> cache_dir)) {
                throw runtime_error("configuration error: invalid 'cache_dir'");
            }
        }
        else if (token == "env")
        {
            string var;
            string value;

            if (iss >> var >> ws
                && getline(iss, value))
            {
                setenv(var.c_str(), value.c_str(), 1);
            }
            else {
                throw runtime_error("configuration error: invalid 'env'");
            }
        }
        else if (token == "hash_bin")
        {
            if (!(iss >> hash_bin)) {
                throw runtime_error("configuration error: invalid 'hash_bin'");
            }
        }
        else if (token == "hashsum_size")
        {
            if (!(iss >> hashsum_size)) {
                throw runtime_error("configuration error: invalid 'hashsum_size'");
            }
        }
        else if (token == "root")
        {
            if (iss >> root) {
                setenv("SWD_ROOT", root.c_str(), 1);
            }
            else {
                throw runtime_error("configuration error: invalid 'root'");
            }
        }
    }
}

Config::~Config() = default;
