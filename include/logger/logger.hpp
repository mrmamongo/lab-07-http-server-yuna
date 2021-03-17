//
// Created by lamp on 27.02.2021.
//

/*
 * This little logger is made to simplify the process
 * of logging for the /bmstu-iu8-cpp-sem-3/ - labs
 *
 * Copyright 2020 Lamp
 */

#ifndef LOGGER_LOGGER_HPP
#define LOGGER_LOGGER_HPP

#include <boost/thread.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <thread>
#include <mutex>
#include <future>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace logger{
    template<typename T>
    class tsqueue {
    public:
        tsqueue() = default;
        tsqueue(const tsqueue<T>&) = delete;
        virtual ~tsqueue() { clear(); }

    public:
        // Returns and maintains item at front of Queue
        const T& front()
        {
            std::scoped_lock lock(muxQueue);
            return deqQueue.front();
        }

        // Returns and maintains item at back of Queue
        const T& back()
        {
            std::scoped_lock lock(muxQueue);
            return deqQueue.back();
        }

        // Removes and returns item from front of Queue
        T pop_front()
        {
            std::scoped_lock lock(muxQueue);
            auto t = std::move(deqQueue.front());
            deqQueue.pop_front();
            return t;
        }

        // Removes and returns item from back of Queue
        T pop_back()
        {
            std::scoped_lock lock(muxQueue);
            auto t = std::move(deqQueue.back());
            deqQueue.pop_back();
            return t;
        }

        // Adds an item to back of Queue
        void push_back(const T& item)
        {
            std::scoped_lock lock(muxQueue);
            deqQueue.emplace_back(std::move(item));

            std::unique_lock<std::mutex> ul(muxBlocking);
            cvBlocking.notify_one();
        }

        // Adds an item to front of Queue
        void push_front(const T& item)
        {
            std::scoped_lock lock(muxQueue);
            deqQueue.emplace_front(std::move(item));

            std::unique_lock<std::mutex> ul(muxBlocking);
            cvBlocking.notify_one();
        }

        // Returns true if Queue has no items
        bool empty()
        {
            std::scoped_lock lock(muxQueue);
            return deqQueue.empty();
        }

        // Returns number of items in Queue
        size_t count()
        {
            std::scoped_lock lock(muxQueue);
            return deqQueue.size();
        }

        // Clears Queue
        void clear()
        {
            std::scoped_lock lock(muxQueue);
            deqQueue.clear();
        }

        void wait()
        {
            while (empty())
            {
                std::unique_lock<std::mutex> ul(muxBlocking);
                cvBlocking.wait(ul);
            }
        }

    protected:
        std::mutex muxQueue;
        std::deque<T> deqQueue;
        std::condition_variable cvBlocking;
        std::mutex muxBlocking;
    };
}

namespace logger{
    namespace logging = boost::log;
    namespace src = boost::log::sources;
    namespace sinks = boost::log::sinks;
    namespace keywords = boost::log::keywords;
    namespace attrs = boost::log::attributes;
    typedef src::severity_logger<logging::trivial::severity_level> logger_t;

    typedef logging::trivial::severity_level log_level_t;

    class logger {
    public:
        struct message {
            log_level_t severity;
            std::string msg;

            logger& parent_;

            message(logger& parent):parent_(parent){}

            template<typename T>
            friend message operator<<(message m, T t) {
              std::stringstream ss;
              ss << t;
              if (ss.str() == "karoche"){
                m.parent_.log(m);
              } else {
                m.msg += ss.str();
              }
                return m;
            }
        };

    public:
        logger &operator=(const logger &) = delete;

        logger(const logger &) = delete;

        /*
         *  @params
         *  log_name - default = "log_" - name of the log files. Final logs: log/<log_name>00000.log
         *  format - default = "[%TimeStamp%][%Severity%][%TID%]: %Message%" - regular expression to personalise the log
         */
        explicit logger(const std::string &log_name = "log_",
                        const std::string& format  = "[%TimeStamp%][%Severity%][%TID%]: %Message%"
                                ) {
            initiate(log_name, format);
        }

        virtual ~logger() {
            _in_messages.clear();
        };

    public:
        message set_severity(log_level_t severity) {
            message msg(*this);
            msg.severity = severity;
            return msg;
        }

        void log(const message &value_to_log) {
            _in_messages.push_back(std::async(
                    [this, value_to_log] {
                        _mutex.lock();
                        std::cout << "\n[LOG]"
                                  << "[" << to_string(value_to_log.severity) << "]"
                                  << "[" << boost::this_thread::get_id() << "]: "
                                  << value_to_log.msg << "\n";
                        _mutex.unlock();
                        BOOST_LOG_SEV(_logger, value_to_log.severity) << value_to_log.msg;
                        return true;
                    }).get());
        }

    private:
        static std::string to_string(const log_level_t &level) {
            switch (level) {
                case log_level_t::info: {
                    return "info";
                }
                case log_level_t::trace: {
                    return "trace";
                }
                case log_level_t::debug: {
                    return "debug";
                }
                case log_level_t::warning: {
                    return "warning";
                }
                case log_level_t::error: {
                    return "error";
                }
                case log_level_t::fatal: {
                    return "fatal";
                }
            }
            return std::string();
        }

        static void add_common_attributes() {
            auto core = logging::core::get();
            core->add_global_attribute("TID", attrs::current_thread_id());
            core->add_global_attribute("TimeStamp", attrs::local_clock());
        }

        static void initiate(const std::string &log_name, const std::string& format) {
            add_common_attributes();
            logging::add_file_log(
                    keywords::file_name = "logs/" + log_name + "%5N.log",
                    keywords::rotation_size = 10 * 1024 * 1024,
                    keywords::time_based_rotation =
                            sinks::file::rotation_at_time_point(0, 0, 0),
                    keywords::format = format);
        }

        logger_t _logger;
        std::mutex _mutex;
        tsqueue<bool> _in_messages;
    };
}


/*
 * Macros to use the logger are matching to the boost used ones
 */
//
#define l_info logger::log_level_t::info
#define l_warning logger::log_level_t::warning
#define l_error logger::log_level_t::error
#define l_trace logger::log_level_t::trace
#define l_debug logger::log_level_t::debug
#define l_fatal logger::log_level_t::fatal

/*
 * The main macro to log data. Ending symbol is "karoche"
 */
#define LOG(log, severity) ( \
log.set_severity(severity) \
)
#define karoche "karoche"
#endif  // LOGGER_LOGGER_HPP
