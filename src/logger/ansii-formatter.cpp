#include "ansii-formatter.h"

#include "logger/logger-types.h"

namespace remote_scan
{
   void AnsiiFormatter::format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest)
   {
      switch (msg.level)
      {
         case spdlog::level::info:
            patternFormatter_.set_pattern(LOG_PATTERN_ANSII_INFO);
            break;
         case spdlog::level::warn:
            patternFormatter_.set_pattern(LOG_PATTERN_ANSII_WARNING);
            break;
         case spdlog::level::err:
            patternFormatter_.set_pattern(LOG_PATTERN_ANSII_ERROR);
            break;
         case spdlog::level::critical:
            patternFormatter_.set_pattern(LOG_PATTERN_ANSII_CRITICAL);
            break;
         default:
            patternFormatter_.set_pattern(LOG_PATTERN_ANSII_DEFAULT);
            break;
      }

      // Use the normal pattern formatter to create the log message with no ansii codes
      patternFormatter_.format(msg, dest);
   }

   std::unique_ptr<spdlog::formatter> AnsiiFormatter::clone() const
   {
      return spdlog::details::make_unique<AnsiiFormatter>();
   }
}