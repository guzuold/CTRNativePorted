// SPDX-License-Identifier: GPL-2.0-only
#include <iostream>
#include <unordered_map>
#include <string_view>
#include <stdexcept>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ostream_sink.h>
namespace Common::Log {
// Initialized completely before publication; no runtime insert/erase is allowed.
std::unordered_map<std::string_view,std::shared_ptr<spdlog::logger>> ALL_LOGGERS=[] {
    std::unordered_map<std::string_view,std::shared_ptr<spdlog::logger>> result;
    auto sink=std::make_shared<spdlog::sinks::ostream_sink_mt>(std::cerr,true);
    // Windows CRT text stderr converts LF to CRLF exactly once.
    sink->set_formatter(std::make_unique<spdlog::pattern_formatter>("%v",spdlog::pattern_time_type::local,"\n"));
    const char* classes[]={
#include "logging_classes.inc"
    };
    for(auto name:classes){
        auto logger=std::make_shared<spdlog::logger>(name,sink);
        logger->set_level(spdlog::level::trace);
        logger->flush_on(spdlog::level::trace);
        logger->set_error_handler([](const std::string& error){throw std::runtime_error(error);});
        if(!result.emplace(name,std::move(logger)).second) throw std::runtime_error("Duplicate graphics log class");
    }
    return result;
}();
}
