#pragma once

#include <spdlog/spdlog.h>

namespace remote_scan
{
   // Class used to strip the ansii character strings from a log message
   class AnsiiFormatter : public spdlog::formatter
   {
   public:
      AnsiiFormatter() = default;
      virtual ~AnsiiFormatter() = default;

      void format(const spdlog::details::log_msg& msg, spdlog::memory_buf_t& dest) override;
      std::unique_ptr<spdlog::formatter> clone() const override;

   private:
      spdlog::pattern_formatter patternFormatter_;
   };
}