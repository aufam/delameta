#include "delameta/serial/client.h"
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;

auto delameta_detail_create_serial(const char* file, int line, std::string port, int baud) -> Result<FileDescriptor>;

auto serial::Client::New(const char* file, int line, Args args) -> Result<Client> {
    return delameta_detail_create_serial(file, line, args.port, args.baud).then([&](FileDescriptor serial) {
        serial.timeout = args.timeout;
        info(file, line, "Created serial client: " + std::to_string(serial.fd));
        return Client(new FileDescriptor(std::move(serial)));
    });
}

serial::Client::Client(FileDescriptor* fd) : StreamSessionClient(fd) {}
