#ifndef PROJECT_DELAMETA_HTTP_REQUEST_H
#define PROJECT_DELAMETA_HTTP_REQUEST_H

#include <string>
#include <unordered_map>
#include "delameta/stream.h"
#include "delameta/url.h"

namespace Project::delameta::http {

    struct RequestWriter {
        Stream dump();

        std::string method;
        URL url;
        std::string version;
        std::unordered_map<std::string, std::string> headers = {};
        mutable std::string body = {};
        mutable Stream body_stream = {};
    };

    struct RequestReader {
        RequestReader(Descriptor& desc, const std::vector<uint8_t>& data);
        RequestReader(Descriptor& desc, std::vector<uint8_t>&& data);
        operator RequestWriter() const;

        std::string_view method;
        URL url;
        std::string_view version;
        std::unordered_map<std::string_view, std::string_view> headers = {};
        mutable std::string body = {};
        mutable Stream body_stream = {};
    
    private:
        std::vector<uint8_t> data;
        void parse(Descriptor& desc, const std::vector<uint8_t>& data);
    };
}

#endif