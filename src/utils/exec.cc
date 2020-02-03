/* swd - Scripts with Dependencies
 * Copyright (C) 2020 Pauli Saksa
 *
 * Licensed under The MIT License, see file LICENSE.txt in this source tree.
 */

#include "exec.hh"

#include "../config.hh"

#include <cstring>
#include <sstream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;

//

namespace
{
    static void copy_fd(int oldfd, int newfd)
    {
        if (oldfd < 0 || newfd < 0)
            throw logic_error("copy_fd given invalid fds");

        if (oldfd == newfd)
            return;

        if (dup2(oldfd, newfd) < 0)
        {
            throw runtime_error("dup2("s + to_string(oldfd) + ", " + to_string(newfd) + "): " + strerror(errno));
        }
    }

    static void move_fd(int oldfd, int newfd)
    {
        copy_fd(oldfd, newfd);
        close(oldfd);
    }

    static pair<int, int> pipe_fork_exec(const string &argv0,
                                         utils::Exec::Flags flags,
                                         pid_t& pid)
    {
        int fds_read[2]  {-1, -1};
        int fds_write[2] {-1, -1};

        // create fds for reading from process

        if (flags.flag(utils::Exec::Flag::DevNullRead))
        {
            fds_read[0] = open("/dev/null", O_RDWR);
            fds_read[1] = open("/dev/null", O_RDWR);

            if (fds_read[0] < 0
                || fds_read[1] < 0)
            {
                throw runtime_error("failed to open /dev/null");
            }
        }
        else {
            if (pipe(fds_read) != 0)
            {
                throw runtime_error("pipe: "s + strerror(errno));
            }
        }

        // create fds for writing to process

        if (flags.flag(utils::Exec::Flag::DevNullWrite))
        {
            fds_write[0] = open("/dev/null", O_RDWR);
            fds_write[1] = open("/dev/null", O_RDWR);

            if (fds_write[0] < 0
                || fds_write[1] < 0)
            {
                throw runtime_error("failed to open /dev/null");
            }
        }
        else if (pipe(fds_write) != 0)
        {
            close(fds_read[0]);
            close(fds_read[1]);

            throw runtime_error("pipe: "s + strerror(errno));
        }

        // spawn process

        pid = fork();

        if (pid < 0) {
            throw runtime_error("fork: "s + strerror(errno));
        }
        else if (pid == 0) // child
        {
            close(fds_read[0]);
            close(fds_write[1]);

            move_fd(fds_write[0], STDIN_FILENO);
            move_fd(fds_read[1], STDOUT_FILENO);

            if (flags.flag(utils::Exec::Flag::RedirectErrToOut)) {
                copy_fd(STDOUT_FILENO, STDERR_FILENO);
            }

            if (!flags.flag(utils::Exec::Flag::NoShell))
            {
                execl(Config::instance().bash_bin.c_str(),
                      Config::instance().bash_bin.c_str(),
                      "-c",
                      argv0.c_str(),
                      static_cast<char *>( nullptr ));
            }
            else {
                execl(argv0.c_str(),
                      argv0.c_str(),
                      static_cast<char *>( nullptr ));
            }
            perror("execl");
            _exit(-1);
        }

        // parent

        close(fds_read[1]);
        close(fds_write[0]);

        return make_pair(fds_read[0], fds_write[1]);
    }
}

// ------------------------------------------------------------

utils::Exec::Exec(const string& argv0, Flags flags)
    : m_flags(flags),
      m_pid(0),
      fds(pipe_fork_exec(argv0, m_flags, m_pid)),
      input_buf(fds.first, _S_in),
      output_buf(fds.second, _S_out),
      input_stream(&input_buf),
      output_stream(&output_buf)
{
}

utils::Exec::Exec(const string& argv0)
    : Exec(argv0, Flags())
{
}

utils::Exec::~Exec()
{
    if (!flag(Flag::ASync)
        && m_pid > 0)
    {
        waitpid(m_pid, 0, 0);
    }
}

void utils::Exec::close_read()
{
    input_buf.close();
}

void utils::Exec::close_write()
{
    output_stream.flush();
    output_buf.close();
}

bool utils::Exec::wait()
{
    if (!m_pid)
        return false;

    int status;
    const pid_t w = waitpid(m_pid, &status, 0);

    if (w != m_pid) {
        throw runtime_error("waitpid failed, pid=" + to_string(w) + ": " + strerror(errno));
    }

    m_pid = 0;

    return WIFEXITED(status)
        && WEXITSTATUS(status) == EXIT_SUCCESS;
}
