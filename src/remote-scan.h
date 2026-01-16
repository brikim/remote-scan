#pragma once

#include "api/api-manager.h"
#include "config-reader/config-reader-types.h"

#include <warp/log-types.h>
#include <watcher/watcher.hpp>

#include <chrono>
#include <condition_variable>
#include <list>
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

   class RemoteScan
   {
   public:
      explicit RemoteScan(std::shared_ptr<ConfigReader> configReader);
      virtual ~RemoteScan() = default;

      // Rule of Five: Manage thread/mutex resources safely
      RemoteScan(const RemoteScan&) = delete;
      RemoteScan& operator=(const RemoteScan&) = delete;

      void Run();
      void ProcessFileUpdate(std::string_view scanName, std::string_view path, std::string_view filename);
      void ProcessShutdown();

   private:
      struct ActiveMonitorPaths
      {
         std::string path;
         std::string displayFolder;
      };

      struct ActiveMonitor
      {
         std::string scanName;
         std::chrono::system_clock::time_point time;
         std::vector<ActiveMonitorPaths> paths;
      };

      void SetupScans();
      void Monitor(std::stop_token stopToken);
      void CleanupShutdown();

      bool GetScanPathValid(std::string_view path);
      bool GetFileExtensionValid(std::string_view filename);
      std::string GetDisplayFolder(std::string_view path);

      void LogMonitorAdded(std::string_view scanName, std::string_view displayFolder);
      void AddFileMonitor(std::string_view scanName, std::string_view path);

      void LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library);
      void LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library);
      bool NotifyServer(warp::ApiType type, const ScanLibraryConfig& library);
      void NotifyMediaServers(const ActiveMonitor& monitor);

      std::shared_ptr<ConfigReader> configReader_;
      ApiManager apiManager_;

      std::list<wtr::watch> activeWatches_;

      std::unordered_set<std::string> validExtensions_;

      // Synchronization
      std::mutex monitorLock_;
      std::condition_variable_any monitorCv_;
      std::vector<ActiveMonitor> activeMonitors_;
      std::unique_ptr<std::jthread> monitorThread_;
      std::chrono::system_clock::time_point lastNotifyTime_{std::chrono::system_clock::now()};

      std::stop_source stopSource_;
   };
}