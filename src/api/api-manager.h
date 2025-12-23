#pragma once

#include "src/api/api-base.h"
#include "src/api/api-emby.h"
#include "src/api/api-plex.h"
#include "src/config-reader/config-reader.h"
#include "src/types.h"

#include <memory>
#include <vector>

namespace remote_scan
{
   class ApiManager
   {
   public:
      ApiManager(std::shared_ptr<ConfigReader> configReader);
      virtual ~ApiManager() = default;

      [[nodiscard]] ApiBase* GetApi(ApiType type, std::string_view name) const;
      [[nodiscard]] PlexApi* GetPlexApi(std::string_view name) const;
      [[nodiscard]] EmbyApi* GetEmbyApi(std::string_view name) const;

   private:
      void SetupPlexApis(const std::vector<ServerConfig>& serverConfigs);
      void SetupEmbyApis(const std::vector<ServerConfig>& serverConfigs);

      void LogServerConnectionSuccess(std::string_view serverName, ApiBase* api);
      void LogServerConnectionError(ApiBase* api);

      std::vector<std::unique_ptr<PlexApi>> plexApis_;
      std::vector<std::unique_ptr<EmbyApi>> embyApis_;

   };
}