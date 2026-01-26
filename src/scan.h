#pragma once

#include <functional>
#include <memory>

namespace remote_scan
{
   struct ScanConfig;
   struct FileMonitorData;

   class ScanImpl;

   class Scan
   {
   public:
      explicit Scan(const ScanConfig& config, const std::function<void(const FileMonitorData& fileMonitor)>& fileMonitorFunc);
      virtual ~Scan();

      void Shutdown();

   private:
      void Init(const ScanConfig& config, const std::function<void(const FileMonitorData& fileMonitor)>& fileMonitorFunc);

      std::unique_ptr<ScanImpl> pimpl_;
   };
}