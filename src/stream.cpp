#include "delameta/stream.h"
#include "delameta/debug.h"
#include <algorithm>

using namespace Project;
using namespace delameta;

using etl::Err;

Stream::~Stream() {
    if (at_destructor) at_destructor();
}

Stream& Stream::operator<<(std::string_view data) {
    return (rules.push_back([data](Stream&) { return data; }), *this);
}

Stream& Stream::operator<<(const char* data) {
    return (rules.push_back([data](Stream&) -> std::string_view { return data; }), *this);
}

Stream& Stream::operator<<(std::string data) {
    return (rules.push_back([data=std::move(data)](Stream&) -> std::string_view { return data; }), *this);
}

Stream& Stream::operator<<(std::vector<uint8_t> data) {
    return (rules.push_back([data=std::move(data)](Stream&) -> std::string_view { return {reinterpret_cast<const char*>(data.data()), data.size()}; }), *this);
}

Stream& Stream::operator<<(Stream& other) {
    if (!at_destructor) at_destructor = std::move(other.at_destructor);
    else at_destructor = [f1=std::move(at_destructor), f2=std::move(other.at_destructor)]() { f1(); f2(); };
    return (rules.splice(rules.end(), std::move(other.rules)), *this);
}

Stream& Stream::operator<<(Stream&& other) {
    if (!at_destructor) at_destructor = std::move(other.at_destructor);
    else at_destructor = [f1=std::move(at_destructor), f2=std::move(other.at_destructor)]() { f1(); f2(); };
    return (rules.splice(rules.end(), std::move(other.rules)), *this);
}

Stream& Stream::operator>>(Stream& other) {
    other << *this;
    return *this;
}

Stream& Stream::operator<<(Rule in) {
    return (rules.push_back(std::move(in)), *this);
}

Stream& Stream::operator<<(std::function<std::string_view()> in) {
    return (rules.push_back([in=std::move(in)](Stream&) { return in(); }), *this);
}

Stream& Stream::operator>>(std::function<void(std::string_view)> out) {
    while (!rules.empty()) {
        again = false;
        auto data = rules.front()(*this);
        out(data);
        if (!again) rules.pop_front();
    }
    return *this;
}

Stream& Stream::operator<<(Descriptor& des) {
    rules.emplace_back([&des, data = std::vector<uint8_t>()](Stream&) mutable {
        auto res = des.read();
        if (res.is_err()) return std::string_view{};
        data = std::move(res.unwrap());
        return std::string_view{reinterpret_cast<const char*>(data.data()), data.size()};
    });
    return *this;
}

Stream& Stream::operator>>(Descriptor& des) {
    while (!rules.empty()) {
        again = false;
        auto data = rules.front()(*this);
        auto res = des.write(data);
        if (res.is_err()) return *this;
        if (!again) rules.pop_front();
    }
    return *this;
}

StreamSessionServer::StreamSessionServer(StreamSessionHandler handler) : handler(std::move(handler)) {}

Stream StreamSessionServer::execute_stream_session(Descriptor& desc, const std::string& name, const std::vector<uint8_t>& data) {
    return handler ? handler(desc, name, data) : Stream{};
}

StreamSessionClient::StreamSessionClient(Descriptor* desc) : desc(desc) {}

StreamSessionClient::StreamSessionClient(StreamSessionClient&& other) : desc(std::exchange(other.desc, nullptr)) {}

StreamSessionClient& StreamSessionClient::operator=(StreamSessionClient&& other) {
    if (this == &other) return *this;
    delete desc;
    desc = std::exchange(other.desc, nullptr);
    return *this;
}

auto StreamSessionClient::request(Stream& in_stream) -> Result<std::vector<uint8_t>> {
    if (desc == nullptr) {
        std::string what = "Fatal error: No descriptor created in the session";
        warning(__FILE__, __LINE__, what);
        return Err(Error{-1, what});
    }

    in_stream >> *desc;
    return desc->read();
}
