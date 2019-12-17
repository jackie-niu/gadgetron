#ifndef GADGETRON_SERVER_H
#define GADGETRON_SERVER_H

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"

#include <boost/asio.hpp>

#pragma clang diagnostic pop
#include <boost/filesystem/path.hpp>
#include <boost/program_options/variables_map.hpp>

namespace Gadgetron::Server {

    class Server {
    public:
        Server(const boost::program_options::variables_map &args);
        void serve();

    private:
        const boost::program_options::variables_map &args;
    };
}

#endif //GADGETRON_SERVER_H
