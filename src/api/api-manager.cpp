#include "api-manager.h"

#include <warp/log.h>
#include <warp/log-utils.h>

#include <algorithm>
#include <ranges>

namespace remote_scan
{
   ApiManager::ApiManager(std::shared_ptr<ConfigReader> configReader)
   {
      SetupPlexApis(configReader->GetPlexServers());
      SetupEmbyApis(configReader->GetEmbyServers());
   }

   void ApiManager::SetupPlexApis(const std::vector<ServerConfig>& serverConfigs)
   {
      std::ranges::for_each(serverConfigs, [this](const auto& server) {
         auto plexApi{std::make_unique<PlexApi>(server)};
         plexApi->GetValid() ? LogServerConnectionSuccess(warp::GetFormattedPlex(), plexApi.get()) : LogServerConnectionError(plexApi.get());
         plexApis_.emplace_back(std::move(plexApi));
      });
   }

   void ApiManager::SetupEmbyApis(const std::vector<ServerConfig>& serverConfigs)
   {
      std::ranges::for_each(serverConfigs, [this](const auto& server) {
         auto embyApi{std::make_unique<EmbyApi>(server)};
         embyApi->GetValid() ? LogServerConnectionSuccess(warp::GetFormattedEmby(), embyApi.get()) : LogServerConnectionError(embyApi.get());
         embyApis_.emplace_back(std::move(embyApi));
      });
   }

   void ApiManager::LogServerConnectionSuccess(std::string_view serverName, ApiBase* api)
   {
      auto serverReportedName{api->GetServerReportedName()};
      if (serverReportedName)
      {
         warp::log::Info("Connected to {}({}) successfully", serverName, *api->GetServerReportedName());
      }
      else
      {
         LogServerConnectionError(api);
      }
   }

   void ApiManager::LogServerConnectionError(ApiBase* api)
   {
      warp::log::Warning("{}({}) server not available. Is this correct {} {}",
                         warp::GetFormattedEmby(),
                         api->GetName(),
                         warp::GetTag("url", api->GetUrl()),
                         warp::GetTag("api_key", api->GetApiKey()));
   }

   PlexApi* ApiManager::GetPlexApi(std::string_view name) const
   {
      auto iter = std::ranges::find_if(plexApis_, [name](const auto& api) {
         return api->GetName() == name;
      });
      return iter != plexApis_.end() ? iter->get() : nullptr;
   }

   EmbyApi* ApiManager::GetEmbyApi(std::string_view name) const
   {
      auto iter = std::ranges::find_if(embyApis_, [name](const auto& api) {
         return api->GetName() == name;
      });
      return iter != embyApis_.end() ? iter->get() : nullptr;
   }

   ApiBase* ApiManager::GetApi(warp::ApiType type, std::string_view name) const
   {
      switch (type)
      {
         case warp::ApiType::PLEX:
            return GetPlexApi(name);
         case warp::ApiType::EMBY:
            return GetEmbyApi(name);
         default:
            break;
      }
      return nullptr;
   }
}