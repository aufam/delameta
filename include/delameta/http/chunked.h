#ifndef PROJECT_DELAMETA_HTTP_CHUNKED_H
#define PROJECT_DELAMETA_HTTP_CHUNKED_H

#include "delameta/stream.h"

namespace Project::delameta::http {
    Stream chunked_encode(Stream& s);
    Stream chunked_decode(Descriptor& s); // TODO
}

#endif
