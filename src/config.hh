/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#pragma once

#include <string>

struct Config {
    std::string root;
    std::string cache_dir = ".swd-cache";

    std::string bash_bin = "/bin/bash";
    std::string hash_bin = "/usr/bin/sha256sum";
    std::string::size_type hashsum_size = 64;

    //

    volatile bool& interrupted;

    //

    static const Config& instance();

private:
    Config(volatile bool& i);
    ~Config();
};
