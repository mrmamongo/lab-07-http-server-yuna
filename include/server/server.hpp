// Copyright 2021 lamp
//
// Created by lamp on 17.03.2021.
//

#ifndef INCLUDE_SERVER_SERVER_HPP_
#define INCLUDE_SERVER_SERVER_HPP_

#include <common.hpp>

class server {
 public:
  server(const std::string& host, size_t port)
      : _context{1},
        _acceptor{_context,
                  {boost::asio::ip::address::from_string(host),
                   static_cast<uint16_t>((port))}},
        _handler() {}
  server(const std::string& host, size_t port,
         const std::string& suggestions_path)
      : _context{1},
        _acceptor{_context,
                  {boost::asio::ip::address::from_string(host),
                   static_cast<uint16_t>((port))}},
        _handler(suggestions_path) {}
  ~server() {}

 public:
  void start() {
    try {
      BOOST_LOG_TRIVIAL(debug)
          << "Starting server at "
          << _acceptor.local_endpoint().address().to_string() << ":"
          << _acceptor.local_endpoint().port();
      for (size_t counter = 1000;; ++counter) {
        boost::asio::ip::tcp::socket socket{_context};

        _acceptor.accept(socket);
        BOOST_LOG_TRIVIAL(debug)
            << "[" << counter << "] Connection established";
        auto ans = std::async(std::launch::async,
                              [&socket, this] { return do_session(socket); });
        if (ans.get()) {
          BOOST_LOG_TRIVIAL(info)
              << "[" << counter << "] Message responded. Status OK";
        } else {
          BOOST_LOG_TRIVIAL(info)
              << "[" << counter << "] Message responded. Status FAIL";
        }
      }
    } catch (std::exception& e) {
      BOOST_LOG_TRIVIAL(fatal) << "An error occurred: " << e.what();
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
      BOOST_LOG_TRIVIAL(debug) << "Illegal request: " << req.target();
      return send(send_request(req, ("Illegal request"), bad_request));
    }

    boost::beast::error_code ec;

    // Handle an unknown error
    if (ec) {
      BOOST_LOG_TRIVIAL(error) << "An unknown error: " << ec.message();
      return send(send_request(req, ec.message(), server_error));
    }

    if (req.method() == boost::beast::http::verb::post) {
      try {
        const auto body = json::parse(req.body());
        BOOST_LOG_TRIVIAL(debug) << "Body read: " << body.dump();
        return send(send_request(req, generate_message(body), message));
      } catch (std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << "Body reading error: " << e.what();
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
        BOOST_LOG_TRIVIAL(error) << "Reading error: " << ec.message();
        good = false;
      }

      handle_request(std::move(req), lambda);
      if (ec) {
        BOOST_LOG_TRIVIAL(error) << "Writing error: " << ec.message();
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
  std::string generate_message(const json& msg) {
    if (msg.contains("input")) {
      BOOST_LOG_TRIVIAL(debug) << "Message: " << msg.dump();
      return json{
          {"suggestions", _handler.read(msg["input"].get<std::string>())}}
          .dump();
    } else {
      return std::string{R"({"suggestions":[]})"};
    }
  }

 private:
  boost::asio::io_context _context;
  boost::asio::ip::tcp::acceptor _acceptor;
  suggest_handler _handler;
};

#endif  // INCLUDE_SERVER_SERVER_HPP_
