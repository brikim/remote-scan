#pragma once

#include "config-reader/config-reader-types.h"
#include "types.h"

#include <warp/api/api-manager.h>
#include <warp/log/log-types.h>
#include <warp/types.h>

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace remote_scan
{
   class ConfigReader;

   class FileMonitor
   {
   public:
      explicit FileMonitor(std::shared_ptr<ConfigReader> configReader);
      virtual ~FileMonitor() = default;

      FileMonitor(const FileMonitor&) = delete;
      FileMonitor& operator=(const FileMonitor&) = delete;

      void GetTasks(std::vector<warp::Task>& tasks);

      void Run();
      void Shutdown();

      void Process(const FileMonitorData& fileMonitor);

   private:
      struct ActiveMonitorPaths
      {
         std::filesystem::path path;
         std::string displayFolder;
      };

      struct ActiveMonitor
      {
         std::string scanName;
         std::chrono::system_clock::time_point time;
         std::vector<ActiveMonitorPaths> paths;
         std::filesystem::path lastPath;
         bool destroy{false};
      };

      void Monitor(std::stop_token stopToken);

      bool GetScanPathValid(const std::filesystem::path& path);
      bool GetFileExtensionValid(const std::filesystem::path& filename);

      void LogMonitorAdded(std::string_view scanName, std::string_view displayFolder);
      void AddFileMonitor(const FileMonitorData& fileMonitor);

      void LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library);
      void LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library);
      void NotifyMediaServers(const ActiveMonitor& monitor);

      std::shared_ptr<ConfigReader> configReader_;
      std::unique_ptr<warp::ApiManager> apiManager_;

      std::vector<std::string> ignoreFolders_;
      std::unordered_set<std::string> validExtensions_;

      // Synchronization
      std::mutex monitorLock_;
      std::condition_variable_any monitorCv_;
      std::vector<ActiveMonitor> activeMonitors_;
      std::chrono::system_clock::time_point lastNotifyTime_{std::chrono::system_clock::now()};
      std::jthread monitorThread_;
   };
}