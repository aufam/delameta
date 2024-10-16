#include "delameta/http/request.h"
#include "delameta/http/chunked.h"
#include "delameta/utils.h"
#include <etl/string_view.h>

using namespace Project;
using namespace Project::delameta;

void delameta_detail_http_request_response_reader_parse_headers_body(
    std::string_view sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    std::string_view& host_value,
    Descriptor& desc,
    Stream& body_stream
);

http::RequestReader::RequestReader(Descriptor& desc, std::vector<uint8_t>& data) : data() { parse(desc, data); }
http::RequestReader::RequestReader(Descriptor& desc, std::vector<uint8_t>&& data) : data(std::move(data)) { parse(desc, this->data); }

void http::RequestReader::parse(Descriptor& desc, std::vector<uint8_t>& data) {
    auto sv = std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    while (sv.find("\r\n\r\n") == std::string::npos and sv.find("\n\n") == std::string::npos) {
        auto read_result = desc.read();
        if (read_result.is_err()) {
            return;
        }

        data.insert(data.end(), read_result.unwrap().begin(), read_result.unwrap().end());
        sv = std::string_view(reinterpret_cast<const char*>(data.data()), data.size());
    }

    auto first_line = string_view_consume_line(sv);
    auto [method, path, version] = etl::string_view(first_line.data(), first_line.size()).split<3>(" ");

    this->method = std::string_view(method.data(), method.len());
    this->url = std::string(path.data(), path.len());
    this->version = std::string_view(version.data(), version.len());

    std::string_view host = "";
    delameta_detail_http_request_response_reader_parse_headers_body(sv, this->headers, host, desc, this->body_stream);

    if (not host.empty()) {
        this->url.host = host;
    }
}

auto http::RequestWriter::dump() -> Stream {
    std::string first_line;
    first_line.reserve(method.size() + 1 + url.full_path.size() + 1 + version.size() + 2);
    first_line += method;
    first_line += ' ';
    first_line += url.full_path;
    first_line += ' ';
    first_line += version;
    first_line += "\r\n";

    Stream s;
    s << [buffer=std::string(), first_line=std::move(first_line), headers=std::move(headers)](Stream&) mutable -> std::string_view {
        size_t total = first_line.size();
        for (const auto &[key, value]: headers) {
            total += key.size() + 2 + value.size() + 2;
        }
        total += 2;

        buffer.reserve(total);
        buffer += std::move(first_line);
        while (not headers.empty()) {
            auto it = headers.begin();
            buffer += std::move(it->first);
            buffer += ": ";
            buffer += std::move(it->second);
            buffer += "\r\n";
            headers.erase(it);
        }
        buffer += "\r\n";
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

http::RequestReader::operator RequestWriter() const {
    std::unordered_map<std::string, std::string> headers;
    for (auto [key, value] : this->headers) {
        headers.emplace(key, value);
    }
    return {
        .method=std::string(method),
        .url=url,
        .version=std::string(version),
        .headers=std::move(headers),
        .body=std::move(body),
        .body_stream=std::move(body_stream),
    };
}

void delameta_detail_http_request_response_reader_parse_headers_body(
    std::string_view sv,
    std::unordered_map<std::string_view, std::string_view>& headers,
    std::string_view& host_value,
    Descriptor& desc,
    Stream& body_stream
) {
    std::string_view content_length_value;
    std::string_view transfer_encoding_value;

    for (;;) {
        auto line = string_view_consume_line(sv);
        if (line.empty()) break;

        std::string_view key, value;
        auto key_end = line.find(':');

        key = line.substr(0, key_end);
        if (key_end != std::string::npos) {
            value = line.substr(key_end + 1);
        } else {
            key = line;
        }

        while (!key.empty() && key.back() == ' ') {
            key = key.substr(0, key.size() - 1);
        }

        while (!value.empty() && value.front() == ' ') {
            value = value.substr(1);
        }

        if (content_length_value.empty() and (key == "Content-Length" or key == "content-length")) {
            content_length_value = value;
        }

        if (transfer_encoding_value.empty() and (key == "Transfer-Encoding" or key == "transfer-encoding")) {
            transfer_encoding_value = value;
        }

        if (host_value.empty() and (key == "Host" or key == "host")) {
            host_value = value;
        }

        headers[key] = value;
    }

    // the remaining payload data is body
    auto body = sv;

    // prioritize transfer encoding
    if (transfer_encoding_value == "chunked") {
        if (not body.empty()) {
            // create string view descriptor, decode it, and push to body stream
            auto svd = new StringViewDescriptor(body);
            body_stream << http::chunked_decode(*svd);
            body_stream.at_destructor = [svd]() { delete svd; };
        }

        body_stream << http::chunked_decode(desc);
        return;
    }

    // put the already read body in front of the body stream rules
    if (not body.empty()) body_stream << body;

    if (not content_length_value.empty()) {
        size_t content_length = string_num_into<size_t>(content_length_value).unwrap_or(0);
        if (content_length > body.size()) {
            // let the descriptor read again later as stream rules
            body_stream << desc.read_as_stream(content_length - body.size());
        }
    }
}
