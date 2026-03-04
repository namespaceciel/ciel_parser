#pragma once

#define QUILL_DISABLE_NON_PREFIXED_MACROS

#include <quill/Backend.h>
#include <quill/Frontend.h>
#include <quill/LogMacros.h>
#include <quill/Logger.h>
#include <quill/sinks/ConsoleSink.h>
#include <quill/sinks/FileSink.h>

namespace cielparser {

inline quill::Logger* g_quill_logger{};

inline void SetupQuill(const std::filesystem::path& log_path) {
  const quill::BackendOptions backend_options{.check_printable_char = nullptr};
  quill::Backend::start(backend_options);
  quill::PatternFormatterOptions formatter_options;
  formatter_options.format_pattern = "%(time) %(short_source_location:<16) %(log_level:<9) %(message)";
  formatter_options.timestamp_pattern = "%Y-%m-%d %H:%M:%S.%Qns";
  formatter_options.add_metadata_to_multi_line_logs = false;
  auto console_sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1");
  auto file_sink = quill::Frontend::create_or_get_sink<quill::FileSink>(log_path);
  g_quill_logger = quill::Frontend::create_or_get_logger("root", {console_sink, file_sink}, formatter_options);
}

}  // namespace cielparser

#define LOG_INFO(fmt, ...) QUILL_LOG_INFO(cielparser::g_quill_logger, fmt, ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) QUILL_LOG_WARNING(cielparser::g_quill_logger, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) QUILL_LOG_ERROR(cielparser::g_quill_logger, fmt, ##__VA_ARGS__)
