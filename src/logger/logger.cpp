#include "logger.h"

#include "ansii-remove-formatter.h"
#include "logger-types.h"

#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <ranges>
#include <regex>

namespace remote_scan
{
   Logger::Logger()
   {
      spdlog::set_pattern(LOG_PATTERN);

#if defined(_DEBUG) || defined(DEBUG)
      spdlog::set_level(spdlog::level::trace);
#endif

      // Create the console logger
      loggerVec_.emplace_back(spdlog::stdout_color_mt("console"));

      // If the log path is defined create a rotating file logger
      if (const auto* logPath = std::getenv("LOG_PATH");
          logPath != nullptr)
      {
         std::string logPathFilename = logPath;
         logPathFilename.append("/remote-scan.log");

         constexpr auto max_size{1048576 * 5};
         constexpr auto max_files{5};
         auto fileLogger = spdlog::rotating_logger_mt("rotating-file-logger", logPathFilename, max_size, max_files);
         fileLogger->set_formatter(std::make_unique<AnsiiRemoveFormatter>());
         loggerVec_.emplace_back(fileLogger);
      }
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
      std::ranges::for_each(loggerVec_, [&msg](auto& logger) {
         logger->trace(msg);
      });
   }

   void Logger::Info(const std::string& msg)
   {
      std::ranges::for_each(loggerVec_, [&msg](auto& logger) {
         logger->info(msg);
      });
   }

   void Logger::Warning(const std::string& msg)
   {
      std::ranges::for_each(loggerVec_, [&msg](auto& logger) {
         logger->warn(msg);
      });

      if (logApprise_)
      {
         logApprise_->Send(std::format("WARNING: {}", AnsiiRemoveFormatter::StripAsciiCharacters(msg)));
      }
   }

   void Logger::Error(const std::string& msg)
   {
      std::ranges::for_each(loggerVec_, [&msg](auto& logger) {
         logger->error(msg);
      });

      if (logApprise_)
      {
         logApprise_->Send(std::format("ERROR: {}", AnsiiRemoveFormatter::StripAsciiCharacters(msg)));
      }
   }
}