#include "delameta/serial/client.h"
#include "../delameta.h"

using namespace Project;
using namespace Project::delameta;
using namespace std::literals;

using etl::Err;
using etl::Ok;
auto delameta_detail_create_serial(const char* file, int line, std::string port, int baud) -> Result<file_descriptor::Stream>;

auto serial::Client::New(const char* file, int line, Args args) -> Result<Client> {
    return delameta_detail_create_serial(file, line, args.port, args.baud).then([&](file_descriptor::Stream serial) {
        serial.timeout = args.timeout;
        info(file, line, "Created serial client: " + std::to_string(serial.fd));
        return Client(new file_descriptor::Stream(std::move(serial)));
    });
}

serial::Client::Client(file_descriptor::Stream* stream) : stream(stream) {}

serial::Client::Client(Client&& other) : stream(std::exchange(other.stream, nullptr)) {}

auto serial::Client::operator=(Client&& other) -> Client& {
    if (this == &other) {
        return *this;
    }

    this->~Client();
    stream = std::exchange(other.stream, nullptr);
    return *this;
}

serial::Client::~Client() {
    if (stream) {
        delete stream;
        stream = nullptr;
    }
}

auto serial::Client::request(delameta::Stream in_stream) -> Result<std::vector<uint8_t>> {
    if (stream == nullptr) {
        std::string what = "No client stream created";
        warning(__FILE__, __LINE__, what);
        return Err(Error{-1, what});
    }

    *stream << in_stream;
    stream->write();

    return stream->read();
}