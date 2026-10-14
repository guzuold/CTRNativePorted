// SPDX-License-Identifier: GPL-2.0-or-later
// Plain HTTP/1.1 exchange with the project server (R8): the transport under the runtime's
// libSceHttp and libSceNpWebApi. The server listens on CTR_ONLINE_SERVER, port
// CTR_ONLINE_HTTP_PORT (default 8780; CTR_ONLINE_WEBAPI_PORT is accepted as an alias).
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ctr_hle::online {
struct HttpResponse {
    int status = 0;
    std::string header_block;                    // header lines after the status line
    std::map<std::string, std::string> headers;  // lower-case names
    std::string body;
};
// Sends "<method> <target>" with the given headers and body; Connection: close. Adds
// X-Ctr-Online-Id. Returns false when the server cannot be reached or does not answer HTTP.
bool http_exchange(const std::string& method, const std::string& target,
                   const std::vector<std::pair<std::string, std::string>>& headers,
                   const std::string& body, HttpResponse* response);
std::string lower(std::string text);
std::string json_escape(const std::string& text);
} // namespace ctr_hle::online
