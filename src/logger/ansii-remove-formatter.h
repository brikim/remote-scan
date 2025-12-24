#pragma once

#include "config-reader/config-reader-types.h"

#include <spdlog/spdlog.h>
#include <string>

namespace remote_scan
{
   // Class used to strip the ansii character strings from a log message
   class AnsiiRemoveFormatter : public spdlog::formatter
   {
   public:
      AnsiiRemoveFormatter();
      virtual ~AnsiiRemoveFormatter() = default;

      static std::string StripAsciiCharacters(const std::string& data);

      void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override;
      std::unique_ptr<spdlog::formatter> clone() const override;

   private:
      spdlog::pattern_formatter patternFormatter_;
   };
}