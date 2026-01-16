#include "api-emby.h"

#include "types.h"

#include <glaze/glaze.hpp>
#include <warp/log.h>
#include <warp/log-types.h>
#include <warp/log-utils.h>

#include <format>
#include <ranges>

namespace remote_scan
{
   namespace
   {
      constexpr std::string_view API_BASE{"/emby"};
      constexpr std::string_view API_TOKEN_NAME{"api_key"};

      constexpr std::string_view API_SYSTEM_INFO{"/System/Info"};
      constexpr std::string_view API_MEDIA_FOLDERS{"/Library/SelectableMediaFolders"};
   }

   struct JsonServerResponse
   {
      std::string ServerName;
   };

   struct JsonEmbyLibrary
   {
      std::string Name;
      std::string Id;
   };

   EmbyApi::EmbyApi(const ServerConfig& serverConfig)
      : ApiBase(serverConfig, "EmbyApi", warp::ANSI_CODE_EMBY)
      , client_(GetUrl())
   {
      constexpr time_t timeoutSec{5};
      client_.set_connection_timeout(timeoutSec);
   }

   std::string_view EmbyApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view EmbyApi::GetApiTokenName() const
   {
      return API_TOKEN_NAME;
   }

   bool EmbyApi::GetValid()
   {
      auto res = client_.Get(BuildApiPath(API_SYSTEM_INFO), emptyHeaders_);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> EmbyApi::GetServerReportedName()
   {
      auto res = client_.Get(BuildApiPath(API_SYSTEM_INFO), emptyHeaders_);

      if (!IsHttpSuccess(__func__, res))
      {
         return std::nullopt;
      }

      JsonServerResponse serverResponse;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (serverResponse, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      if (serverResponse.ServerName.empty())
      {
         return std::nullopt;
      }
      return std::move(serverResponse.ServerName);
   }

   std::optional<std::string> EmbyApi::GetLibraryId(std::string_view libraryName)
   {
      auto res = client_.Get(BuildApiPath(API_MEDIA_FOLDERS), emptyHeaders_);

      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      std::vector<JsonEmbyLibrary> jsonLibraries;
      if (auto ec = glz::read < glz::opts{.error_on_unknown_keys = false} > (jsonLibraries, res.value().body))
      {
         LogWarning("{} - JSON Parse Error: {}",
                    __func__, glz::format_error(ec, res.value().body));
         return std::nullopt;
      }

      auto it = std::ranges::find_if(jsonLibraries, [&](const auto& lib) {
         return lib.Name == libraryName;
      });

      if (it != jsonLibraries.end())
      {
         // Move the ID out of our temporary vector and return it
         return std::move(it->Id);
      }
      return std::nullopt;
   }

   void EmbyApi::SetLibraryScan(std::string_view libraryId)
   {
      httplib::Headers headers = {
         {"accept", "*/*"}
      };

      const auto apiUrl = BuildApiParamsPath(std::format("/Items/{}/Refresh", libraryId), {
         {"Recursive", "true"},
         {"ImageRefreshMode", "Default"},
         {"ReplaceAllImages", "false"},
         {"ReplaceAllMetadata", "false"}
      });

      auto res = client_.Post(apiUrl, headers);
      IsHttpSuccess(__func__, res);
   }
}