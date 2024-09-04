#include "delameta/http/request.h"
#include <etl/string_view.h>

using namespace Project;
using namespace Project::delameta;

void delameta_detail_http_request_response_reader_parse_headers_body(
    etl::StringView sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    Descriptor& desc,
    Stream& body_stream
);

http::RequestReader::RequestReader(Descriptor& desc, const std::vector<uint8_t>& data) : data() { parse(desc, data); }
http::RequestReader::RequestReader(Descriptor& desc, std::vector<uint8_t>&& data) : data(std::move(data)) { parse(desc, this->data); }

void http::RequestReader::parse(Descriptor& desc, const std::vector<uint8_t>& data) {
    auto sv = etl::string_view(data.data(), data.size());

    auto [method, path] = sv.split<2>(" ");
    if (not path) {
        return;
    }
    
    auto consumed = (path.end() - sv.begin()) + 1; 
    sv = sv.substr(consumed, sv.len() - consumed);
    auto version = sv.split<1>("\n")[0];
    
    auto sv_begin = version.end() + 1;
    size_t sv_len = sv.end() > sv_begin && version.end() != nullptr ? sv.end() - sv_begin : 0;
    sv = etl::StringView{sv_begin, sv_len};

    if (version and version.back() == '\r')
        version = version.substr(0, version.len() - 1);
    
    this->method = std::string_view(method.data(), method.len());
    this->url = std::string(path.data(), path.len());
    this->version = std::string_view(version.data(), version.len());
    delameta_detail_http_request_response_reader_parse_headers_body(sv, this->headers, desc, this->body_stream);

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
            buffer += "";
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
    Descriptor& desc,
    Stream& body_stream
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

    bool content_length_found = false;

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
            if (cl > int(body_length))  {
                body_stream << desc.read_as_stream(cl - body_length); // read the rest as stream
            } else if (cl >= 0 && cl < int(body_length)) {
                body_length = cl; // body length somehow exceeds content-length
            }
        }

        // store the header
        headers[std::string_view(key.data(), key.len())] = std::string_view(value.data(), value.len());
    }

    body_stream.rules.push_front([body=std::string(sv.data() + body_start, body_length)](Stream&) -> std::string_view {
        return body;
    });
}