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
    std::string status_int = std::to_string(status);
    std::string first_line;
    first_line.reserve(version.size() + 1 + status_int.size() + 1 + status_string.size() + 2);
    first_line += version;
    first_line += ' ';
    first_line += status_int;
    first_line += ' ';
    first_line += status_string;
    first_line += "\r\n";
    
    Stream s;
    s << std::move(first_line);
    s << [buffer=std::string(), headers=std::move(headers)](Stream& s) mutable -> std::string_view {
        s.again = !headers.empty();
        if (!s.again) {
            return "\r\n";
        }

        auto it = headers.begin();
        buffer.clear();
        buffer.reserve(it->first.size() + 2 + it->second.size() + 2);
        buffer += it->first;
        buffer += ": ";
        buffer += it->second;
        buffer += "\r\n";
        headers.erase(it);
        return buffer;
    };

    if (!body.empty()) {
        s << std::move(body);
    }

    if (!body_stream.rules.empty()) {
        s << body_stream;
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