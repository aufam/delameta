#include "delameta/serial/client.h"

using namespace Project;
using namespace Project::delameta;

auto serial::Client::New(const char* file, int line, Args args) -> Result<Client> {
    return FileDescriptor::Open(file, line, args.port.c_str(), 0).then([&](FileDescriptor serial) {
        serial.timeout = args.timeout;
        return Client(new FileDescriptor(std::move(serial)));
    });
}

serial::Client::Client(FileDescriptor* fd) : StreamSessionClient(fd) {}
