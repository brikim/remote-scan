#include "api-plex.h"

#include "logger/logger.h"
#include "logger/log-utils.h"
#include "types.h"

#include <format>
#include <pugixml/src/pugixml.hpp>
#include <ranges>

namespace remote_scan
{
   static const std::string API_BASE{""};
   static const std::string API_SERVERS{"/servers"};
   static const std::string API_LIBRARIES{"/library/sections/"};

   static constexpr std::string_view ELEM_MEDIA_CONTAINER{"MediaContainer"};

   static constexpr std::string_view ATTR_NAME{"name"};
   static constexpr std::string_view ATTR_KEY{"key"};
   static constexpr std::string_view ATTR_TITLE{"title"};

   PlexApi::PlexApi(const ServerConfig& serverConfig)
      : ApiBase(serverConfig, "PlexApi", ANSI_CODE_PLEX)
      , client_(GetUrl())
   {
      constexpr time_t timeoutSec{5};
      client_.set_connection_timeout(timeoutSec);
   }

   std::string PlexApi::BuildApiPath(std::string_view path)
   {
      return std::format("{}{}?X-Plex-Token={}", API_BASE, path, GetApiKey());
   }

   bool PlexApi::GetValid()
   {
      httplib::Headers header;
      auto res = client_.Get(BuildApiPath(API_SERVERS), header);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> PlexApi::GetServerReportedName()
   {
      httplib::Headers header;
      if (auto res = client_.Get(BuildApiPath(API_SERVERS), header);
          res.error() == httplib::Error::Success)
      {
         pugi::xml_document data;
         if (data.load_buffer(res.value().body.c_str(), res.value().body.size()).status == pugi::status_ok
             && data.child(ELEM_MEDIA_CONTAINER)
             && data.child(ELEM_MEDIA_CONTAINER).first_child()
             && data.child(ELEM_MEDIA_CONTAINER).first_child().attribute(ATTR_NAME))
         {
            return data.child(ELEM_MEDIA_CONTAINER).first_child().attribute(ATTR_NAME).as_string();
         }
         else
         {
            Logger::Instance().Warning(std::format("{} - GetServerReportedName malformed xml reply received", GetLogHeader()));
         }
      }
      return std::nullopt;
   }

   std::optional<std::string> PlexApi::GetLibraryId(std::string_view libraryName)
   {
      httplib::Headers header;
      if (auto res = client_.Get(BuildApiPath(API_LIBRARIES), header);
          res.error() == httplib::Error::Success)
      {
         pugi::xml_document data;
         if (data.load_buffer(res.value().body.c_str(), res.value().body.size()).status == pugi::status_ok
             && data.child(ELEM_MEDIA_CONTAINER))
         {
            for (const auto& library : data.child(ELEM_MEDIA_CONTAINER))
            {
               if (library
                   && library.attribute(ATTR_TITLE)
                   && library.attribute(ATTR_TITLE).as_string() == libraryName
                   && library.attribute(ATTR_KEY))
               {
                  return library.attribute(ATTR_KEY).as_string();
               }
            }
         }
      }
      return std::nullopt;
   }

   void PlexApi::SetLibraryScan(std::string_view libraryId)
   {
      httplib::Headers headers;
      auto apiUrl = BuildApiPath(std::format("/library/sections/{}/refresh", libraryId));
      if (auto res = client_.Get(apiUrl, headers);
          res.error() != httplib::Error::Success
          || res.value().status >= VALID_HTTP_RESPONSE_MAX)
      {
         Logger::Instance().Warning(std::format("{} - Library Scan {}",
                                                GetLogHeader(),
                                                GetTag("error", res.error() != httplib::Error::Success ? std::to_string(static_cast<int>(res.error())) : std::format("{} - {}", res.value().reason, res.value().body))
         ));
      }
   }
}