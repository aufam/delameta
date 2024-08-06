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

serial::Client::Client(FileDescriptor* fd) : fd(fd) {}

serial::Client::Client(Client&& other) : fd(std::exchange(other.fd, nullptr)) {}

auto serial::Client::operator=(Client&& other) -> Client& {
    if (this == &other) {
        return *this;
    }

    this->~Client();
    fd = std::exchange(other.fd, nullptr);
    return *this;
}

serial::Client::~Client() {
    if (fd) {
        delete fd;
        fd = nullptr;
    }
}

auto serial::Client::request(Stream& in_stream) -> Result<std::vector<uint8_t>> {
    if (fd == nullptr) {
        std::string what = "No client fd created";
        warning(__FILE__, __LINE__, what);
        return Err(Error{-1, what});
    }

    in_stream >> *fd;
    return fd->read();
}