#include "scan.h"

#include "config-reader/config-reader-types.h"
#include "types.h"

#include <warp/log/log.h>
#include <warp/log/log-utils.h>
#include <wtr/watcher.hpp>

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

      for (const auto& pathConfig : config.pathsFromBase)
      {
         auto fullPath = config.basePath / pathConfig.path;
         if (std::filesystem::exists(fullPath))
         {
            pimpl_->activeWatches.emplace_back(fullPath, [fileMonitorFunc, testLogEnabled, scanName = config.name](const wtr::event& e) {
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
                                     warp::GetTag("path", e.path_name.generic_string()));

                  }

                  auto pathName = e.path_name;

                  EffectType effect;
                  switch (e.effect_type)
                  {
                     case wtr::event::effect_type::rename:
                        effect = EffectType::RENAME;
                        if (e.associated)
                        {
                           pathName = e.associated->path_name;
                        }
                        break;
                     case wtr::event::effect_type::create:
                        effect = EffectType::CREATE;
                        break;
                     case wtr::event::effect_type::destroy:
                        effect = EffectType::DESTROY;
                        break;
                     default:
                        effect = EffectType::MODIFY;
                  }

                  bool isDirectory = false;
                  if (e.effect_type != wtr::event::effect_type::destroy)
                  {
                     // If the item still exists, trust the OS check over the event metadata
                     std::error_code ec;
                     if (std::filesystem::is_directory(pathName, ec))
                     {
                        isDirectory = true;
                     }
                  }
                  else
                  {
                     // For DESTROY events, the item is gone. 
                     // We have to trust the watcher's metadata or assume based on extension.
                     isDirectory = (e.path_type == wtr::event::path_type::dir);

                     // Fallback: If it has no extension, it's very likely it was a directory
                     if (!isDirectory && !pathName.has_extension())
                     {
                        isDirectory = true;
                     }
                  }

                  fileMonitorFunc(FileMonitorData{
                     .scanName = scanName,
                     .path = isDirectory ? pathName : pathName.parent_path(),
                     .filename = isDirectory ? "" : pathName.filename(),
                     .isDirectory = isDirectory,
                     .effect = effect
                  });
               }
               return true;
            });

            warp::log::Trace("Started watch for {} on path {}", config.name, fullPath.generic_string());
         }
      }
   }

   void Scan::Shutdown()
   {
      pimpl_->activeWatches.clear();
   }
}