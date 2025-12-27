#include "log-apprise.h"

#include "logger/logger.h"
#include "types.h"

#include <format>

namespace remote_scan
{
   LogApprise::LogApprise(const AppriseLoggingConfig& config)
      : client_(config.url)
      , key_(config.key)
      , title_(config.title)
   {
   }

   void LogApprise::Send(const std::string& msg)
   {
      httplib::Params params{
         {"title", title_},
         {"body", msg}
      };
      auto path = std::format("/notify/{}", key_);
      if (auto res = client_.Post(path, params);
          res.error() != httplib::Error::Success || res.value().status >= VALID_HTTP_RESPONSE_MAX)
      {
         Logger::Instance().Warning("Apprise failed to log message!");
      }
   }
}