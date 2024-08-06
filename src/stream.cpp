#include "delameta/stream.h"

using namespace Project;
using namespace delameta;

Stream& Stream::operator<<(std::string_view data) {
    return (rules.push_back([data]() { return data; }), *this);
}

Stream& Stream::operator<<(const char* data) {
    return (rules.push_back([data]() -> std::string_view { return data; }), *this);
}

Stream& Stream::operator<<(std::string data) {
    return (rules.push_back([data=std::move(data)]() -> std::string_view { return data; }), *this);
}

Stream& Stream::operator<<(std::vector<uint8_t> data) {
    return (rules.push_back([data=std::move(data)]() -> std::string_view { return {reinterpret_cast<const char*>(data.data()), data.size()}; }), *this);
}

Stream& Stream::operator<<(Stream& other) {
    return (rules.splice(rules.end(), std::move(other.rules)), *this);
}

Stream& Stream::operator<<(Stream&& other) {
    return (rules.splice(rules.end(), std::move(other.rules)), *this);
}

Stream& Stream::operator>>(Stream& other) {
    return (other.rules.splice(other.rules.end(), std::move(rules)), *this);
}

Stream& Stream::operator<<(Rule in) {
    return (rules.push_back(std::move(in)), *this);
}

Stream& Stream::operator>>(std::function<void(std::string_view)> out) {
    while (!rules.empty()) {
        auto data = rules.front()();
        out(data);
        rules.pop_front();
    }
    return *this;
}

Stream& Stream::operator<<(Descriptor& des) {
    rules.emplace_back([&des, data = std::vector<uint8_t>()]() mutable {
        auto res = des.read();
        if (res.is_err()) return std::string_view{};
        data = std::move(res.unwrap());
        return std::string_view{reinterpret_cast<const char*>(data.data()), data.size()};
    });
    return *this;
}

Stream& Stream::operator>>(Descriptor& des) {
    while (!rules.empty()) {
        auto data = rules.front()();
        auto res = des.write(data);
        if (res.is_err()) return *this;
        rules.pop_front();
    }
    return *this;
}