#include "delameta/http/response.h"
#include "delameta/utils.h"
#include <string_view>

using namespace Project;
using namespace Project::delameta;

void delameta_detail_http_request_response_reader_parse_headers_body(
    std::string_view sv, 
    std::unordered_map<std::string_view, std::string_view>& headers, 
    std::string_view& host_value,
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

http::ResponseReader::ResponseReader(Descriptor& desc, std::vector<uint8_t>& data) : data() { parse(desc, data); }
http::ResponseReader::ResponseReader(Descriptor& desc, std::vector<uint8_t>&& data) : data(std::move(data)) { parse(desc, this->data); }

void http::ResponseReader::parse(Descriptor& desc, std::vector<uint8_t>& data) {
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

    auto version_end = first_line.find(' ');
    auto version = first_line.substr(0, version_end);
    first_line = version_end == std::string::npos ? "" : first_line.substr(version_end + 1);

    auto status_end = first_line.find(' ');
    auto status = first_line.substr(0, status_end);
    first_line = version_end == std::string::npos ? "" : first_line.substr(status_end + 1);

    auto status_string = first_line;

    this->version = version;
    this->status = string_num_into<int>(status).unwrap_or(-1);
    this->status_string = status_string;

    std::string_view dummy_host = "";
    delameta_detail_http_request_response_reader_parse_headers_body(sv, this->headers, dummy_host, desc, this->body_stream);
}

http::ResponseReader::operator ResponseWriter() const {
    std::unordered_map<std::string, std::string> headers;
    for (auto [key, value] : this->headers) {
        headers.emplace(key, value);
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

auto http::status_to_string(int status) -> std::string {
    switch (status) {
        // 100
        case http::StatusContinue           : return "Continue"; // RFC 9110, 15.2.1
        case http::StatusSwitchingProtocols : return "Switching Protocols"; // RFC 9110, 15.2.2
        case http::StatusProcessing         : return "Processing"; // RFC 2518, 10.1
        case http::StatusEarlyHints         : return "EarlyHints"; // RFC 8297

        // 200
        case http::StatusOK                   : return "OK"; // RFC 9110, 15.3.1
        case http::StatusCreated              : return "Created"; // RFC 9110, 15.3.2
        case http::StatusAccepted             : return "Accepted"; // RFC 9110, 15.3.3
        case http::StatusNonAuthoritativeInfo : return "Non Authoritative Info"; // RFC 9110, 15.3.4
        case http::StatusNoContent            : return "No Content"; // RFC 9110, 15.3.5
        case http::StatusResetContent         : return "Reset Content"; // RFC 9110, 15.3.6
        case http::StatusPartialContent       : return "Partial Content"; // RFC 9110, 15.3.7
        case http::StatusMultiStatus          : return "Multi Status"; // RFC 4918, 11.1
        case http::StatusAlreadyReported      : return "Already Reported"; // RFC 5842, 7.1
        case http::StatusIMUsed               : return "IM Used"; // RFC 3229, 10.4.1

        // 300
        case http::StatusMultipleChoices   : return "Multiple Choices"; // RFC 9110, 15.4.1
        case http::StatusMovedPermanently  : return "Moved Permanently"; // RFC 9110, 15.4.2
        case http::StatusFound             : return "Found"; // RFC 9110, 15.4.3
        case http::StatusSeeOther          : return "See Other"; // RFC 9110, 15.4.4
        case http::StatusNotModified       : return "Not Modified"; // RFC 9110, 15.4.5
        case http::StatusUseProxy          : return "Use Proxy"; // RFC 9110, 15.4.6
        case http::StatusTemporaryRedirect : return "Temporary Redirect"; // RFC 9110, 15.4.8
        case http::StatusPermanentRedirect : return "Permanent Redirect"; // RFC 9110, 15.4.9

        // 400
        case http::StatusBadRequest                   : return "Bad Request"; // RFC 9110, 15.5.1
        case http::StatusUnauthorized                 : return "Unauthorized"; // RFC 9110, 15.5.2
        case http::StatusPaymentRequired              : return "Payment Required"; // RFC 9110, 15.5.3
        case http::StatusForbidden                    : return "Forbidden"; // RFC 9110, 15.5.4
        case http::StatusNotFound                     : return "Not Found"; // RFC 9110, 15.5.5
        case http::StatusMethodNotAllowed             : return "Method Not Allowed"; // RFC 9110, 15.5.6
        case http::StatusNotAcceptable                : return "Not Acceptable"; // RFC 9110, 15.5.7
        case http::StatusProxyAuthRequired            : return "Proxy AuthRequired"; // RFC 9110, 15.5.8
        case http::StatusRequestTimeout               : return "Request Timeout"; // RFC 9110, 15.5.9
        case http::StatusConflict                     : return "Conflict"; // RFC 9110, 15.5.10
        case http::StatusGone                         : return "Gone"; // RFC 9110, 15.5.11
        case http::StatusLengthRequired               : return "Length Required"; // RFC 9110, 15.5.12
        case http::StatusPreconditionFailed           : return "Precondition Failed"; // RFC 9110, 15.5.13
        case http::StatusRequestEntityTooLarge        : return "Request Entity TooLarge"; // RFC 9110, 15.5.14
        case http::StatusRequestURITooLong            : return "Request URI TooLong"; // RFC 9110, 15.5.15
        case http::StatusUnsupportedMediaType         : return "Unsupported Media Type"; // RFC 9110, 15.5.16
        case http::StatusRequestedRangeNotSatisfiable : return "Requested Range Not Satisfiable"; // RFC 9110, 15.5.17
        case http::StatusExpectationFailed            : return "Expectation Failed"; // RFC 9110, 15.5.18
        case http::StatusTeapot                       : return "Teapot"; // RFC 9110, 15.5.19 (Unused)
        case http::StatusMisdirectedRequest           : return "Misdirected Request"; // RFC 9110, 15.5.20
        case http::StatusUnprocessableEntity          : return "Unprocessable Entity"; // RFC 9110, 15.5.21
        case http::StatusLocked                       : return "Locked"; // RFC 4918, 11.3
        case http::StatusFailedDependency             : return "Failed Dependency"; // RFC 4918, 11.4
        case http::StatusTooEarly                     : return "Too Early"; // RFC 8470, 5.2.
        case http::StatusUpgradeRequired              : return "Upgrade Required"; // RFC 9110, 15.5.22
        case http::StatusPreconditionRequired         : return "Precondition Required"; // RFC 6585, 3
        case http::StatusTooManyRequests              : return "Too Many Requests"; // RFC 6585, 4
        case http::StatusRequestHeaderFieldsTooLarge  : return "Request Header Fields TooLarge"; // RFC 6585, 5
        case http::StatusUnavailableForLegalReasons   : return "Unavailable For Legal Reasons"; // RFC 7725, 3

        // 500
        case http::StatusInternalServerError           : return "Internal Server Error"; // RFC 9110, 15.6.1
        case http::StatusNotImplemented                : return "Not Implemented"; // RFC 9110, 15.6.2
        case http::StatusBadGateway                    : return "Bad Gateway"; // RFC 9110, 15.6.3
        case http::StatusServiceUnavailable            : return "Service Unavailable"; // RFC 9110, 15.6.4
        case http::StatusGatewayTimeout                : return "Gateway Timeout"; // RFC 9110, 15.6.5
        case http::StatusHTTPVersionNotSupported       : return "HTTP Version Not Supported"; // RFC 9110, 15.6.6
        case http::StatusVariantAlsoNegotiates         : return "Variant Also Negotiates"; // RFC 2295, 8.1
        case http::StatusInsufficientStorage           : return "Insufficient Storage"; // RFC 4918, 11.5
        case http::StatusLoopDetected                  : return "Loop Detected"; // RFC 5842, 7.2
        case http::StatusNotExtended                   : return "Not Extended"; // RFC 2774, 7
        case http::StatusNetworkAuthenticationRequired : return "Network Authentication Required"; // RFC 6585, 6
        default: return "Unknown";
    }
}
