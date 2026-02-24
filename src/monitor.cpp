#include "monitor.h"

#include "config-reader/config-reader.h"
#include "types.h"

#include <warp/log/log.h>
#include <warp/log/log-utils.h>
#include <warp/utils.h>

#include <algorithm>
#include <cctype>
#include <ranges>
#include <set>

namespace remote_scan
{
   Monitor::Monitor(std::shared_ptr<ConfigReader> configReader)
      : configReader_(configReader)
      , notify_(configReader_, [this](const std::filesystem::path& path) { return this->GetFileImage(path); })
   {
      const auto& ignoreFolders = configReader_->GetIgnoreFolders();
      for (const auto& ignoreFolder : ignoreFolders)
      {
         ignoreFolders_.emplace_back(ignoreFolder.folder);
      }

      auto addExtensionsToSet = [](const auto& extensions, std::unordered_set<std::string>& set) {
         for (const auto& ext : extensions)
         {
            auto lowerExt = warp::ToLower(ext.extension);
            if (!lowerExt.empty() && lowerExt[0] != '.')
            {
               lowerExt = "." + lowerExt;
            }
            set.insert(lowerExt);
         }
      };

      addExtensionsToSet(configReader_->GetImageExtensions(), validImageExtensions_);
      addExtensionsToSet(configReader_->GetValidFileExtensions(), validExtensions_);
   }

   void Monitor::GetTasks(std::vector<warp::Task>& tasks)
   {
      notify_.GetTasks(tasks);
   }

   void Monitor::Run()
   {
      // Create the thread to monitor active scans
      workThread_ = std::jthread([this](std::stop_token stopToken) {
         this->Work(stopToken);
      });
   }

   void Monitor::Shutdown()
   {
      if (workThread_.joinable())
      {
         workThread_.request_stop();

         warp::log::Info("Waiting for monitor thread to finish...");
         workThread_.join();
      }
   }

   void Monitor::Work(std::stop_token stopToken)
   {
      warp::log::Info("Process thread started");

      const auto& config = configReader_->GetRemoteScanConfig();
      const auto settleDelay = std::chrono::seconds(config.secondsBeforeNotify);
      const auto globalDelay = std::chrono::seconds(config.secondsBetweenNotifies);

      while (!stopToken.stop_requested())
      {
         std::optional<ActiveMonitor> monitorToProcess;

         {
            std::unique_lock lock(workLock_);

            // If the queue is empty, wait indefinitely until data arrives or stop is requested.
            if (activeMonitors_.empty())
            {
               workCv_.wait(lock, stopToken, [this] {
                  return !activeMonitors_.empty();
               });
            }

            // Always check stopToken after waking up from a wait.
            if (stopToken.stop_requested()) break;

            // Helper to calculate the earliest possible time we can process the oldest item.
            auto getNextWakeTime = [&]() -> std::chrono::system_clock::time_point {
               if (activeMonitors_.empty()) return std::chrono::system_clock::now();

               auto oldest = std::ranges::min_element(activeMonitors_, {}, &ActiveMonitor::time);
               auto readyAt = oldest->time + settleDelay;
               auto throttleAt = lastNotifyTime_ + globalDelay;
               return (readyAt > throttleAt) ? readyAt : throttleAt;
            };

            auto wakeTime = getNextWakeTime();
            auto now = std::chrono::system_clock::now();

            // If the current time is before our calculated wake time, we must sleep.
            if (now < wakeTime)
            {
               workCv_.wait_until(lock, stopToken, wakeTime, [] { return false; });

               if (stopToken.stop_requested()) break;

               // Re-evaluate conditions after waking up.
               // New items might have been added that pushed the 'wakeTime' further out.
               now = std::chrono::system_clock::now();
               if (activeMonitors_.empty() || now < getNextWakeTime())
               {
                  continue; // Back to the start of the while loop to re-calculate.
               }
            }

            // If we are here, we have passed all throttle and settle checks.
            auto oldestIter = std::ranges::min_element(activeMonitors_, {}, &ActiveMonitor::time);
            if (oldestIter != activeMonitors_.end())
            {
               monitorToProcess = std::move(*oldestIter);
               activeMonitors_.erase(oldestIter);
               lastNotifyTime_ = std::chrono::system_clock::now();
            }

         } // Lock is released here.

         // Perform the actual notification outside of the lock.
         if (monitorToProcess)
         {
            warp::log::Trace("Throttle passed. Notifying for: {}", monitorToProcess->scanName);
            notify_.NotifyMediaServers(*monitorToProcess);
         }
      }

      warp::log::Info("Work thread has exited");
   }

