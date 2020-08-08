/***
    This file is part of snapcast
    Copyright (C) 2014-2020  Johannes Pohl

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
***/

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

#include "common/aixlog.hpp"
#include "common/snap_exception.hpp"
#include "common/str_compat.hpp"
#include "pipe_stream.hpp"


using namespace std;

namespace streamreader
{

static constexpr auto LOG_TAG = "PipeStream";


PipeStream::PipeStream(PcmListener* pcmListener, boost::asio::io_context& ioc, const StreamUri& uri) : PosixStream(pcmListener, ioc, uri)
{
    umask(0);
    string mode = uri_.getQuery("mode", "create");

    LOG(INFO, LOG_TAG) << "PipeStream mode: " << mode << "\n";
    if ((mode != "read") && (mode != "create"))
        throw SnapException("create mode for fifo must be \"read\" or \"create\"");

    if (mode == "create")
    {
        if ((mkfifo(uri_.path.c_str(), 0666) != 0) && (errno != EEXIST))
            throw SnapException("failed to make fifo \"" + uri_.path + "\": " + cpt::to_string(errno));
    }
}


void PipeStream::do_connect()
{
    int fd = open(uri_.path.c_str(), O_RDONLY | O_NONBLOCK);
    int pipe_size = -1;
#ifndef MACOS
    pipe_size = fcntl(fd, F_GETPIPE_SZ);
#endif
    LOG(INFO, LOG_TAG) << "Stream: " << name_ << ", connect to pipe: " << uri_.path << ", fd: " << fd << ", pipe size: " << pipe_size << "\n";
    stream_ = std::make_unique<boost::asio::posix::stream_descriptor>(ioc_, fd);
    on_connect();
}
} // namespace streamreader
