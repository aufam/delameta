#include "delameta/http/response.h"
#include "etl/string_view.h"

using namespace Project;
using namespace Project::delameta;

void delameta_detail_http_request_response_reader_parse_headers_body(
    etl::StringView sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    Descriptor& desc,
    Stream& body_stream
);

auto http::ResponseWriter::dump() -> Stream {
    Stream s;
    std::string payload = std::move(version) + " " + std::to_string(status) + " " + std::move(status_string) + "\r\n";
    for (auto &[key, value]: headers) {
        payload += std::move(key) + ": " + std::move(value) + "\r\n";
    }

    payload += "\r\n";

    s << std::move(payload);

    if (!body.empty()) {
        s << std::move(body);
    }

    if (!body_stream.rules.empty()) {
        s << std::move(body_stream);
    }

    return s;
}

http::ResponseReader::ResponseReader(Descriptor& desc, const std::vector<uint8_t>& data) : data() { parse(desc, data); }
http::ResponseReader::ResponseReader(Descriptor& desc, std::vector<uint8_t>&& data) : data(std::move(data)) { parse(desc, this->data); }

void http::ResponseReader::parse(Descriptor& desc, const std::vector<uint8_t>& data) {
    auto sv = etl::string_view(data.data(), data.size());

    auto [version, status] = sv.split<2>(" ");
    if (not status) {
        return;
    }

    auto consumed = (status.end() - sv.begin()) + 1; 
    sv = sv.substr(consumed, sv.len() - consumed);
    auto status_string = sv.split<1>("\n")[0];
    
    auto sv_begin = status_string.end() + 1;
    size_t sv_len = sv.end() > sv_begin && status_string.end() != nullptr ? sv.end() - sv_begin : 0;
    sv = etl::StringView{sv_begin, sv_len};

    if (status_string and status_string.back() == '\r')
        status_string = status_string.substr(0, status_string.len() - 1);
    
    this->version = std::string_view(version.data(), version.len());
    this->status = status.to_int();
    this->status_string = std::string_view(status_string.data(), status_string.len());
    delameta_detail_http_request_response_reader_parse_headers_body(sv, this->headers, desc, this->body_stream);
}

http::ResponseReader::operator ResponseWriter() const {
    std::unordered_map<std::string, std::string> headers;
    for (auto [key, value] : this->headers) {
        headers[std::string(key)] = std::string(value);
    }
    return {
        .version=std::string(version),
        .status=status,
        .status_string=std::string(status_string),
        .headers=std::move(headers),
        .body=std::move(body),
        .body_stream=std::move(body_stream),
    };
}