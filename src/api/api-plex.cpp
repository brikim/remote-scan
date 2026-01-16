#include "api-plex.h"

#include "types.h"

#include <pugixml.hpp>
#include <warp/log.h>
#include <warp/log-types.h>
#include <warp/log-utils.h>

#include <format>
#include <ranges>

namespace remote_scan
{
   namespace
   {
      constexpr std::string_view API_BASE{""};
      constexpr std::string_view API_TOKEN_NAME("X-Plex-Token");

      constexpr std::string_view API_SERVERS{"/servers"};
      constexpr std::string_view API_LIBRARIES{"/library/sections/"};

      constexpr std::string_view ELEM_MEDIA_CONTAINER{"MediaContainer"};

      constexpr std::string_view ATTR_NAME{"name"};
      constexpr std::string_view ATTR_KEY{"key"};
      constexpr std::string_view ATTR_TITLE{"title"};
   }

   PlexApi::PlexApi(const ServerConfig& serverConfig)
      : ApiBase(serverConfig, "PlexApi", warp::ANSI_CODE_PLEX)
      , client_(GetUrl())
   {
      constexpr time_t timeoutSec{5};
      client_.set_connection_timeout(timeoutSec);
   }

   std::string_view PlexApi::GetApiBase() const
   {
      return API_BASE;
   }

   std::string_view PlexApi::GetApiTokenName() const
   {
      return API_TOKEN_NAME;
   }

   bool PlexApi::GetValid()
   {
      auto res = client_.Get(BuildApiPath(API_SERVERS), headers_);
      return res.error() == httplib::Error::Success && res.value().status < VALID_HTTP_RESPONSE_MAX;
   }

   std::optional<std::string> PlexApi::GetServerReportedName()
   {
      auto res = client_.Get(BuildApiPath(API_SERVERS), headers_);

      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      pugi::xml_document doc;
      if (doc.load_buffer(res->body.data(), res->body.size()).status != pugi::status_ok)
      {
         LogWarning("{} - Malformed XML reply received", std::string(__func__));
         return std::nullopt;
      }

      auto serverNode = doc.select_node("//Server[@name]");
      if (!serverNode)
      {
         LogWarning("{} - No Server element with a name attribute found", __func__);
         return std::nullopt;
      }

      return serverNode.node().attribute(ATTR_NAME).as_string();
   }

   std::optional<std::string> PlexApi::GetLibraryId(std::string_view libraryName)
   {
      auto res = client_.Get(BuildApiPath(API_LIBRARIES), headers_);

      if (!IsHttpSuccess(__func__, res)) return std::nullopt;

      pugi::xml_document doc;
      if (doc.load_buffer(res->body.data(), res->body.size()).status != pugi::status_ok)
      {
         return std::nullopt;
      }

      auto query = std::format("//{}[@{}='{}']", "Directory", ATTR_TITLE, libraryName);
      auto libraryNode = doc.select_node(query.c_str());
      if (!libraryNode)
      {
         return std::nullopt;
      }

      std::string key = libraryNode.node().attribute(ATTR_KEY).as_string();
      return key.empty() ? std::nullopt : std::make_optional(key);
   }

   void PlexApi::SetLibraryScan(std::string_view libraryId)
   {
      auto apiUrl = BuildApiPath(std::format("{}{}/refresh", API_LIBRARIES, libraryId));
      auto res = client_.Get(apiUrl, headers_);
      IsHttpSuccess(__func__, res);
   }
}