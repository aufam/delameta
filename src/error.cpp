#include "delameta/error.h"
#include <cstring>

using namespace Project::delameta;

Error::Error(Code code) : code(code) {
    switch (code) {
        case Error::ConnectionClosed:     what = "Connection closed"; break;
        case Error::TransferTimeout:      what = "Transfer timeout"; break;
        default: what = "Unknown"; break;
    }
}

Error::Error(int code, std::string what) : code(code), what(std::move(what)) {}