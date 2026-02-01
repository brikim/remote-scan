#include "file-monitor.h"

#include "config-reader/config-reader.h"
#include "types.h"
#include "version.h"

#include <warp/log/log.h>
#include <warp/log/log-utils.h>
#include <warp/utils.h>

#include <algorithm>
#include <cctype>
#include <ranges>
#include <set>

namespace remote_scan
{
   FileMonitor::FileMonitor(std::shared_ptr<ConfigReader> configReader)
      : configReader_(configReader)
   {
      std::vector<warp::ServerConfig> plexConfigs;
      for (const auto& plexServer : configReader_->GetPlexServers())
      {
         plexConfigs.emplace_back(warp::ServerConfig{
            .serverName = plexServer.name,
            .url = plexServer.url,
            .apiKey = plexServer.apiKey,
            .trackerUrl = "",
            .trackerApiKey = "",
            .mediaPath = ""});
      }

      std::vector<warp::ServerConfig> embyConfigs;
      for (const auto& embyServer : configReader_->GetEmbyServers())
      {
         embyConfigs.emplace_back(warp::ServerConfig{
            .serverName = embyServer.name,
            .url = embyServer.url,
            .apiKey = embyServer.apiKey,
            .trackerUrl = "",
            .trackerApiKey = "",
            .mediaPath = ""});
      }
      apiManager_ = std::make_unique<warp::ApiManager>(REMOTE_SCAN_NAME, REMOTE_SCAN_VERSION, plexConfigs, embyConfigs);

      const auto& ignoreFolders = configReader_->GetIgnoreFolders();
      for (const auto& ignoreFolder : ignoreFolders)
      {
         ignoreFolders_.emplace_back(ignoreFolder.folder);
      }

      const auto& validFileExtensions = configReader_->GetValidFileExtensions();
      for (const auto& ext : validFileExtensions)
      {
         auto lowerExt = warp::ToLower(ext.extension);
         if (!lowerExt.empty() && lowerExt[0] != '.')
         {
            lowerExt = "." + lowerExt;
         }
         validExtensions_.insert(lowerExt);
      }
   }

   void FileMonitor::GetTasks(std::vector<warp::Task>& tasks)
   {
      apiManager_->GetTasks(tasks);
   }

   void FileMonitor::Run()
   {
      // Create the thread to monitor active scans
      monitorThread_ = std::jthread([this](std::stop_token stopToken) {
         this->Monitor(stopToken);
      });
   }

   void FileMonitor::Shutdown()
   {
      if (monitorThread_.joinable())
      {
         monitorThread_.request_stop();

         warp::log::Info("Waiting for monitor thread to finish...");
         monitorThread_.join();
      }
   }

