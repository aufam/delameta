#include "delameta/stream.h"
#include "delameta/debug.h"
#include "delameta/utils.h"
#include <algorithm>

using namespace Project;
using namespace delameta;

using etl::Err;
using etl::Ok;

Stream::~Stream() {
    if (at_destructor) at_destructor();
}

Stream::Stream(Stream&& other) 
    : rules(std::move(other.rules))
    , at_destructor(std::move(other.at_destructor)) {}

Stream& Stream::operator=(Stream&& other) {
    if (this == &other) return *this;
    if (at_destructor) at_destructor();
    rules = std::move(other.rules);
    at_destructor = std::move(other.at_destructor);
    return *this;
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
    else at_destructor = [f1=std::move(at_destructor), f2=std::move(other.at_destructor)]() {
        f1();
        if (f2) f2();
    };
    return (rules.splice(rules.end(), std::move(other.rules)), *this);
}

Stream& Stream::operator<<(Stream&& other) {
    if (!at_destructor) at_destructor = std::move(other.at_destructor);
    else at_destructor = [f1=std::move(at_destructor), f2=std::move(other.at_destructor)]() {
        f1();
        if (f2) f2();
    };
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

Result<void> Stream::out_with_prefix(Descriptor& des, std::function<Result<void>(std::string_view)> prefix) {
    while (!rules.empty()) {
        again = false;
        auto data = rules.front()(*this);
        
        auto [_, err] = prefix(data);
        if (err) return Err(std::move(*err));

        auto [__, err_w] = des.write(data);
        if (err_w) return Err(std::move(*err_w));

        if (!again) rules.pop_front();
    }
    return Ok();
}

std::vector<uint8_t> Stream::pop_once() {
    if (rules.empty()) return {};

    again = false;
    auto data = rules.front()(*this);

    std::vector<uint8_t> res {data.begin(), data.end()};
    if (!again) rules.pop_front();

    return res;
}

StreamSessionServer::StreamSessionServer(StreamSessionHandler handler) : handler(std::move(handler)) {}

Stream StreamSessionServer::execute_stream_session(Descriptor& desc, const std::string& name, std::vector<uint8_t>& data) {
    return handler ? handler(desc, name, data) : Stream{};
}

StreamSessionClient::StreamSessionClient(Descriptor* desc) 
    : desc(desc) 
    , is_dyn(true) {}

StreamSessionClient::StreamSessionClient(Descriptor& desc) 
    : desc(&desc) 
    , is_dyn(false) {}

StreamSessionClient::StreamSessionClient(StreamSessionClient&& other) 
    : desc(std::exchange(other.desc, nullptr)) 
    , is_dyn(other.is_dyn) {}

StreamSessionClient::~StreamSessionClient() {
    if (desc && is_dyn) {
        delete desc;
    }
}

auto StreamSessionClient::request(Stream& in_stream) -> Result<std::vector<uint8_t>> {
    if (desc == nullptr) {
        PANIC("Fatal error: No descriptor created in the session");
    }

    in_stream >> *desc;
    return desc->read();
}

StringViewDescriptor::StringViewDescriptor(std::string_view sv) : sv(sv) {}

auto StringViewDescriptor::read() -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res(sv.begin(), sv.end());
    sv = "";
    return Ok(std::move(res));
}

auto StringViewDescriptor::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res(sv.begin(), sv.begin() + n);
    sv = n > sv.size() ? "" : sv.substr(n);
    return Ok(std::move(res));
}

auto StringViewDescriptor::read_as_stream(size_t n) -> Stream {
    Stream s;
    s << sv.substr(0, n);
    sv = n > sv.size() ? "" : sv.substr(n);
    return s;
}

auto StringViewDescriptor::write(std::string_view) -> Result<void> {
    return Err(delameta::Error(-1, "Not implemented"));
}


auto StringViewDescriptor::read_line() -> std::string_view {
   return string_view_consume_line(sv);
}

auto StringStream::read() -> Result<std::vector<uint8_t>> {
    if (buffer.empty()) {
        return Err("buffer is empty");
    }

    std::vector<uint8_t> res(buffer.front().begin(), buffer.front().end());
    buffer.pop_front();
    return Ok(std::move(res));
}

