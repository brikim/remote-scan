#pragma once

#include "config-reader/config-reader-types.h"
#include "notify.h"
#include "types.h"

#include <warp/log/log-types.h>
#include <warp/types.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

namespace remote_scan
{
   class ConfigReader;

   class Monitor
   {
   public:
      explicit Monitor(std::shared_ptr<ConfigReader> configReader);
      virtual ~Monitor() = default;

      Monitor(const Monitor&) = delete;
      Monitor& operator=(const Monitor&) = delete;

      void GetTasks(std::vector<warp::Task>& tasks);

      void Run();
      void Shutdown();

      void Process(const FileMonitorData& fileMonitor);

   private:
      void Work(std::stop_token stopToken);

      [[nodiscard]] bool GetScanPathValid(const std::filesystem::path& path) const;
      [[nodiscard]] bool GetFileImage(const std::filesystem::path& filename) const;
      [[nodiscard]] bool GetFileExtensionValid(const std::filesystem::path& filename) const;

      void LogMonitorAdded(std::string_view scanName,
                           const ActiveMonitorPath& monitor);

      void AddNewFileMonitor(const FileMonitorData& fileMonitor);
      void UpdateExistingFileMonitor(const FileMonitorData& fileMonitor, ActiveMonitor& activeMonitor);
      void AddFileMonitor(const FileMonitorData& fileMonitor);

      std::filesystem::path GetStrippedFileName(const std::filesystem::path& originalPath);

      std::shared_ptr<ConfigReader> configReader_;
      Notify notify_;

      std::vector<std::filesystem::path> ignoreFolders_;
      std::unordered_set<std::string> validImageExtensions_;
      std::unordered_set<std::string> validExtensions_;
      std::unordered_set<std::string> stripExtensions_;

      // Synchronization
      std::mutex workLock_;
      std::condition_variable_any workCv_;
      std::vector<ActiveMonitor> activeMonitors_;
      std::chrono::system_clock::time_point lastNotifyTime_{std::chrono::system_clock::now() - std::chrono::hours(24)};
      std::jthread workThread_;
   };
}