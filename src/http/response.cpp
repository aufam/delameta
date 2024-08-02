#include "delameta/http/response.h"
#include "etl/string_view.h"

using namespace Project;
using namespace Project::delameta;

void delameta_detail_http_request_response_reader_parse_headers_body(
    etl::StringView sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    socket::Stream& in_stream,
    delameta::Stream& body_stream
);

auto http::ResponseWriter::dump() -> Stream {
    Stream s;
    s << std::move(version) + " " + std::to_string(status) + " " + std::move(status_string) + "\r\n";
    for (auto &[key, value]: headers) {
        s << std::move(key) + ": " + std::move(value) + "\r\n";
    }

    s << "\r\n";
    if (!body.empty()) {
        s << std::move(body);
    }

    if (!body_stream.rules.empty()) {
        s << std::move(body_stream);
    }

    return s;
}

http::ResponseReader::ResponseReader(socket::Stream& in_stream, const std::vector<uint8_t>& data) : data() { parse(in_stream, data); }
http::ResponseReader::ResponseReader(socket::Stream& in_stream, std::vector<uint8_t>&& data) : data(std::move(data)) { parse(in_stream, this->data); }

void http::ResponseReader::parse(socket::Stream& in_stream, const std::vector<uint8_t>& data) {
    auto sv = etl::string_view(data.data(), data.size());
    auto methods = sv.split<3>(" ");

    if (methods.len() < 3)
        return;
    
    auto version = methods[0];
    auto status = methods[1].to_int();
    auto status_string = methods[2].split<1>("\n")[0];
    
    sv = status_string.end() + 1;
    if (status_string and status_string.back() == '\r')
        status_string = status_string.substr(0, status_string.len() - 1);
    
    this->version = std::string_view(version.data(), version.len());
    this->status = status;
    this->status_string = std::string_view(status_string.data(), status_string.len());
    delameta_detail_http_request_response_reader_parse_headers_body(sv, this->headers, in_stream, this->body_stream);
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