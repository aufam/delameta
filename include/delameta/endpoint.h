#ifndef PROJECT_DELAMETA_ENDPOINT_H
#define PROJECT_DELAMETA_ENDPOINT_H

#include "delameta/stream.h"
#include "delameta/url.h"

namespace Project::delameta {
    
    class Endpoint : public Descriptor {
    public:
        Endpoint(Descriptor*);
        virtual ~Endpoint();

        Endpoint(Endpoint&&);
        Endpoint& operator=(Endpoint&&);

        static Result<Endpoint> Open(const char* file, int line, std::string_view uri);
        static Result<Endpoint> Open(std::string_view uri) { return Open("", 0, uri); }

        Result<std::vector<uint8_t>> read() override;
        Result<std::vector<uint8_t>> read_until(size_t n) override;
        Stream read_as_stream(size_t n) override;

        Result<void> write(std::string_view data) override;
        using Descriptor::write;

        Endpoint& operator<<(std::string_view data);
        Endpoint& operator<<(const std::vector<uint8_t>& data);
        Endpoint& operator<<(Stream& s);

        Endpoint& operator>>(std::string& out);
        Endpoint& operator>>(std::vector<uint8_t>& out);
        Endpoint& operator>>(Stream& s);

    protected:
        Descriptor* desc;
    };

    typedef Result<Endpoint>(*EndpointFactoryFunction)(const char* file, int line, const URL&);
}

#endif
