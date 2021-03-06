// Copyright 2021 lamp
//
// Created by lamp on 17.03.2021.
//

#ifndef INCLUDE_SERVER_COMMON_HPP_
#define INCLUDE_SERVER_COMMON_HPP_


#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>

#include <boost/beast/core.hpp>

#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>

#include <utility>
#include <string>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <iostream>
#include <shared_mutex>
#include <set>
#include <chrono>
#include <fstream>

void init(){
  boost::log::add_file_log(
      boost::log::keywords::file_name = "logs/server_%5N.log",
      boost::log::keywords::rotation_size = 10 * 1024 * 1024,
      boost::log::keywords::time_based_rotation =
boost::log::sinks::file::rotation_at_time_point(0, 0, 0),
      boost::log::keywords::format =
          "[%TimeStamp%][%ThreadID%][%Severity%]: %Message%");
  boost::log::add_console_log(
    std::cout, boost::log::keywords::format =
"[%TimeStamp%][%ThreadID%][%Severity%]: %Message%");
}

template<class Stream>
struct send_lambda {
  Stream& stream_;
  bool& close_;
  boost::beast::error_code& ec_;

  explicit send_lambda(Stream& stream, bool& close,
                       boost::beast::error_code& ec)
      : stream_(stream), close_(close), ec_(ec) {}

  template <bool isRequest, class Body, class Fields>
  void operator()(
      boost::beast::http::message<isRequest, Body, Fields>&& msg) const {
    close_ = msg.need_eof();

    boost::beast::http::serializer<isRequest, Body, Fields> sr{msg};
    boost::beast::http::write(stream_, sr, ec_);
  }
};

/*
 * Handler
 * - has an stl container which would contain suggestions
 * - suggestions would be in a little containers like: std::pair(?)
 * - updates every 15 min
 * - _mutex locks shared on reading and locks unique on writing
 */
typedef std::pair<size_t, std::string> suggestion; // pair<cost, suggestion>
// json_suggestion: {"suggestion": <string>, "cost": <size_t>
class suggest_handler {
 private:
  struct suggest_comparator {
    bool operator()(const suggestion& lhs, const suggestion& rhs) const {
      return lhs.first > rhs.first;
    }
  };

 public:
  explicit suggest_handler(const std::string& path = "data/suggestions.json")
      : _path{path} {
    start_updating();
  }
  suggest_handler(const suggest_handler&) = delete;
  suggest_handler& operator=(const suggest_handler&) = delete;
  suggest_handler(suggest_handler&&) = delete;
  suggest_handler& operator=(suggest_handler&&) = delete;

  ~suggest_handler() {
    _suggestions.clear();
    if (_updating_thread.joinable()) {
      _updating_thread.detach();
    }
  }

 private:
  void start_updating() {
    _updating_thread = std::thread([this] {
      for (;;) {
        update();
        std::this_thread::sleep_for(std::chrono::seconds(900));
      }
    });
  }
  void update() {
    std::ifstream suggestions(_path, std::ios::in);
    json suggestions_json;
    suggestions >> suggestions_json;
    if (suggestions_json.is_array() && !suggestions_json.empty()) {
      std::lock_guard<std::shared_mutex> lock(_mutex);
      auto last_size = _suggestions.size();
      for (auto&& sug : suggestions_json) {
        write(sug);
      }

      BOOST_LOG_TRIVIAL(debug)
          << "Collection updated. New size: " << _suggestions.size()
          << " size before: " << last_size;
    }
  }

  void write(const json& sug) {
    if (sug.contains("cost") && sug.contains("suggestion")) {
      _suggestions.emplace(std::make_pair(
          sug["cost"].get<int>(), sug["suggestion"].get<std::string>()));
    }
  }

 public:
  // read(substring to find) -> json
  /*
   * returns a json array which contains first 10 suggestions
   * with the subst
   */
  json read(const std::string& subst = "") {
    std::set<suggestion, suggest_comparator> out;
    std::shared_lock<std::shared_mutex> lock(_mutex);
    for (auto&& sug : _suggestions) {
      if (sug.second.find(subst) || subst.empty()) {
        out.emplace(sug);
      }
    }
    json out_json;
    size_t max_sug = 10;
    for (auto&& sug : out) {
      if (max_sug) {
        json s{{"cost", sug.first}, {"suggestion", sug.second}};
        out_json.emplace_back(s);
        max_sug--;
      } else {
        break;
      }
    }
    return out_json;
  }

 private:
  const std::string _path;
  std::shared_mutex _mutex;
  // Container: set<pair>(?) vector(?)
  std::set<suggestion, suggest_comparator> _suggestions;

  std::thread _updating_thread;
};

#endif  // INCLUDE_SERVER_COMMON_HPP_
