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

      bool GetIsDirectory(enum wtr::event::effect_type effectType,
                          enum wtr::event::path_type pathType,
                          const std::filesystem::path& path)
      {
         bool isDirectory = false;
         if (effectType != wtr::event::effect_type::destroy)
         {
            // If the item still exists, trust the OS check over the event metadata
            std::error_code ec;
            if (std::filesystem::is_directory(path, ec))
            {
               return true;
            }
         }
         else
         {
            if (pathType == wtr::event::path_type::dir || !path.has_extension())
            {
               return true;
            }
         }
         return false;
      }

      void ProcessRenameEvent(const wtr::event& e,
                              const std::string& scanName,
                              bool testLogEnabled,
                              const std::function<void(const FileMonitorData& fileMonitor)>& fileMonitorFunc)
      {
         if (e.associated)
         {
            // If a rename event has occured and the associated field is set,
            // we need to send a DESTROY for the old path and a CREATE for the new path.
            auto oldIsDirectory = GetIsDirectory(wtr::event::effect_type::destroy, e.path_type, e.path_name);
            fileMonitorFunc(FileMonitorData{
                     .scanName = scanName,
                     .path = oldIsDirectory ? e.path_name : e.path_name.parent_path(),
                     .filename = oldIsDirectory ? "" : e.path_name.filename(),
                     .isDirectory = oldIsDirectory,
                     .effect = EffectType::DESTROY
             });

            auto newIsDirectory = GetIsDirectory(wtr::event::effect_type::create, e.path_type, e.associated->path_name);
            fileMonitorFunc(FileMonitorData{
                     .scanName = scanName,
                     .path = newIsDirectory ? e.associated->path_name : e.associated->path_name.parent_path(),
                     .filename = newIsDirectory ? "" : e.associated->path_name.filename(),
                     .isDirectory = newIsDirectory,
                     .effect = EffectType::CREATE
             });

         }
         else
         {
            auto isDirectory = GetIsDirectory(e.effect_type, e.path_type, e.path_name);
            fileMonitorFunc(FileMonitorData{
                     .scanName = scanName,
                     .path = isDirectory ? e.path_name : e.path_name.parent_path(),
                     .filename = isDirectory ? "" : e.path_name.filename(),
                     .isDirectory = isDirectory,
                     .effect = EffectType::CREATE
             });
         }
      }

      bool ProcessEvent(const wtr::event& e,
                        const std::string& scanName,
                        bool testLogEnabled,
                        const std::function<void(const FileMonitorData& fileMonitor)>& fileMonitorFunc)
      {
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

            EffectType effectType;
            switch (e.effect_type)
            {
               case wtr::event::effect_type::rename:
                  effectType = EffectType::RENAME;
                  break;
               case wtr::event::effect_type::create:
                  effectType = EffectType::CREATE;
                  break;
               case wtr::event::effect_type::destroy:
                  effectType = EffectType::DESTROY;
                  break;
               default:
                  effectType = EffectType::MODIFY;
                  break;
            }

            if (effectType == EffectType::RENAME)
            {
               ProcessRenameEvent(e, scanName, testLogEnabled, fileMonitorFunc);
            }
            else
            {
               bool isDirectory = GetIsDirectory(e.effect_type, e.path_type, e.path_name);

               fileMonitorFunc(FileMonitorData{
                  .scanName = scanName,
                  .path = isDirectory ? e.path_name : e.path_name.parent_path(),
                  .filename = isDirectory ? "" : e.path_name.filename(),
                  .isDirectory = isDirectory,
                  .effect = effectType
               });
            }
         }
         return true;
      }
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
            auto processEventFunc = [this, scanName = config.name, testLogEnabled, fileMonitorFunc](const wtr::event& e) { return pimpl_->ProcessEvent(e, scanName, testLogEnabled, fileMonitorFunc); };
            pimpl_->activeWatches.emplace_back(fullPath, processEventFunc);

            warp::log::Trace("Started watch for {} on path {}", config.name, fullPath.generic_string());
         }
      }
   }

   void Scan::Shutdown()
   {
      pimpl_->activeWatches.clear();
   }
}