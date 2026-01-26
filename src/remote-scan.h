#pragma once

#include "config-reader/config-reader-types.h"
#include "file-monitor.h"
#include "scan.h"

#include <warp/scheduler/cron-scheduler.h>

#include <memory>
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
      void ProcessShutdown();

   private:
      void AddTasksToScheduler();
      void SetupScans();
      void CleanupShutdown();

      warp::CronScheduler cronScheduler_;
      FileMonitor fileMonitor_;
      RemoteScanConfig scanConfig_;

      std::vector<std::unique_ptr<Scan>> scans_;

      std::stop_source stopSource_;
   };
}