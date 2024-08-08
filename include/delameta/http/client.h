#ifndef PROJECT_HTTP_CLIENT_H
#define PROJECT_HTTP_CLIENT_H

#include "delameta/http/request.h"
#include "delameta/http/response.h"

namespace Project::delameta::http {

    Result<ResponseReader> request(StreamSessionClient& session, RequestWriter req);
}

#endif