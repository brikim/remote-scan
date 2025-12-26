#include "ansii-remove-formatter.h"

#include "logger-types.h"

#include <regex>

namespace remote_scan
{
   AnsiiRemoveFormatter::AnsiiRemoveFormatter()
   {
      patternFormatter_.set_pattern(LOG_PATTERN);
   }

   std::string AnsiiRemoveFormatter::StripAsciiCharacters(const std::string& data)
   {
      // Strip ansii codes from the log msg
      const std::regex ansii("\x1B(?:[@-Z\\-_]|\[[0-?]*[ -/]*[@-~])");
      return std::regex_replace(data, ansii, "");

   }

   void AnsiiRemoveFormatter::format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest)
   {
      // Create a new log message with the new string
      auto formattedMsg{StripAsciiCharacters(msg.payload.data())};
      spdlog::details::log_msg newMsg(msg.time, msg.source, msg.logger_name, msg.level, formattedMsg);

      // Use the normal pattern formatter to create the log message with no ansii codes
      patternFormatter_.format(newMsg, dest);
   }

   std::unique_ptr<spdlog::formatter> AnsiiRemoveFormatter::clone() const
   {
      return spdlog::details::make_unique<AnsiiRemoveFormatter>();
   }
}