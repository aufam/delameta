#include "delameta/http/request.h"
#include <fcntl.h>
#include <etl/string_view.h>

using namespace Project;
using namespace Project::delameta;

void delameta_detail_http_request_response_reader_parse_headers_body(
    etl::StringView sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    socket::Stream& in_stream,
    delameta::Stream& body_stream
);

http::RequestReader::RequestReader(socket::Stream& in_stream, const std::vector<uint8_t>& data) : data() { parse(in_stream, data); }
http::RequestReader::RequestReader(socket::Stream& in_stream, std::vector<uint8_t>&& data) : data(std::move(data)) { parse(in_stream, this->data); }

void http::RequestReader::parse(socket::Stream& in_stream, const std::vector<uint8_t>& data) {
    auto sv = etl::string_view(data.data(), data.size());

    auto request_line = sv.split<3>(" ");
    if (request_line.len() < 3)
        return;
    
    auto method = request_line[0];
    auto path = request_line[1];
    auto version = request_line[2].split<1>("\n")[0];
    
    sv = version.end() + 1;
    if (version and version.back() == '\r')
        version = version.substr(0, version.len() - 1);
    
    this->method = std::string_view(method.data(), method.len());
    this->url = std::string(path.data(), path.len());
    this->version = std::string_view(version.data(), version.len());
    delameta_detail_http_request_response_reader_parse_headers_body(sv, this->headers, in_stream, this->body_stream);

    std::string_view host = "";
    auto it = this->headers.find("Host");
    if (it == this->headers.end()) {
        it = this->headers.find("host");
    }
    if (it != this->headers.end()) {
        host = it->second;
    }
    if (!host.empty()) {
        this->url.host = host;
    }
}

auto http::RequestWriter::dump() -> Stream {
    Stream s;
    s << std::move(method) + " " + std::move(url.full_path) + " " + std::move(version) + "\r\n";
    for (auto &[key, value]: headers) {
        s << std::move(key) + ": " + std::move(value) + "\r\n";
    }

    s << "\r\n";
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
        headers[std::string(key)] = std::string(value);
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
    etl::StringView sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    socket::Stream& in_stream,
    delameta::Stream& body_stream
) {
    auto head_end = sv.find("\r\n\r\n");
    auto body_start = head_end + 4;
    if (head_end >= sv.len()) {
        head_end = sv.find("\n\n");
        body_start -= 2;
        if (head_end >= sv.len()) {
            body_start -= 2;
        }
    }

    auto hsv = sv.substr(0, head_end);
    auto body_length = sv.len() - body_start;
    if (body_length > 0) {
        body_stream << std::string(sv.data() + body_start, body_length);
    }

    bool content_length_found = false;
    bool connection_found = false;
    bool keep_alive_found = false;

    for (auto line : hsv.split<32>("\n")) {
        auto key = line.split<1>(":")[0];
        auto value = etl::StringView{};
        if (key.end() != line.end()) {
            value = {key.end() + 1, line.len() - key.len() - 1};
        }

        if (value and value.back() == '\r')
            value = value.substr(0, value.len() - 1);
        
        while (value and value.front() == ' ') 
            value = value.substr(1, value.len() - 1);
        

        // handle content length
        if (not content_length_found and (key == "Content-Length" or key == "content-length")) {
            content_length_found = true;

            int cl = value.to_int();
            if (cl >= int(body_length)) {
                cl -= body_length;
            }

            while (cl > 0) {
                int n = std::min(cl, MAX_HANDLE_SZ);
                body_stream << [&in_stream, n, buffer=std::vector<uint8_t>{}]() mutable -> std::string_view {
                    auto data = in_stream.receive_until(n);
                    if (data.is_ok()) {
                        buffer = std::move(data.unwrap());
                    }
                    return {reinterpret_cast<const char*>(buffer.data()), buffer.size()};
                };
                cl -= n;
            }
        }

        // handle connection
        if (not connection_found and (key == "Connection" or key == "connection")) {
            connection_found = true;
            if (value == "keep-alive") {
                in_stream.keep_alive = true;
            } else if (value == "close") {
                in_stream.keep_alive = false;
            }
        }

        // handle keep alive
        if (not keep_alive_found and (key == "Keep-Alive" or key == "keep-alive")) {
            keep_alive_found = true;
            auto timeout_idx = value.find("timeout=");
            if (timeout_idx < value.len()) {
                in_stream.timeout = ::atoi(value.data() + timeout_idx + 9);
            }
            auto max_idx = value.find("max=");
            if (max_idx < value.len()) {
                in_stream.max = ::atoi(value.data() + max_idx + 5);
            }
        }

        // store the header
        headers[std::string_view(key.data(), key.len())] = std::string_view(value.data(), value.len());
    }
}