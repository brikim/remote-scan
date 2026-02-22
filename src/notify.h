#pragma once

#include "config-reader/config-reader-types.h"
#include "types.h"

#include <warp/api/api-manager.h>
#include <warp/types.h>

#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

namespace remote_scan
{
   class ConfigReader;

   class Notify
   {
   public:
      Notify(std::shared_ptr<ConfigReader> configReader,
             std::function<bool(const std::filesystem::path)> getImageFunc);
      virtual ~Notify() = default;

      Notify(const Notify&) = delete;
      Notify& operator=(const Notify&) = delete;

      void GetTasks(std::vector<warp::Task>& tasks);

      void NotifyMediaServers(const ActiveMonitor& monitor);

   private:
      void LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library);
      void LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library);

      bool NotifyPlex(const ActiveMonitor& monitor, const std::filesystem::path& basePath, const ScanLibraryConfig& library, bool dryRun);
      bool NotifyEmby(const ActiveMonitor& monitor, const std::filesystem::path& basePath, const ScanLibraryConfig& library, bool dryRun);

      std::shared_ptr<ConfigReader> configReader_;
      std::unique_ptr<warp::ApiManager> apiManager_;
      std::function<bool(const std::filesystem::path)> getImageFunc_;
   };
}