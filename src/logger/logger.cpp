#include "logger.h"

#include "logger/ansii-remove-formatter.h"
#include "logger/logger-types.h"
#include "logger/log-utils.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <ranges>
#include <regex>

namespace remote_scan
{
   Logger::Logger()
   {
      std::vector<spdlog::sink_ptr> sinks;
      sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

      // If the log path is defined create a rotating file logger
      if (const auto* logPath = std::getenv("LOG_PATH");
          logPath != nullptr)
      {
         std::string logPathFilename = logPath;
         logPathFilename.append("/remote-scan.log");

         constexpr size_t max_size{1048576 * 5};
         constexpr size_t max_files{5};
         auto& fileSink{sinks.emplace_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logPathFilename, max_size, max_files))};
         fileSink->set_formatter(std::make_unique<AnsiiRemoveFormatter>());
      }

      logger_ = std::make_shared<spdlog::logger>("remote-scan", sinks.begin(), sinks.end());
      logger_->flush_on(spdlog::level::info);
      logger_->set_pattern(LOG_PATTERN);

#if defined(_DEBUG) || !defined(NDEBUG)
      logger_->set_level(spdlog::level::trace);
#endif
   }

   Logger& Logger::Instance()
   {
      static Logger instance;
      return instance;
   }

   void Logger::InitApprise(const AppriseLoggingConfig& config)
   {
      if (config.enabled)
      {
         logApprise_ = std::make_unique<LogApprise>(config);
      }
   }

   void Logger::Trace(const std::string& msg)
   {
      logger_->trace(msg);
   }

   void Logger::Info(const std::string& msg)
   {
      logger_->info(msg);
   }

   void Logger::Warning(const std::string& msg)
   {
      logger_->warn(msg);

      if (logApprise_)
      {
         logApprise_->Send(std::format("WARNING: {}", utils::StripAsciiCharacters(msg)));
      }
   }

   void Logger::Error(const std::string& msg)
   {
      logger_->error(msg);

      if (logApprise_)
      {
         logApprise_->Send(std::format("ERROR: {}", utils::StripAsciiCharacters(msg)));
      }
   }
}