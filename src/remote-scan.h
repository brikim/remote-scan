
#include "api/api-manager.h"
#include "config-reader/config-reader.h"
#include "config-reader/config-reader-types.h"
#include "update-listener.h"

#include <atomic>
#include <chrono>
#include <efsw/efsw.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace remote_scan
{
   struct ActiveMonitor
   {
      std::string scanName;
      std::chrono::system_clock::time_point time;
      std::vector<std::string> paths;
   };

   class RemoteScan
   {
   public:
      RemoteScan(std::shared_ptr<ConfigReader> configReader);
      virtual ~RemoteScan() = default;

      void Run();

      void ProcessFileUpdate(std::string_view scanName, std::string_view path, std::string_view filename);

      void ProcessShutdown();

   private:
      void Monitor(std::stop_token stopToken);

      std::string GetLowercase(std::string_view name);

      bool GetScanPathValid(std::string_view path);
      bool GetFileExtensionValid(std::string_view filename);
      std::string GetFolderName(std::string_view path);

      void LogMonitorAdded(std::string_view scanName, std::string_view path);
      void AddFileMonitor(std::string_view scanName, std::string_view path);

      void LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library);
      void LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library);
      bool NotifyServer(ApiType type, const ScanLibraryConfig& library);
      void NotifyMediaServers(const ActiveMonitor& monitor);

      std::shared_ptr<ConfigReader> configReader_;
      ApiManager apiManager_;

      std::vector<std::pair<std::unique_ptr<efsw::FileWatcher>, std::unique_ptr<UpdateListener>>> watchers_;

      std::vector<ActiveMonitor> activeMonitors_;
      std::mutex monitorLock_;

      std::chrono::system_clock::time_point lastNotifyTime_{std::chrono::system_clock::now()};

      std::atomic_bool shutdown_{false};
      std::atomic_bool runThread_{false};

      std::mutex conditionLock_;
      std::condition_variable condition_;
      std::unique_ptr<std::jthread> monitorThread_;
   };
}