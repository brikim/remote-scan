#include "api-base.h"

#include "types.h"

#include <warp/log.h>
#include <warp/log-utils.h>

namespace remote_scan
{
   ApiBase::ApiBase(const ServerConfig& serverConfig,
                    std::string_view className,
                    std::string_view ansiiCode)
      : header_(std::format("{}{}{}({})", ansiiCode, className, warp::ANSI_CODE_LOG, serverConfig.name))
      , name_(serverConfig.name)
      , url_(serverConfig.address)
      , apiKey_(serverConfig.apiKey)
      , logHeader_(std::format("{}{}{}({})", ansiiCode, className, warp::ANSI_CODE_LOG, name_))
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

   void ApiBase::AddApiParam(std::string& url, const ApiParams& params) const
   {
      if (params.empty()) return;

      bool hasQuery = (url.find('?') != std::string::npos);
      bool lastIsSeparator = !url.empty() && (url.back() == '?' || url.back() == '&');
      for (const auto& [key, value] : params)
      {
         if (!lastIsSeparator)
         {
            url += hasQuery ? '&' : '?';
         }
         url += key;
         url += '=';
         url += GetPercentEncoded(value);

         hasQuery = true;
         lastIsSeparator = false;
      }
   }

   std::string ApiBase::BuildApiPath(std::string_view path) const
   {
      auto apiTokenName = GetApiTokenName();
      char separator = (path.find('?') == std::string_view::npos) ? '?' : '&';
      if (apiTokenName.empty())
      {
         return std::format("{}{}", GetApiBase(), path);
      }
      else
      {
         return std::format("{}{}{}{}={}",
                            GetApiBase(),
                            path,
                            separator,
                            apiTokenName,
                            GetPercentEncoded(GetApiKey()));
      }
   }

   std::string ApiBase::BuildApiParamsPath(std::string_view path, const ApiParams& params) const
   {
      auto apiPath = BuildApiPath(path);
      AddApiParam(apiPath, params);
      return apiPath;
   }

   std::string ApiBase::GetPercentEncoded(std::string_view src) const
   {
      // 1. Lookup table for "unreserved" characters (RFC 3986)
      // 0 = needs encoding, 1 = safe
      static const bool SAFE[256] = {
          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 0-31
          0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0, // 32-63 (Keep . -)
          0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1, // 64-95 (Keep _)
          0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,1,0, // 96-127 (Keep ~)
          // ... all others (128-255) are 0
      };

      static const char hex_chars[] = "0123456789ABCDEF";

      // 2. Pre-calculate exact size to avoid reallocations
      size_t new_size{0};
      for (unsigned char c : src)
      {
         new_size += SAFE[c] ? 1 : 3;
      }

      // 3. One single allocation
      std::string result;
      result.reserve(new_size);

      // 4. Direct pointer-style insertion
      for (unsigned char c : src)
      {
         if (SAFE[c])
         {
            result.push_back(c);
         }
         else
         {
            result.push_back('%');
            result.push_back(hex_chars[c >> 4]);   // High nibble
            result.push_back(hex_chars[c & 0x0F]); // Low nibble
         }
      }

      return result;
   }

   bool ApiBase::IsHttpSuccess(std::string_view name, const httplib::Result& result, bool log)
   {
      std::string error;

      if (result.error() != httplib::Error::Success)
      {
         error = httplib::to_string(result.error());
      }
      else if (result->status >= VALID_HTTP_RESPONSE_MAX)
      {
         error = std::format("Status {}: {} - {}", result->status, result->reason, result->body);
      }
      else
      {
         // Everything passed
         return true;
      }

      if (log) warp::log::Warning("{} - HTTP error {}", name, warp::GetTag("error", error));
      return false;
   }
}