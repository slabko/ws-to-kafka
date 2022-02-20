#include <algorithm>
#include <string>

#ifndef URI_H_INCLUDED
#define URI_H_INCLUDED

namespace slabko::wskafka {

struct Uri {
public:
  std::string QueryString, Path, Protocol, Host, Port;

  static Uri Parse(const std::string &uri) {
    Uri result;

    using iterator_t = std::string::const_iterator;

    if (uri.length() == 0) {
      return result;
    }

    iterator_t uri_end = uri.end();

    // get query start
    iterator_t query_start = std::find(uri.begin(), uri_end, '?');

    // protocol
    iterator_t protocol_start = uri.begin();
    iterator_t protocol_end = std::find(protocol_start, uri_end, ':'); //"://");

    if (protocol_end != uri_end) {
      std::string prot = &*(protocol_end);
      if ((prot.length() > 3) && (prot.substr(0, 3) == "://")) {
        result.Protocol = std::string(protocol_start, protocol_end);
        protocol_end += 3; //      ://
      } else {
        protocol_end = uri.begin(); // no protocol
      }
    } else {
      protocol_end = uri.begin(); // no protocol
    }

    // host
    iterator_t host_start = protocol_end;
    iterator_t path_start =
        std::find(host_start, uri_end, '/'); // get pathStart

    iterator_t host_end = std::find(
        protocol_end, (path_start != uri_end) ? path_start : query_start,
        ':'); // check for port

    result.Host = std::string(host_start, host_end);

    // port
    if ((host_end != uri_end) && ((&*(host_end))[0] == ':')) // we have a port
    {
      host_end++;
      iterator_t port_end = (path_start != uri_end) ? path_start : query_start;
      result.Port = std::string(host_end, port_end);
    }

    // path
    if (path_start != uri_end) {
      result.Path = std::string(path_start, query_start);
    }

    // query
    if (query_start != uri_end) {
      result.QueryString = std::string(query_start, uri.end());
    }

    if (result.Port.empty()) {
      if (result.Protocol == "https" || result.Protocol == "wss") {
        result.Port = "443";
      }
      if (result.Protocol == "http" || result.Protocol == "ws") {
        result.Port = "80";
      }
    }
    return result;
  }
};

} // namespace slabko::wskafka
#endif
