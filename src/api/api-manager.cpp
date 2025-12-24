#include "api-manager.h"

#include "logger/logger.h"
#include "logger/log-utils.h"

#include <format>
#include <ranges>

namespace remote_scan
{
   ApiManager::ApiManager(std::shared_ptr<ConfigReader> configReader)
   {
      SetupPlexApis(configReader->GetPlexServers());
      SetupEmbyApis(configReader->GetEmbyServers());

      const auto& jellyfinServers{configReader->GetJellyfinServers()};
      std::ranges::for_each(jellyfinServers, [](const auto& server) {
      });
   }

   void ApiManager::SetupPlexApis(const std::vector<ServerConfig>& serverConfigs)
   {
      std::ranges::for_each(serverConfigs, [this](const auto& server) {
         auto plexApi{std::make_unique<PlexApi>(server)};
         plexApi->GetValid() ? LogServerConnectionSuccess(GetFormattedPlex(), plexApi.get()) : LogServerConnectionError(plexApi.get());
         plexApis_.emplace_back(std::move(plexApi));
      });
   }

   void ApiManager::SetupEmbyApis(const std::vector<ServerConfig>& serverConfigs)
   {
      std::ranges::for_each(serverConfigs, [this](const auto& server) {
         auto embyApi{std::make_unique<EmbyApi>(server)};
         embyApi->GetValid() ? LogServerConnectionSuccess(GetFormattedEmby(), embyApi.get()) : LogServerConnectionError(embyApi.get());
         embyApis_.emplace_back(std::move(embyApi));
      });
   }

   void ApiManager::LogServerConnectionSuccess(std::string_view serverName, ApiBase* api)
   {
      auto serverReportedName{api->GetServerReportedName()};
      if (serverReportedName.has_value())
      {
         Logger::Instance().Info(std::format("Connected to {}({}) successfully", serverName, api->GetServerReportedName().value()));
      }
      else
      {
         LogServerConnectionError(api);
      }
   }

   void ApiManager::LogServerConnectionError(ApiBase* api)
   {
      Logger::Instance().Warning(std::format("{}({}) server not available. Is this correct {} {}",
                                             GetFormattedEmby(),
                                             api->GetName(),
                                             GetTag("url", api->GetUrl()),
                                             GetTag("api_key", api->GetApiKey())));
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

   ApiBase* ApiManager::GetApi(ApiType type, std::string_view name) const
   {
      switch (type)
      {
         case ApiType::PLEX:
            return GetPlexApi(name);
         case ApiType::EMBY:
            return GetEmbyApi(name);
         case ApiType::JELLYFIN:
            break;
         default:
            break;
      }
      return nullptr;
   }
}