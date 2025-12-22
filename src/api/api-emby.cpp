#include "api-emby.h"

#include "src/logger/logger.h"
#include "src/logger/log-utils.h"
#include "src/types.h"

#include <external/json/json.hpp>
#include <format>
#include <ranges>

namespace remote_scan
{
   static const std::string API_BASE{"/emby"};
   static const std::string API_SYSTEM_INFO{"/System/Info"};
   static const std::string API_MEDIA_FOLDERS{"/Library/SelectableMediaFolders"};

   static constexpr std::string_view SERVER_NAME{"ServerName"};
   static constexpr std::string_view NAME{"Name"};
   static constexpr std::string_view ID{"Id"};

   EmbyApi::EmbyApi(const ServerConfig& serverConfig)
      : ApiBase(serverConfig, "EmbyApi", ANSII_CODE_EMBY)
      , client_(GetUrl())
   {
      constexpr time_t timeoutSec{5};
      client_.set_connection_timeout(timeoutSec);
   }

   std::string EmbyApi::BuildApiPath(std::string_view path)
   {
      return std::format("{}{}?api_key={}", API_BASE, path, GetApiKey());
   }

   void EmbyApi::AddApiParam(std::string& url, const std::list<std::pair<std::string_view, std::string_view>>& params)
   {
      std::ranges::for_each(params, [&url](const auto& param) {
         url.append(std::format("&{}={}", param.first, param.second));
      });
   }

   bool EmbyApi::GetValid()
   {
      httplib::Headers header;
      auto res = client_.Get(BuildApiPath(API_SYSTEM_INFO), header);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> EmbyApi::GetServerReportedName()
   {
      httplib::Headers headers;
      auto res = client_.Get(BuildApiPath(API_SYSTEM_INFO), headers);
      if (res.error() == httplib::Error::Success)
      {
         auto data = nlohmann::json::parse(res.value().body);
         if (data.contains(SERVER_NAME))
         {
            return data.at(SERVER_NAME).get<std::string>();
         }
      }
      return std::nullopt;
   }

   std::optional<std::string> EmbyApi::GetLibraryId(std::string_view libraryName)
   {
      httplib::Headers headers;
      auto res = client_.Get(BuildApiPath(API_MEDIA_FOLDERS), headers);
      if (res.error() == httplib::Error::Success)
      {
         auto data = nlohmann::json::parse(res.value().body);
         for (const auto& library : data)
         {
            if (library.contains(NAME) && library.at(NAME).get<std::string>() == libraryName && library.contains(ID))
            {
               return library.at(ID).get<std::string>();
            }
         }
      }
      return std::nullopt;
   }

   void EmbyApi::SetLibraryScan(std::string_view libraryId)
   {
      httplib::Headers headers = {
         {"accept", "*/*"}
      };

      auto apiUrl = BuildApiPath(std::format("/Items/{}/Refresh", libraryId));
      AddApiParam(apiUrl, {
         {"Recursive", "true"},
         {"ImageRefreshMode", "Default"},
         //{"MetadataRefreshMode", ""}, // optional paramater
         {"ReplaceAllImages", "false"},
         {"ReplaceAllMetadata", "false"}
      });

      auto res = client_.Post(apiUrl, headers);
      if (res.error() != httplib::Error::Success || res.value().status >= VALID_HTTP_RESPONSE_MAX)
      {
         Logger::Instance().Warning(std::format("{} - Library Scan {}",
                                                GetLogHeader(),
                                                GetTag("error", res.error() != httplib::Error::Success ? std::to_string(static_cast<int>(res.error())) : std::format("{} - {}", res.value().reason, res.value().body))
         ));
      }
   }
}