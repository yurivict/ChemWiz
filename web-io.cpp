// Based on:
// * HTTP example:  https://www.boost.org/doc/libs/1_66_0/libs/beast/example/http/client/sync/http_client_sync.cpp
// * HTTPS example: https://github.com/boostorg/beast/blob/develop/example/http/client/sync-ssl/http_client_sync_ssl.cpp

#include "web-io.h"
#include "xerror.h"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/experimental/core/ssl_stream.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/algorithm/string.hpp>

#include <string>
#include <cstdlib>
#include <sstream>
#include <iostream>
#include <memory>
#include <regex>

namespace WebIo {

static const int version = 11; // can also be 10

namespace beast = boost::beast; // from <boost/beast.hpp>
namespace net = boost::asio;    // from <boost/asio.hpp>
namespace http = beast::http;   // from <boost/beast/http.hpp>
using tcp = net::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


namespace Plain {

static Binary* download(const std::string &host, const std::string &service, const std::string &target) {
  std::unique_ptr<Binary> output(new Binary);

  // The io_context is required for all I/O
  net::io_context ioc;

  // These objects perform our I/O
  tcp::resolver resolver{ioc};
  tcp::socket socket{ioc};

  // Look up the domain name
  auto const results = resolver.resolve(host, service);

  // Make the connection on the IP address we get from a lookup
  net::connect(socket, results.begin(), results.end());

  // Set up an HTTP GET request message
  http::request<http::string_body> req{http::verb::get, target, version};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  // Send the HTTP request to the remote host
  http::write(socket, req);

  // This buffer is used for reading and must be persisted
  beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::vector_body<Binary::value_type>> res;

  // Receive the HTTP response
  http::read(socket, buffer, res);

  // Write message into the output
  output->swap(res.body());

  // Gracefully close the socket
  boost::system::error_code ec;
  socket.shutdown(tcp::socket::shutdown_both, ec);

  // not_connected happens sometimes so don't bother reporting it.
  //
  if (ec && ec != boost::system::errc::not_connected)
    throw boost::system::system_error{ec};

  // If we get here then the connection is closed gracefully

  return output.release();
}

} // Plain

namespace Ssl {

namespace ssl = net::ssl;       // from <boost/asio/ssl.hpp>

static Binary* download(const std::string &host, const std::string &service, const std::string &target) {
  std::unique_ptr<Binary> output(new Binary);

  // The io_context is required for all I/O
  net::io_context ioc;

  // The SSL context is required, and holds certificates
  ssl::context ctx{ssl::context::sslv23_client};

  // This holds the root certificate used for verification
  ctx.set_default_verify_paths(); // standard OpenSSL certificates work
  //load_root_certificates(ctx); // recommended in the example - doesn't work, perhaps the expired certificate?

  // Verify the remote server's certificate
  ctx.set_verify_mode(ssl::verify_peer);

  // These objects perform our I/O
  tcp::resolver resolver{ioc};
  beast::ssl_stream<tcp::socket> stream{ioc, ctx};

  // Set SNI Hostname (many hosts need this to handshake successfully)
  if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) {
    beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
    throw beast::system_error{ec};
  }

  // Look up the domain name
  auto const results = resolver.resolve(host, service);

  // Make the connection on the IP address we get from a lookup
  net::connect(stream.next_layer(), results.begin(), results.end());

  // Perform the SSL handshake
  stream.handshake(ssl::stream_base::client);

  // Set up an HTTP GET request message
  http::request<http::string_body> req{http::verb::get, target, version};
  req.set(http::field::host, host);
  req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

  // Send the HTTP request to the remote host
  http::write(stream, req);

  // This buffer is used for reading and must be persisted
  beast::flat_buffer buffer;

  // Declare a container to hold the response
  http::response<http::vector_body<Binary::value_type>> res;

  // Receive the HTTP response
  http::read(stream, buffer, res);

  // Write message into the output
  //output->insert(output->end(), (uint8_t*)&res.body(), (uint8_t*)((&res.body() + *res.payload_size())));
  output->swap(res.body());

  // Gracefully close the stream
  beast::error_code ec;
  stream.shutdown(ec);
  if (ec == net::error::eof) {
    // Rationale:
    // http://stackoverflow.com/questions/25587403/boost-asio-ssl-async-shutdown-always-finishes-with-an-error
    ec = {};
  }
  if (ec)
    throw beast::system_error{ec};

  // If we get here then the connection is closed gracefully

  return output.release();
}

} // Ssl

//
// iface
//

Binary* download(const std::string &host, const std::string &service, const std::string &target) {
  try {
    if (service != "443" && service != "https")
      return Plain::download(host, service, target);
    else
      return Ssl::download(host, service, target);
  } catch (std::exception const& e) {
    ERROR(e.what() << ": URL download request failed for host=" << host << " service=" << service << " target=" << target)
  }
}


// parser based on: https://github.com/boostorg/beast/issues/787#issuecomment-376259849
struct ParsedURI {
  std::string protocol;
  std::string domain;  // only domain must be present
  std::string port;
  std::string resource;
  std::string query;   // everything after '?', possibly nothing
};

static ParsedURI parseURI(const std::string& url) {
  ParsedURI result;
  auto value_or = [](const std::string& value, std::string&& deflt) -> std::string {
    return (value.empty() ? deflt : value);
  };
  // Note: only "http", "https", "ws", and "wss" protocols are supported
  static const std::regex PARSE_URL{ R"((([httpsw]{2,5})://)?([^/ :]+)(:(\d+))?(/([^ ?]+)?)?/?\??([^/ ]+\=[^/ ]+)?)",
                                     std::regex_constants::ECMAScript | std::regex_constants::icase };
  std::smatch match;
  if (std::regex_match(url, match, PARSE_URL) && match.size() == 9) {
    result.protocol = value_or(boost::algorithm::to_lower_copy(std::string(match[2])), "http");
    result.domain   = match[3];
    const bool is_sequre_protocol = (result.protocol == "https" || result.protocol == "wss");
    result.port     = value_or(match[5], (is_sequre_protocol)? "443" : "80");
    result.resource = value_or(match[6], "/");
    result.query = match[8];
    assert(!result.domain.empty());
  }
  return result;
}

Binary* downloadUrl(const std::string &url) {
  // parse URL into 3 parts to supply to the above 'download' function
  auto p = parseURI(url);
  return download(p.domain, p.protocol, p.resource+p.query); // XXX inaccurate when port is explicitly specified
}

} // WebIo
