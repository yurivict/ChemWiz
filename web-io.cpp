// based on the HTTP example: https://www.boost.org/doc/libs/1_66_0/libs/beast/example/http/client/sync/http_client_sync.cpp
// HTTPS example is here: https://github.com/boostorg/beast/blob/develop/example/http/client/sync-ssl/http_client_sync_ssl.cpp

#include "web-io.h"
#include "xerror.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <string>
#include <cstdlib>
#include <sstream>
#include <iostream>

using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
namespace http = boost::beast::http;    // from <boost/beast/http.hpp>

namespace WebIo {

//static const int version = 11; // can also be 10
static const int version = 10; // can also be 10

std::string download(const std::string &host, const std::string &service, const std::string &target) {
  std::ostringstream content;

  try {
    // The io_context is required for all I/O
    boost::asio::io_context ioc;

    // These objects perform our I/O
    tcp::resolver resolver{ioc};
    tcp::socket socket{ioc};

    // Look up the domain name
    auto const results = resolver.resolve(host, service);

    // Make the connection on the IP address we get from a lookup
    boost::asio::connect(socket, results.begin(), results.end());

    // Set up an HTTP GET request message
    http::request<http::string_body> req{http::verb::get, target, version};
    req.set(http::field::host, host);
    req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

    // Send the HTTP request to the remote host
    http::write(socket, req);

    // This buffer is used for reading and must be persisted
    boost::beast::flat_buffer buffer;

    // Declare a container to hold the response
    http::response<http::dynamic_body> res;

    // Receive the HTTP response
    http::read(socket, buffer, res);

    // Write message into the output
    content << res;

    // Gracefully close the socket
    boost::system::error_code ec;
    socket.shutdown(tcp::socket::shutdown_both, ec);

    // not_connected happens sometimes
    // so don't bother reporting it.
    //
    if(ec && ec != boost::system::errc::not_connected)
        throw boost::system::system_error{ec};

    // If we get here then the connection is closed gracefully
  } catch (std::exception const& e) {
    ERROR(e.what() << ": URL download request failed for host=" << host << " service=" << service << " target=" << target)
  }

  return content.str();
}

} // WebIo