   void FileMonitor::LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library)
   {
      warp::log::Warning("{}({}) {} not found ... Skipped notify",
                         serverType,
                         library.server,
                         warp::GetTag("library", library.library));
   }

   void FileMonitor::LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library)
   {
      warp::log::Warning("{}({}) server not available ... Skipped notify for {}",
                         serverType,
                         library.server,
                         warp::GetTag("library", library.library));
   }

   void FileMonitor::NotifyMediaServers(const ActiveMonitor& monitor)
   {
      const auto& scanConfig = configReader_->GetRemoteScanConfig();

      auto scanIter{std::ranges::find_if(scanConfig.scans, [&monitor](const auto& scan) { return scan.name == monitor.scanName; })};
      if (scanIter == scanConfig.scans.end())
      {
         warp::log::Error("Attempting to notify media servers but {} not found!", monitor.scanName);
         return;
      }

      const auto& scan{*scanIter};
      std::string syncServers;

      auto notifyServer = [this](auto* api, auto type, const auto& library) {
         if (api->GetValid())
         {
            auto libraryId{api->GetLibraryId(library.library)};
            if (libraryId)
            {
               api->SetLibraryScan(*libraryId);
               return true;
            }
            else
            {
               LogServerLibraryIssue(warp::GetFormattedApiName(type), library);
            }
         }
         else
         {
            LogServerNotAvailable(warp::GetFormattedApiName(type), library);
         }
         return false;
      };

      for (const auto& plexLibrary : scan.plexLibraries)
      {
         if (notifyServer(apiManager_->GetPlexApi(plexLibrary.server), warp::ApiType::PLEX, plexLibrary))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedApiName(warp::ApiType::PLEX), plexLibrary.server);
         }
      }

      for (const auto& embyLibrary : scan.embyLibraries)
      {
         if (notifyServer(apiManager_->GetEmbyApi(embyLibrary.server), warp::ApiType::EMBY, embyLibrary))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedApiName(warp::ApiType::EMBY), embyLibrary.server);
         }
      }

      if (syncServers.empty() == false)
      {
         for (auto& path : monitor.paths)
         {
            warp::log::Info("{}{} Moved {} to target {} {}",
                            scanConfig.dryRun ? "[DRY RUN] " : "",
                            warp::GetAnsiText(">>>", ANSI_MONITOR_PROCESSED),
                            warp::GetTag("monitor", monitor.scanName),
                            syncServers,
                            warp::GetTag("folder", path.displayFolder.generic_string()));
         }
      }
      else
      {
         warp::log::Warning("No Servers Notified for monitor {}", monitor.scanName);
      }
   }

   void FileMonitor::Monitor(std::stop_token stopToken)
   {
      warp::log::Info("Monitor thread started");

      const int32_t secondsBetweenNotifies = configReader_->GetRemoteScanConfig().secondsBetweenNotifies;
      const int32_t secondsBeforeNotify = configReader_->GetRemoteScanConfig().secondsBeforeNotify;

      while (!stopToken.stop_requested())
      {
         std::optional<ActiveMonitor> monitorToProcess;

         // Scoped section around lock
         {
            std::unique_lock lock(monitorLock_);

            monitorCv_.wait(lock, stopToken, [this] {
               return !activeMonitors_.empty();
            });

            if (stopToken.stop_requested()) break;

            auto currentTime = std::chrono::system_clock::now();
            auto secondsSinceLastGlobalNotify = std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - lastNotifyTime_).count();

            if (secondsSinceLastGlobalNotify >= secondsBetweenNotifies)
            {
               if (activeMonitors_.empty()) continue;

               auto oldestIter = std::ranges::min_element(activeMonitors_, {}, &ActiveMonitor::time);

               auto secondsSinceEvent = std::chrono::duration_cast<std::chrono::seconds>(
                   currentTime - oldestIter->time).count();

               if (secondsSinceEvent >= secondsBeforeNotify)
               {
                  monitorToProcess = std::move(*oldestIter);
                  activeMonitors_.erase(oldestIter);
                  lastNotifyTime_ = currentTime;
               }
            }
         } // Release the lock

         if (monitorToProcess)
         {
            warp::log::Trace("Throttle passed. Notifying for: {}", monitorToProcess->scanName);
            NotifyMediaServers(*monitorToProcess);
         }
         else
         {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
         }
      }

      warp::log::Info("Monitor thread has exited");
   }

   void FileMonitor::LogMonitorAdded(std::string_view scanName, const std::filesystem::path& displayFolder)
   {
      warp::log::Info("{} Scan moved to {} {}",
                      warp::GetAnsiText("-->", ANSI_MONITOR_ADDED),
                      warp::GetTag("monitor", scanName),
                      warp::GetTag("folder", displayFolder.generic_string()));
   }

   void FileMonitor::AddFileMonitor(const FileMonitorData& fileMonitor)
   {
      std::unique_lock lock(monitorLock_);

      auto monitorIter = std::ranges::find_if(activeMonitors_,
          [&fileMonitor](const auto& monitor) { return monitor.scanName == fileMonitor.scanName; });

      if (monitorIter != activeMonitors_.end())
      {
         auto now = std::chrono::system_clock::now();
         auto msSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
             now - monitorIter->time).count();

         monitorIter->time = now;
         monitorIter->destroy = monitorIter->destroy || fileMonitor.destroy;

         if (!fileMonitor.destroy && msSinceLastUpdate < 500 && fileMonitor.path == monitorIter->lastPath)
         {
            return;
         }

         monitorIter->lastPath = fileMonitor.path;

         auto pathIter = std::ranges::find_if(monitorIter->paths,
             [&fileMonitor](const auto& monitorPath) { return monitorPath.path == fileMonitor.path; });

         if (pathIter == monitorIter->paths.end())
         {
            auto& newPath = monitorIter->paths.emplace_back(fileMonitor.path, warp::GetDisplayFolder(fileMonitor.path));
            LogMonitorAdded(fileMonitor.scanName, newPath.displayFolder);
         }
      }
      else
      {
         // Brand new monitor entry
         auto& newMonitor = activeMonitors_.emplace_back();
         newMonitor.scanName = fileMonitor.scanName;
         newMonitor.time = std::chrono::system_clock::now();
         newMonitor.destroy = fileMonitor.destroy;
         newMonitor.lastPath = fileMonitor.path;

         auto& newPath = newMonitor.paths.emplace_back(fileMonitor.path, warp::GetDisplayFolder(fileMonitor.path));
         LogMonitorAdded(fileMonitor.scanName, newPath.displayFolder);
      }

      lock.unlock();
      monitorCv_.notify_one();
   }

   bool FileMonitor::GetScanPathValid(const std::filesystem::path& path)
   {
      return !std::ranges::any_of(ignoreFolders_, [&path](const auto& ignore) {
         return std::ranges::any_of(path, [&ignore](const auto& part) {
            return part == ignore;
         });
      });
   }

   bool FileMonitor::GetFileExtensionValid(const std::filesystem::path& filename)
   {
      if (validExtensions_.empty()) return true;

      auto ext = filename.extension();
      if (ext.empty()) return false;

      std::string lowerExt = warp::ToLower(ext.string());
      return validExtensions_.contains(lowerExt);
   }

   void FileMonitor::Process(const FileMonitorData& fileMonitor)
   {
      // Is the scan path valid and this is a destroy or the file being added has a valid extension
      if (GetScanPathValid(fileMonitor.path)
          && (fileMonitor.destroy || fileMonitor.isDirectory || GetFileExtensionValid(fileMonitor.filename)))
      {
         AddFileMonitor(fileMonitor);
      }
   }
}