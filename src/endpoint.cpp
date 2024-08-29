#include "delameta/endpoint.h"
#include "delameta/debug.h"
#include "delameta/file.h"
#include "delameta/serial.h"
#include "delameta/tcp.h"
#include "delameta/udp.h"

using namespace Project;
using namespace delameta;
using etl::Err;
using etl::Ok;

Result<Endpoint> EndpointFactoryStdInOut(const char* file, int line, const URL& uri);
Result<Endpoint> EndpointFactoryFile(const char* file, int line, const URL& uri);
Result<Endpoint> EndpointFactorySerial(const char* file, int line, const URL& uri);
Result<Endpoint> EndpointFactoryTCP(const char* file, int line, const URL& uri);
Result<Endpoint> EndpointFactoryUDP(const char* file, int line, const URL& uri);

std::unordered_map<std::string_view, EndpointFactoryFunction> delameta_endpoints_map {
    {"stdinout", &EndpointFactoryStdInOut},
    {"file",     &EndpointFactoryFile},
    {"serial",   &EndpointFactorySerial},
    {"tcp",      &EndpointFactoryTCP},
    {"udp",      &EndpointFactoryUDP},
};

Result<Endpoint> Endpoint::Open(const char* file, int line, std::string_view uri) {
    URL u = std::string(uri);
    auto it = delameta_endpoints_map.find(u.protocol);
    if (it == delameta_endpoints_map.end()) {
        warning(file, line, "Unknown endpoint" + u.url);
        return Err(Error{-1, "Unknown endpoint"});
    }

    return it->second(file, line, u);
}

Endpoint::Endpoint(Descriptor* desc) : desc(desc) {}

Endpoint::~Endpoint() { delete desc; }

Endpoint::Endpoint(Endpoint&& other) : desc(std::exchange(other.desc, nullptr)) {}

Endpoint& Endpoint::operator=(Endpoint&& other) {
    if (this == &other) return *this;
    this->~Endpoint();
    desc = std::exchange(other.desc, nullptr);
    return *this;
}

auto Endpoint::read() -> Result<std::vector<uint8_t>> {
    if (desc == nullptr) return Err(Error{-1, "Null descriptor"});
    return desc->read();
}

auto Endpoint::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    if (desc == nullptr) return Err(Error{-1, "Null descriptor"});
    return desc->read_until(n);
}

auto Endpoint::read_as_stream(size_t n) -> Stream {
    if (desc == nullptr) return {};
    return desc->read_as_stream(n);
}

auto Endpoint::write(std::string_view data) -> Result<void> {
    if (desc == nullptr) return Err(Error{-1, "Null descriptor"});
    return desc->write(data);
}

#if defined(__linux__) || defined(__APPLE__)
#include <iostream>

class StdInOut : public Descriptor {
public:
    StdInOut(const char* file, int line) : file(file), line(line) {}
    virtual ~StdInOut() = default;

    StdInOut(StdInOut&&) noexcept = default;
    StdInOut& operator=(StdInOut&&) noexcept = default;

    Result<std::vector<uint8_t>> read() override {
        try {
            std::string str;
            std::getline(std::cin, str);
            return Ok(std::vector<uint8_t>(str.begin(), str.end()));
        } catch (const std::exception& e) {
            Error err{-1, e.what()};
            warning(file, line, err.what);
            return Err(std::move(err));
        }
    }

    Result<std::vector<uint8_t>> read_until(size_t n) override {
        std::vector<uint8_t> data;
        data.reserve(n);

        char buffer[256];
        while (data.size() < n) {
            size_t bytes_to_read = std::min(n - data.size(), sizeof(buffer));
            
            std::cin.read(buffer, bytes_to_read);
            std::streamsize bytes_read = std::cin.gcount();
            if (bytes_read <= 0) {
                break;
            }

            data.insert(data.end(), buffer, buffer + bytes_read);
            if (data.size() >= n) {
                break;
            }
        }

        return Ok(std::move(data));
    }

    Stream read_as_stream(size_t n) override {
        Stream s;
        s << [this, n, data=std::vector<uint8_t>()]() mutable -> std::string_view {
            auto read_result = read_until(n);
            if (read_result.is_ok()) data = std::move(read_result.unwrap());
            return {reinterpret_cast<const char*>(data.data()), data.size()};
        };
        return s;
    }

    Result<void> write(std::string_view data) override {
        try {
            std::cout << data;
            if (std::cout.fail()) {
                throw std::runtime_error("Write operation failed");
            }
            return Ok();
        } catch (const std::exception& e) {
            Error err{-1, e.what()};
            warning(file, line, err.what);
            return Err(std::move(err));
        }
    }

    const char* file;
    int line;
};

Result<Endpoint> EndpointFactoryStdInOut(const char* file, int line, const URL&) {
    return Ok(new StdInOut(file, line));
}
#endif

Result<Endpoint> EndpointFactoryFile(const char* file, int line, const URL& uri) {
    File::Args args;

    auto it = uri.queries.find("mode");
    if (it != uri.queries.end()) {
        args.mode = it->second;
    }

    args.filename = uri.host.size() > 0 ? uri.host : uri.path;
    auto fd = File::Open(file, line, std::move(args));
    if (fd.is_err()) {
        return Err(std::move(fd.unwrap_err()));
    }
    
    return Ok(new File(std::move(fd.unwrap())));
}

Result<Endpoint> EndpointFactorySerial(const char* file, int line, const URL& uri) {
    Serial::Args args;

    auto it = uri.queries.find("baud");
    if (it != uri.queries.end()) {
        args.baud = std::stoi(it->second);
    } 

    it = uri.queries.find("timeout");
    if (it != uri.queries.end()) {
        args.timeout = std::stoi(it->second);
    } 

    args.port = uri.host.size() > 0 ? uri.host : uri.path;
    auto fd = Serial::Open(file, line, std::move(args));
    if (fd.is_err()) {
        return Err(std::move(fd.unwrap_err()));
    }
    
    return Ok(new Serial(std::move(fd.unwrap())));
}

Result<Endpoint> EndpointFactoryTCP(const char* file, int line, const URL& uri) {
    TCP::Args args;

    auto it = uri.queries.find("timeout");
    if (it != uri.queries.end()) {
        args.timeout = std::stoi(it->second);
    } 

    it = uri.queries.find("connection-timeout");
    if (it != uri.queries.end()) {
        args.connection_timeout = std::stoi(it->second);
    } 

    args.host = uri.host;

    auto fd = TCP::Open(file, line, std::move(args));
    if (fd.is_err()) {
        return Err(std::move(fd.unwrap_err()));
    }
    
    return Ok(new TCP(std::move(fd.unwrap())));
}

Result<Endpoint> EndpointFactoryUDP(const char* file, int line, const URL& uri) {
    UDP::Args args;

    auto it = uri.queries.find("timeout");
    if (it != uri.queries.end()) {
        args.timeout = std::stoi(it->second);
    } 

    args.host = uri.host;

    auto fd = UDP::Open(file, line, std::move(args));
    if (fd.is_err()) {
        return Err(std::move(fd.unwrap_err()));
    }
    
    return Ok(new UDP(std::move(fd.unwrap())));
}