#ifndef PROJECT_DELAMETA_HTTP_ERROR_H
#define PROJECT_DELAMETA_HTTP_ERROR_H

#include "delameta/error.h"

namespace Project::delameta::http {

    struct Error {
        int status;
        std::string what;

        Error(int status);
        Error(int status, std::string what);
        Error(delameta::Error);

        operator delameta::Error() const& { return {status, what}; }
        operator delameta::Error() && { return {status, std::move(what)}; }
    };

    template <typename T>
    using Result = etl::Result<T, Error>;

    template <typename T> struct is_http_result : std::false_type {};
    template <typename T, typename E> struct is_http_result<etl::Result<T, E>> : std::is_convertible<E, Error> {};
    template <typename T> static constexpr bool is_server_result_v = is_http_result<T>::value;
}

#endif