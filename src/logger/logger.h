#pragma once

#include "config-reader/config-reader-types.h"
#include "log-apprise.h"

#include <memory>
#include <spdlog/spdlog.h>
#include <vector>

namespace remote_scan
{
   class Logger
   {
   public:
      // Returns a static instance of the Logger class
      static Logger& Instance();

      void InitApprise(const AppriseLoggingConfig& config);

      void Trace(const std::string& msg);
      void Info(const std::string& msg);
      void Warning(const std::string& msg);
      void Error(const std::string& msg);

   private:
      Logger();
      virtual ~Logger() = default;

      std::vector<std::shared_ptr<spdlog::logger>> loggerVec_;
      std::unique_ptr<LogApprise> logApprise_;
   };
}