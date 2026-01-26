#include "remote-scan.h"

#include "config-reader/config-reader.h"
#include "types.h"

#include <warp/log/log.h>
#include <warp/log/log-utils.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>

namespace remote_scan
{
   namespace
   {
      constexpr std::string_view APP_NAME("Remote-Scan");
   };

   RemoteScan::RemoteScan(std::shared_ptr<ConfigReader> configReader)
      : fileMonitor_(configReader)
      , scanConfig_(configReader->GetRemoteScanConfig())
   {
      if (scanConfig_.dryRun)
      {
         warp::log::Info("[DRY RUN MODE] Remote Scan will not notify media servers of changes");
      }
   }

   void RemoteScan::SetupScans()
   {
      for (const auto& scan : scanConfig_.scans)
      {
         scans_.emplace_back(std::make_unique<Scan>(scan, [this](const FileMonitorData& data) { fileMonitor_.Process(data); }));
      }
   }

   void RemoteScan::AddTasksToScheduler()
   {
      std::vector<warp::Task> apiTasks;
      fileMonitor_.GetTasks(apiTasks);
      for (const auto& task : apiTasks)
      {
         cronScheduler_.Add(task);
      }
   }

   void RemoteScan::CleanupShutdown()
   {
      warp::log::Info("Removing directory watches");
      for (auto& scan : scans_)
      {
         scan->Shutdown();
      }

      fileMonitor_.Shutdown();
   }

   void RemoteScan::Run()
   {
      // Add any needed tasks to the scheduler
      AddTasksToScheduler();

      if (!cronScheduler_.Start())
      {
         warp::log::Critical("No enabled services");
         return;
      }

      // Setup all the scans from the configuration
      SetupScans();

      fileMonitor_.Run();

      // Hold the main thread until shutdown
      std::mutex m;
      std::unique_lock lk(m);
      std::condition_variable_any().wait(lk, stopSource_.get_token(), [] { return false; });

      // Clean up all threads before shutting down
      CleanupShutdown();

      warp::log::Info("Run has completed");
   }

   void RemoteScan::ProcessShutdown()
   {
      warp::log::Info("Shutdown request received");

      cronScheduler_.Shutdown();

      stopSource_.request_stop();
   }
}