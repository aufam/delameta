#ifndef PROJECT_DELAMETA_STREAM_H
#define PROJECT_DELAMETA_STREAM_H

#include "delameta/movable.h"
#include <vector>
#include <string>
#include <list>
#include <functional>
#include <etl/result.h>

namespace Project::delameta {
    
    class Stream : public Movable {
    public:
        Stream() = default;
        virtual ~Stream() = default;

        Stream(Stream&&) noexcept = default;
        Stream& operator=(Stream&&) noexcept = default;

        using Rule = std::function<std::string_view()>;
        std::list<Rule> rules = {};

        template <typename T>
        Stream& operator<<(T rule) {
            if constexpr (
                std::is_same_v<T, std::string_view> || 
                std::is_same_v<T, std::string> || 
                std::is_same_v<T, std::vector<uint8_t>>
            ) {
                rules.push_back([data=std::move(rule)]() mutable { 
                    return std::string_view{reinterpret_cast<const char*>(data.data()), data.size()}; 
                });
                return *this;
            } else if constexpr (std::is_same_v<T, const char*>) {
                rules.push_back([data=rule]() {
                    return std::string_view(data); 
                });
                return *this;
            } else {
                rules.push_back(Rule(std::move(rule)));
                return *this;
            }
        }

        Stream& operator<<(Stream& other) {
            rules.splice(rules.end(), std::move(other.rules));
            return *this;
        }

        Stream& operator<<(Stream&& other) {
            rules.splice(rules.end(), std::move(other.rules));
            return *this;
        }

        Stream& operator>>(std::function<void(std::string_view)> out) {
            while (!rules.empty()) {
                const auto buf = rules.front()();
                out(buf);
                rules.pop_front();
            }
            return *this;
        }
    };
}

#endif
