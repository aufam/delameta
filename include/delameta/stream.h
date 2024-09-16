#ifndef PROJECT_DELAMETA_STREAM_H
#define PROJECT_DELAMETA_STREAM_H

#include "delameta/movable.h"
#include "delameta/error.h"
#include <vector>
#include <list>
#include <functional>
#include <etl/ref.h>

namespace Project::delameta {
    
    class Stream;
    
    class Descriptor : public Movable {
    public:
        Descriptor() = default;
        virtual ~Descriptor() = default;

        Descriptor(Descriptor&&) noexcept = default;
        Descriptor& operator=(Descriptor&&) noexcept = default;

        virtual Result<std::vector<uint8_t>> read() = 0;
        virtual Result<std::vector<uint8_t>> read_until(size_t n) = 0;
        virtual Stream read_as_stream(size_t n) = 0;

        virtual Result<void> write(std::string_view data) = 0;

        Result<void> write(const std::vector<uint8_t>& data) {
            return write(std::string_view{reinterpret_cast<const char*>(data.data()), data.size()});
        }

        Result<void> write(const std::string& data) {
            return write(std::string_view{data.data(), data.size()});
        }

        Result<void> write(const char* data) {
            return write(std::string_view(data));
        }
    };

    class Stream : public Movable {
    public:
        Stream() = default;
        virtual ~Stream();

        Stream(Stream&&) noexcept = default;
        Stream& operator=(Stream&&) noexcept = default;

        using Rule = std::function<std::string_view(Stream&)>;
        std::list<Rule> rules = {};
        std::function<void()> at_destructor;
        bool again = false;

        Stream& operator<<(std::string_view data);
        Stream& operator<<(const char* data);
        Stream& operator<<(std::string data);
        Stream& operator<<(std::vector<uint8_t> data);

        Stream& operator<<(Stream& other);
        Stream& operator<<(Stream&& other);
        Stream& operator>>(Stream& other);

        Stream& operator<<(Rule in);
        Stream& operator<<(std::function<std::string_view()> out);
        Stream& operator>>(std::function<void(std::string_view)> out);

        Stream& operator<<(Descriptor& des);
        Stream& operator>>(Descriptor& des);

        Result<void> out_with_prefix(Descriptor& des, std::function<Result<void>(std::string_view)> prefix);
        std::vector<uint8_t> pop_once();
    };

    class StreamSessionServer : public Movable {
    public:
        using StreamSessionHandler = std::function<Stream(Descriptor&, const std::string&, const std::vector<uint8_t>&)>;

        StreamSessionServer() = default;
        StreamSessionServer(StreamSessionHandler handler);
        virtual ~StreamSessionServer() = default;

        StreamSessionServer(StreamSessionServer&&) noexcept = default;
        StreamSessionServer& operator=(StreamSessionServer&&) noexcept = default;

        Stream execute_stream_session(Descriptor& desc, const std::string& name, const std::vector<uint8_t>& data);
        StreamSessionHandler handler;
    };

    class StreamSessionClient : public Movable {
    public:
        StreamSessionClient(Descriptor* desc);
        StreamSessionClient(Descriptor& desc);

        virtual ~StreamSessionClient();

        StreamSessionClient(StreamSessionClient&& other);
        StreamSessionClient& operator=(StreamSessionClient&& other) = delete;

        Result<std::vector<uint8_t>> request(Stream& in_stream);
        Descriptor* desc;
        bool is_dyn;
    };

    template <typename T>
    class Server {};
}

#ifdef FMT_FORMAT_H_

template <> 
struct fmt::formatter<Project::delameta::Stream> {
    constexpr auto parse(fmt::format_parse_context& ctx) { return ctx.end(); }

    template <typename Ctx>
    inline auto format(const Project::delameta::Stream& m, Ctx& ctx) const {
        return fmt::format_to(ctx.out(), "Stream with {} rule(s)", m.rules.size());
    }
};

#endif

#endif