auto StringStream::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res;
    res.reserve(n);

    while (n > 0) {
        if (buffer.empty()) {
            return Err("buffer is empty");
        }

        auto &front = buffer.front();
        auto front_size = front.size();

        if (front_size > n) {
            res.insert(res.end(), front.begin(), front.begin() + n);
            front = front.substr(n);
            n = 0;
        } else {
            buffer.pop_front();
            n -= front_size;
            res.insert(res.end(), front.begin(), front.end());
        }
    }

    return Ok(std::move(res));
}

auto StringStream::read_as_stream(size_t n) -> Stream {
    Stream s;
    s << [buffer=std::move(buffer), data=std::string(), n](Stream& s) mutable -> std::string_view {
        if (buffer.empty()) {
            return {};
        }

        auto &front = buffer.front();
        auto front_size = front.size();
        if (front_size > n) {
            front = front.substr(n);
            data = front;
            n = 0;
        } else {
            data = front;
            buffer.pop_front();
            n -= front_size;
        }

        s.again = !buffer.empty() and n > 0;
        return data;
    };
    return s;
}

auto StringStream::write(std::string_view sv) -> Result<void> {
    buffer.push_back(std::string(sv));
    return Ok();
}

void StringStream::flush() {
    buffer.clear();
}

auto StringStream::operator<<(std::string s) -> StringStream& {
    buffer.push_back(std::move(s));
    return *this;
}

auto StringStream::operator>>(std::string& s) -> StringStream& {
    if (!buffer.empty()) {
        s = std::move(buffer.front());
        buffer.pop_front();
    }
    return *this;
}

auto StreamDescriptor::read() -> Result<std::vector<uint8_t>> {
    if (!buffer.empty()) {
        std::vector<uint8_t> res(buffer.begin(), buffer.end());
        buffer.clear();
        return Ok(std::move(res));
    }

    if (!stream.rules.empty()) {
        return Ok(stream.pop_once());
    }

    return Err("Stream is empty");
}

auto StreamDescriptor::read_until(size_t n) -> Result<std::vector<uint8_t>> {
    std::vector<uint8_t> res;
    res.reserve(n);

    while (n > 0) {
        if (!buffer.empty()) {
            auto m = std::min(n, buffer.size());
            res.insert(res.end(), buffer.begin(), buffer.begin() + m);
            buffer = buffer.substr(m);
            n -= m;
        } else {
            if (!stream.rules.empty()) {
                return Err("Stream is empty");
            }
            auto data = stream.pop_once();
            buffer = std::string(data.begin(), data.end());
        }
    }

    return Ok(std::move(res));
}

auto StreamDescriptor::read_as_stream(size_t n) -> Stream {
    Stream s;
    
    s << [this, buf=std::string(), n](Stream& s) mutable -> std::string_view {
        while (n > 0) {
            if (!buffer.empty()) {
                auto m = std::min(n, buffer.size());
                buf = buffer.substr(0, m);
                buffer = buffer.substr(m);
                n -= m;
                s.again = n > 0;                
                return buf;
            } else {
                if (!stream.rules.empty()) {
                    return "";
                }
                auto data = stream.pop_once();
                buffer = std::string(data.begin(), data.end());
            }
        }

        return "";
    };

    return s;
}

auto StreamDescriptor::write(std::string_view sv) -> Result<void> {
    stream << std::string(sv);
    return Ok();
}

void StreamDescriptor::flush() {
    buffer.clear();
    stream.rules.clear();
    if (stream.at_destructor) stream.at_destructor();
}

auto StreamDescriptor::operator<<(std::string s) -> StreamDescriptor& {
    stream << std::move(s);
    return *this;
}

auto StreamDescriptor::operator<<(std::vector<uint8_t> s) -> StreamDescriptor& {
    stream << std::move(s);
    return *this;
}

auto StreamDescriptor::operator<<(Stream& s) -> StreamDescriptor& {
    stream << s;
    return *this;
}

auto StreamDescriptor::operator>>(Stream& s) -> StreamDescriptor& {
    if (!buffer.empty()) {
        s << std::move(buffer);
    }

    s << stream;
    return *this;
}
