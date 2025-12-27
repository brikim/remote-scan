#include "ansii-formatter.h"

#include "logger/logger-types.h"

namespace remote_scan
{
   AnsiiFormatter::AnsiiFormatter()
   {
      infoFormatter_.set_pattern(LOG_PATTERN_ANSII_INFO);
      warningFormatter_.set_pattern(LOG_PATTERN_ANSII_WARNING);
      errorFormatter_.set_pattern(LOG_PATTERN_ANSII_ERROR);
      criticalFormatter_.set_pattern(LOG_PATTERN_ANSII_CRITICAL);
      defaultFormatter_.set_pattern(LOG_PATTERN_ANSII_DEFAULT);
   }

   void AnsiiFormatter::format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest)
   {
      spdlog::pattern_formatter* formatter{nullptr};
      switch (msg.level)
      {
         case spdlog::level::info:
            formatter = &infoFormatter_;
            break;
         case spdlog::level::warn:
            formatter = &warningFormatter_;
            break;
         case spdlog::level::err:
            formatter = &errorFormatter_;
            break;
         case spdlog::level::critical:
            formatter = &criticalFormatter_;
            break;
         default:
            formatter = &defaultFormatter_;
            break;
      }

      // Use the normal pattern formatter to create the log message with no ansii codes
      formatter->format(msg, dest);
   }

   std::unique_ptr<spdlog::formatter> AnsiiFormatter::clone() const
   {
      return spdlog::details::make_unique<AnsiiFormatter>();
   }
}