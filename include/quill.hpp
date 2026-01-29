#pragma once

#define QUILL_DISABLE_NON_PREFIXED_MACROS

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>

namespace cielparser {

inline quill::Logger* const g_quill_logger = []() {
  quill::Backend::start();
  quill::Logger* res = quill::Frontend::create_or_get_logger(
      "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"),
      quill::PatternFormatterOptions{"%(time) [%(thread_id)] %(short_source_location:<20) %(log_level:<9) %(message)",
                                     "%H:%M:%S.%Qns", quill::Timezone::GmtTime});
  res->set_log_level(quill::LogLevel::TraceL3);
  return res;
}();

}  // namespace cielparser

#define LOG_INFO(fmt, ...) QUILL_LOG_INFO(cielparser::g_quill_logger, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) QUILL_LOG_WARNING(cielparser::g_quill_logger, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(cielparser::g_quill_logger, fmt, ##__VA_ARGS__)
