#include "scan.h"

#include "config-reader/config-reader-types.h"
#include "types.h"

#include <warp/log/log.h>
#include <warp/log/log-utils.h>
#include <watcher/watcher.hpp>

#include <cstdlib>
#include <list>

namespace remote_scan
{
   class ScanImpl
   {
   public:
      std::list<wtr::watch> activeWatches;
   };

   Scan::Scan(const ScanConfig& config, const std::function<void(const FileMonitorData& fileMonitor)>& fileMonitorFunc)
      : pimpl_(std::make_unique<ScanImpl>())
   {
      Init(config, fileMonitorFunc);
   }

   Scan::~Scan() = default;

   void Scan::Init(const ScanConfig& config, const std::function<void(const FileMonitorData& fileMonitor)>& fileMonitorFunc)
   {
      bool testLogEnabled = false;
      if (std::getenv("REMOTE_SCAN_TEST_LOGS")) testLogEnabled = true;

      for (const auto& pathConfig : config.paths)
      {
         if (std::filesystem::exists(pathConfig.path))
         {
            pimpl_->activeWatches.emplace_back(pathConfig.path, [fileMonitorFunc, testLogEnabled, scanName = config.name](const wtr::event& e) {
               if ((e.effect_type == wtr::event::effect_type::rename ||
                    e.effect_type == wtr::event::effect_type::create ||
                    e.effect_type == wtr::event::effect_type::modify ||
                    e.effect_type == wtr::event::effect_type::destroy) &&
                   e.path_type != wtr::event::path_type::watcher)
               {
                  if (testLogEnabled)
                  {
                     warp::log::Info("[TEST] {} {} {}",
                                     warp::GetTag("effect", wtr::to<std::string>(e.effect_type)),
                                     warp::GetTag("type", wtr::to<std::string>(e.path_type)),
                                     warp::GetTag("path", e.path_name.string()));

                  }

                  bool isDirectory = e.path_type == wtr::event::path_type::dir;
                  fileMonitorFunc(FileMonitorData{
                     .scanName = scanName,
                     .path = isDirectory ? e.path_name : e.path_name.parent_path(),
                     .filename = isDirectory ? "" : e.path_name.filename(),
                     .isDirectory = isDirectory,
                     .destroy = e.effect_type == wtr::event::effect_type::destroy
                  });
               }
               return true;
            });

            warp::log::Trace("Started watch for {} on path {}", config.name, pathConfig.path);
         }
      }
   }

   void Scan::Shutdown()
   {
      pimpl_->activeWatches.clear();
   }
}