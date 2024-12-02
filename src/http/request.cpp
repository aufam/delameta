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

Stream delameta_detail_http_request_response_reader_dump(
    std::string first_line,
    std::unordered_map<std::string, std::string> headers,
    std::string body,
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
    first_line.reserve(method.size() + 1 + url.full_path.size() + 1 + version.size() + 2 + 2);
    first_line += method;
    first_line += ' ';
    first_line += url.full_path;
    first_line += ' ';
    first_line += version;
    first_line += "\r\n";

    if (headers.empty()) {
        first_line += "\r\n";
    }

    return delameta_detail_http_request_response_reader_dump(
        std::move(first_line),
        std::move(headers),
        std::move(body),
        body_stream
    );
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
    headers.reserve(16);
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
    class ChunkedDescriptor : public delameta::Descriptor {
    public:
        ChunkedDescriptor(std::string_view sv, Descriptor& desc) : sv(sv), desc(desc) {}
        ~ChunkedDescriptor() = default;

        delameta::Result<std::vector<uint8_t>> read() override {
            if (not sv.empty()) {
                auto res = std::vector<uint8_t>(sv.begin(), sv.end());
                sv = "";
                return etl::Ok(std::move(res));
            }

            return desc.read();
        }

        delameta::Result<std::vector<uint8_t>> read_until(size_t n) override {
            return desc.read_until(n);
        }

        Stream read_as_stream(size_t n) override {
            return desc.read_as_stream(n);
        }

        delameta::Result<void> write(std::string_view data) override {
            return desc.write(data);
        }

        std::string_view sv;
        Descriptor& desc;
    };

    if (transfer_encoding_value == "chunked") {
        auto svd = new ChunkedDescriptor(body, desc);
        body_stream << http::chunked_decode(*svd);
        body_stream.at_destructor = [svd]() { delete svd; };
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

Stream delameta_detail_http_request_response_reader_dump(
    std::string first_line,
    std::unordered_map<std::string, std::string> headers,
    std::string body,
    Stream& body_stream
) {
    Stream s;
    s << std::move(first_line);

    if (not headers.empty()) {
        s << [buffer=std::string(), it=headers.begin(), headers=std::move(headers)](Stream& s) mutable -> std::string_view {
            auto &item = *it;
            buffer.clear();
            buffer.reserve(item.first.size() + 2 + item.second.size() + 2 + 2);

            buffer += item.first;
            buffer += ": ";
            buffer += item.second;
            buffer += "\r\n";

            ++it;
            s.again = it != headers.end();
            if (not s.again) {
                return buffer += "\r\n";
            }

            return buffer;
        };
    }

    if (!body.empty()) {
        s << std::move(body);
    }

    if (!body_stream.rules.empty()) {
        s << body_stream;
    }

    return s;
}
