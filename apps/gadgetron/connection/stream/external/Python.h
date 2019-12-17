#pragma once

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"

#include <boost/process.hpp>

#pragma clang diagnostic pop

#include "connection/Config.h"

#include "Context.h"

namespace Gadgetron::Server::Connection::Stream {
    boost::process::child start_python_module(const Config::Execute &, unsigned short port, const Gadgetron::Core::Context &);
    bool python_available() noexcept;
}

