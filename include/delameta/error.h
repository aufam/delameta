#ifndef PROJECT_DELAMETA_ERROR_H
#define PROJECT_DELAMETA_ERROR_H

#include <string>
#include <etl/result.h>

namespace Project::delameta {

    class Error {
    public:
        enum Code {
            ConnectionClosed,
            TransferTimeout,
        };

        Error(Code code);
        Error(int code, std::string what);
        virtual ~Error() = default;
    
        int code;
        std::string what;
    };

    template <typename T>
    using Result = etl::Result<T, Error>;
}

#endif