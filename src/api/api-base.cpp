#include "api-base.h"

#include "src/logger/log-utils.h"

namespace remote_scan
{
   ApiBase::ApiBase(const ServerConfig& serverConfig, std::string_view className, std::string_view ansiiCode)
      : name_(serverConfig.name)
      , url_(serverConfig.address)
      , apiKey_(serverConfig.apiKey)
      , logHeader_(std::format("{}{}{}({})", ansiiCode, className, ANSII_CODE_LOG, name_))
   {
   }

   const std::string& ApiBase::GetName() const
   {
      return name_;
   }

   const std::string& ApiBase::GetUrl() const
   {
      return url_;
   }

   const std::string& ApiBase::GetApiKey() const
   {
      return apiKey_;
   }

   const std::string& ApiBase::GetLogHeader() const
   {
      return logHeader_;
   }
}