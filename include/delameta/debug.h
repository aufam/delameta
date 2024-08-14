#ifndef PROJECT_DELAMETA_DEBUG_H
#define PROJECT_DELAMETA_DEBUG_H

#include <string>

namespace Project::delameta {

    __attribute__((weak)) 
    void info(const char*, int, const std::string&);

    __attribute__((weak)) 
    void warning(const char*, int, const std::string&);

    __attribute__((weak)) 
    void panic(const char* file, int line, const std::string& msg);
}

#define FL __FILE__, __LINE__
#define DBG(fn, ...) fn(__FILE__, __LINE__, __VA_ARGS__)

#ifdef FMT_FORMAT_H_
#define DBG_VAL(fn, value) \
    [&]() -> decltype(auto) {\
        auto&& _value = value; \
        fn(__FILE__, __LINE__, fmt::format("{} = {}", #value, _value)); \
        return std::forward<decltype(_value)>(_value); \
    }()
#else

#define DBG_VAL(fn, value) \
    [&]() -> decltype(auto) {\
        auto&& _value = value; \
        fn(__FILE__, __LINE__, #value + std::string(" = ") + std::to_string(_value)); \
        return std::forward<decltype(_value)>(_value); \
    }()
#endif

#endif
