//
// Created by lamp on 17.03.2021.
//

#ifndef HTTP_SERVER_SERVER_HPP
#define HTTP_SERVER_SERVER_HPP

#include <common.hpp>

class server {
 public:
  server(const std::string& host, size_t port)
      : _context{1},
        _acceptor{_context,
                  {boost::asio::ip::address::from_string(host),
                   static_cast<unsigned short>((port))}}, _logger("server"), _handler(_logger) {}
  server(const std::string& host, size_t port, const std::string& suggestions_path)
      : _context{1},
        _acceptor{_context,
                  {boost::asio::ip::address::from_string(host),
                   static_cast<unsigned short>((port))}}, _logger("server"), _handler(_logger, suggestions_path) {}
  ~server(){}

 public:
  void start() {
    try {
      LOG(_logger, l_debug)
          << "Starting server at "
          << _acceptor.local_endpoint().address().to_string() << ":"
          << _acceptor.local_endpoint().port() << karoche;
      for (size_t counter = 1000;; ++counter) {
        boost::asio::ip::tcp::socket socket{_context};

        _acceptor.accept(socket);
        LOG(_logger, l_info) << "[" << counter << "] Connection established" << karoche;
        auto ans = std::async(std::launch::async,
                              [&socket, this] { return do_session(socket); });
        if (ans.get()) {
          LOG(_logger, l_info)
              << "[" << counter << "] Message responded. Status OK" << karoche;
        } else {
          LOG(_logger, l_info)
              << "[" << counter << "] Message responded. Status FAIL" << karoche;
        }
      }
    } catch (std::exception& e) {
      LOG(_logger, l_fatal) << "An error occurred: " << e.what() << karoche;
    }
  }

 public:
  enum ans_type { message, bad_request, server_error };
  template <class Body, class Allocator>
  auto send_request(boost::beast::http::request<
                        Body, boost::beast::http::basic_fields<Allocator>>& req,
                    boost::beast::string_view msg, ans_type type) {
    boost::beast::http::status response_type;
    if (type == message) {
      response_type = boost::beast::http::status::accepted;
    } else if (type == bad_request) {
      response_type = boost::beast::http::status::bad_request;
    } else {
      response_type = boost::beast::http::status::internal_server_error;
    }
    boost::beast::http::response<boost::beast::http::string_body> res{
        response_type, req.version()};

    res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);

    if (type == message) {
      res.set(boost::beast::http::field::content_type, "application/json");
    } else {
      res.set(boost::beast::http::field::content_type, "text/html");
    }

    res.keep_alive(req.keep_alive());

    if (type == message) {
      res.body() = std::string(R"({"message":")" + msg.to_string() + "\"}");
    } else {
      res.body() = std::string("An error occurred: '" + msg.to_string() + "'");
    }

    res.prepare_payload();
    return res;
  }

  template <class Body, class Allocator, class Send>
  void handle_request(
      boost::beast::http::request<
          Body, boost::beast::http::basic_fields<Allocator>>&& req,
      Send&& send) {

    // Request path must be absolute and not contain "..".
    if (req.target() != "/v1/app/suggest") {
      LOG(_logger, l_error) << "Illegal request: " << req.target() << karoche;
      return send(send_request(req, ("Illegal request"), bad_request));
    }

    boost::beast::error_code ec;

    // Handle an unknown error
    if (ec) {
      LOG(_logger, l_error) << "An unknown error: " << ec.message() << karoche;
      return send(send_request(req, ec.message(), server_error));
    }

    if (req.method() == boost::beast::http::verb::post) {
      try {
        const auto body = json::parse(req.body());
        LOG(_logger, l_debug) << "Body read: " << body.dump() << karoche;
        return send(send_request(req, generate_message(body), message));
      } catch (std::exception& e) {
        LOG(_logger, l_error) << "Body reading error: " << e.what() << karoche;
        return send(send_request(req, ("Wrong body type"), bad_request));
      }
    } else {
      return send(send_request(req, ("Unknown HTTP-method"), bad_request));
    }
  }

  bool do_session(boost::asio::ip::tcp::socket& socket) {
    bool close = false, good = true;
    boost::beast::error_code ec;
    boost::beast::flat_buffer buffer;
    send_lambda<boost::asio::ip::tcp::socket> lambda{socket, close, ec};

    for (;;) {
      boost::beast::http::request<boost::beast::http::string_body> req;
      boost::beast::http::read(socket, buffer, req, ec);
      if (ec == boost::beast::http::error::end_of_stream) break;
      if (ec) {
        LOG(_logger, l_error) << "Reading error: " << ec.message() << karoche;
        good = false;
      }

      handle_request(std::move(req), lambda);
      if (ec) {
        LOG(_logger, l_error) << "Writing error: " << ec.message() << karoche;
        good = false;
      }
      if (close) {
        break;
      }
    }
    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    return good;
  }

 private:
  std::string generate_message(const json& msg){
    if (msg.contains("input")) {
      LOG(_logger, l_debug) << "Message: " << msg.dump() << karoche;
      return json{{"suggestions", _handler.read(msg["input"].get<std::string>())}}.dump();
    } else {
      return std::string{R"({"suggestions":[]})"};
    }
  }

 private:
  boost::asio::io_context _context;
  boost::asio::ip::tcp::acceptor _acceptor;
  logger::logger _logger;

  suggest_handler _handler;
};

#endif  // HTTP_SERVER_SERVER_HPP