   void Monitor::LogMonitorAdded(std::string_view scanName, const ActiveMonitorPath& monitor)
   {
      std::string effectType;
      switch (monitor.effect)
      {
         case EffectType::RENAME: effectType = "Rename"; break;
         case EffectType::CREATE: effectType = "Create"; break;
         case EffectType::DESTROY: effectType = "Delete"; break;
         default: effectType = "Modify"; break;
      }
      warp::log::Info("{} Scan moved to {} {} {}",
                      warp::GetAnsiText("-->", ANSI_MONITOR_ADDED),
                      warp::GetTag("monitor", scanName),
                      warp::GetTag("effect", effectType),
                      warp::GetTag("media", monitor.displayFullPath.generic_string()));
   }

   void Monitor::AddNewFileMonitor(const FileMonitorData& fileMonitor)
   {
      // Brand new monitor entry
      auto& newMonitor = activeMonitors_.emplace_back();
      newMonitor.scanName = fileMonitor.scanName;
      newMonitor.time = std::chrono::system_clock::now();
      newMonitor.lastPath = fileMonitor.path;

      auto displayFolder = warp::GetDisplayFolder(fileMonitor.path);
      auto displayFullPath = fileMonitor.filename.empty() ? std::move(displayFolder) : displayFolder / fileMonitor.filename;

      auto& newPath = newMonitor.paths.emplace_back(ActiveMonitorPath{
         .path = fileMonitor.path,
         .fileName = fileMonitor.filename,
         .effect = fileMonitor.effect,
         .displayFullPath = std::move(displayFullPath)
      });
      LogMonitorAdded(fileMonitor.scanName, newPath);
   }

   void Monitor::UpdateExistingFileMonitor(const FileMonitorData& fileMonitor, ActiveMonitor& activeMonitor)
   {
      auto now = std::chrono::system_clock::now();
      auto msSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - activeMonitor.time).count();

      activeMonitor.time = now;

      if (fileMonitor.effect == EffectType::MODIFY && msSinceLastUpdate < 500 && fileMonitor.path == activeMonitor.lastPath)
      {
         return;
      }

      activeMonitor.lastPath = fileMonitor.path;

      auto pathIter = std::ranges::find_if(activeMonitor.paths, [&fileMonitor](const auto& monitorPath) {
         return monitorPath.path == fileMonitor.path && monitorPath.fileName == fileMonitor.filename;
      });

      if (pathIter == activeMonitor.paths.end())
      {
         auto displayFolder = warp::GetDisplayFolder(fileMonitor.path);
         auto displayFullPath = fileMonitor.filename.empty() ? std::move(displayFolder) : displayFolder / fileMonitor.filename;

         auto& newPath = activeMonitor.paths.emplace_back(ActiveMonitorPath{
            .path = fileMonitor.path,
            .fileName = fileMonitor.filename,
            .effect = fileMonitor.effect,
            .displayFullPath = std::move(displayFullPath)
         });
         LogMonitorAdded(fileMonitor.scanName, newPath);
      }
   }

   void Monitor::AddFileMonitor(const FileMonitorData& fileMonitor)
   {
      std::unique_lock lock(workLock_);

      auto monitorIter = std::ranges::find_if(activeMonitors_,
          [&fileMonitor](const auto& monitor) { return monitor.scanName == fileMonitor.scanName; });

      if (monitorIter != activeMonitors_.end())
      {
         UpdateExistingFileMonitor(fileMonitor, *monitorIter);
      }
      else
      {
         AddNewFileMonitor(fileMonitor);
      }

      lock.unlock();
      workCv_.notify_one();
   }

   bool Monitor::GetScanPathValid(const std::filesystem::path& path) const
   {
      return !std::ranges::any_of(ignoreFolders_, [&path](const auto& ignore) {
         return std::ranges::any_of(path, [&ignore](const auto& part) {
            return part == ignore;
         });
      });
   }

   bool Monitor::GetFileImage(const std::filesystem::path& filename) const
   {
      auto ext = filename.extension();
      if (ext.empty()) return false;

      std::string lowerExt = warp::ToLower(ext.string());
      return validImageExtensions_.contains(lowerExt);
   }

   bool Monitor::GetFileExtensionValid(const std::filesystem::path& filename) const
   {
      if (validExtensions_.empty()) return true;

      auto ext = filename.extension();
      if (ext.empty()) return false;

      std::string lowerExt = warp::ToLower(ext.string());
      return validExtensions_.contains(lowerExt);
   }

   void Monitor::Process(const FileMonitorData& fileMonitor)
   {
      // Is the scan path valid and this is a destroy or the file being added has a valid extension
      if (GetScanPathValid(fileMonitor.path)
          && (fileMonitor.isDirectory
              || GetFileExtensionValid(fileMonitor.filename)
              || GetFileImage(fileMonitor.filename)))
      {
         AddFileMonitor(fileMonitor);
      }
   }
}