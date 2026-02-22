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
      warp::ApiManagerConfig apiManagerConfig;
      for (const auto& plexServer : configReader_->GetPlexServers())
      {
         apiManagerConfig.plexConfig.servers.emplace_back(warp::ServerConfig{
            .serverName = plexServer.name,
            .url = plexServer.url,
            .apiKey = plexServer.apiKey,
            .trackerUrl = "",
            .trackerApiKey = "",
            .mediaPath = ""});
      }

      for (const auto& embyServer : configReader_->GetEmbyServers())
      {
         apiManagerConfig.embyConfig.servers.emplace_back(warp::ServerConfig{
            .serverName = embyServer.name,
            .url = embyServer.url,
            .apiKey = embyServer.apiKey,
            .trackerUrl = "",
            .trackerApiKey = "",
            .mediaPath = ""});
      }

      apiManager_ = std::make_unique<warp::ApiManager>(REMOTE_SCAN_NAME, REMOTE_SCAN_VERSION, apiManagerConfig);

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
      addExtensionsToSet(configReader_->GetStripExtensions(), stripExtensions_);
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

   std::filesystem::path FileMonitor::GetBasePath(std::string_view scanName) const
   {
      const auto& scans = configReader_->GetRemoteScanConfig().scans;
      auto scanIter{std::ranges::find_if(scans, [scanName](const auto& scan) { return scan.name == scanName; })};
      if (scanIter != scans.end())
      {
         return scanIter->basePath;
      }
      return {};
   }

   bool FileMonitor::NotifyPlex(const ActiveMonitor& monitor, const ScanLibraryConfig& library)
   {
      auto* plexApi = apiManager_->GetPlexApi(library.server);
      if (!plexApi || plexApi->GetValid() == false)
      {
         LogServerNotAvailable(warp::GetFormattedPlex(), library);
         return false;
      }

      auto libraryId{plexApi->GetLibraryId(library.library)};
      if (!libraryId)
      {
         LogServerLibraryIssue(warp::GetFormattedPlex(), library);
         return false;
      }

      auto basePath = GetBasePath(monitor.scanName);
      if (basePath.empty()) return false;

      std::vector<std::filesystem::path> notifiedPaths;
      for (const auto& path : monitor.paths)
      {
         if (std::ranges::find(notifiedPaths, path.path) == notifiedPaths.end())
         {
            plexApi->SetLibraryScanPath(*libraryId, warp::ReplaceMediaPath(path.path, basePath, library.mediaPath));
            notifiedPaths.emplace_back(path.path);
         }
      }
      return true;
   }

   bool FileMonitor::NotifyEmby(const ActiveMonitor& monitor, const ScanLibraryConfig& library)
   {
      auto* embyApi = apiManager_->GetEmbyApi(library.server);
      if (!embyApi || embyApi->GetValid() == false)
      {
         LogServerNotAvailable(warp::GetFormattedEmby(), library);
         return false;
      }

      auto basePath = GetBasePath(monitor.scanName);
      if (basePath.empty()) return false;

      bool imagesUpdated = false;
      std::vector<warp::EmbyMediaUpdate> mediaUpdates;
      for (const auto& path : monitor.paths)
      {
         if (!path.fileName.empty() && GetFileImage(path.fileName))
         {
            imagesUpdated = true;

            // If image updates were detected we are going to do a library scan so just break out of the loop
            break;
         }

         warp::EmbyUpdateType embyUpdateType;
         switch (path.effect)
         {
            case EffectType::CREATE: embyUpdateType = warp::EmbyUpdateType::CREATED; break;
            case EffectType::DESTROY: embyUpdateType = warp::EmbyUpdateType::DELETED; break;
            default: embyUpdateType = warp::EmbyUpdateType::MODIFIED; break;

               break;
         }

         auto filePathToUse = path.fileName.empty() ? path.path : path.path / path.fileName;
         mediaUpdates.emplace_back(warp::EmbyMediaUpdate{
            .path = warp::ReplaceMediaPath(filePathToUse, basePath, library.mediaPath),
            .type = embyUpdateType
         });
      }

      // If images were updated we need to do a library scan to ensure they are processed, media updates alone won't trigger image processing
      if (imagesUpdated)
      {
         auto libraryId{embyApi->GetLibraryId(library.library)};
         if (!libraryId)
         {
            LogServerLibraryIssue(warp::GetFormattedPlex(), library);
            return false;
         }
         embyApi->SetLibraryScan(*libraryId);
      }
      else
      {
         // else command a media scan for any media file updates, this will ensure the media is refreshed and any needed metadata updates are done, but won't trigger a full library scan
         if (!mediaUpdates.empty())
            embyApi->SetMediaScan(mediaUpdates);
      }

      return true;
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

      for (const auto& plexLibrary : scan.plexLibraries)
      {
         if (NotifyPlex(monitor, plexLibrary))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedPlex(), plexLibrary.server);
         }
      }

      for (const auto& embyLibrary : scan.embyLibraries)
      {
         if (NotifyEmby(monitor, embyLibrary))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedApiName(warp::ApiType::EMBY), embyLibrary.server);
         }
      }

      if (syncServers.empty() == false)
      {
         for (auto& path : monitor.paths)
         {
            auto pathToLog = path.fileName.empty() ? path.displayFolder : path.displayFolder / path.fileName;
            warp::log::Info("{}{} Moved {} to target {} {}",
                            scanConfig.dryRun ? "[DRY RUN] " : "",
                            warp::GetAnsiText(">>>", ANSI_MONITOR_PROCESSED),
                            warp::GetTag("monitor", monitor.scanName),
                            syncServers,
                            warp::GetTag("media", pathToLog.generic_string()));
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

   void FileMonitor::LogMonitorAdded(std::string_view scanName, const ActiveMonitorPath& monitor)
   {
      auto pathToLog = monitor.fileName.empty() ? monitor.displayFolder : monitor.displayFolder / monitor.fileName;

      std::string effectType;
      switch (monitor.effect)
      {
         case EffectType::CREATE: effectType = "Created"; break;
         case EffectType::DESTROY: effectType = "Deleted"; break;
         default: effectType = "Modified"; break;
      }
      warp::log::Info("{} Scan moved to {} {} {}",
                      warp::GetAnsiText("-->", ANSI_MONITOR_ADDED),
                      warp::GetTag("monitor", scanName),
                      warp::GetTag("effect", effectType),
                      warp::GetTag("media", pathToLog.generic_string()));
   }

   std::filesystem::path FileMonitor::GetStrippedFileName(const std::filesystem::path& originalPath)
   {
      std::string pathStr = warp::ToLower(originalPath.string());
      for (const std::string& ext : stripExtensions_)
      {
         // ext is already lowercase (e.g., ".unmanic.part")
         if (pathStr.length() >= ext.length() &&
             pathStr.compare(pathStr.length() - ext.length(), ext.length(), ext) == 0)
         {
            return std::filesystem::path(pathStr.substr(0, pathStr.length() - ext.length()));
         }
      }

      return originalPath;
   }

   void FileMonitor::AddFileMonitor(const FileMonitorData& fileMonitor)
   {
      std::unique_lock lock(monitorLock_);

      auto monitorIter = std::ranges::find_if(activeMonitors_,
          [&fileMonitor](const auto& monitor) { return monitor.scanName == fileMonitor.scanName; });

      if (monitorIter != activeMonitors_.end())
      {
         auto now = std::chrono::system_clock::now();
         auto msSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - monitorIter->time).count();

         monitorIter->time = now;

         if (fileMonitor.effect == EffectType::MODIFY && msSinceLastUpdate < 500 && fileMonitor.path == monitorIter->lastPath)
         {
            return;
         }

         monitorIter->lastPath = fileMonitor.path;

         auto pathIter = std::ranges::find_if(monitorIter->paths,
             [&fileMonitor](const auto& monitorPath) { return monitorPath.path == fileMonitor.path && monitorPath.fileName == fileMonitor.filename; });

         if (pathIter == monitorIter->paths.end())
         {
            auto& newPath = monitorIter->paths.emplace_back(ActiveMonitorPath{
               .path = fileMonitor.path,
               .fileName = GetStrippedFileName(fileMonitor.filename),
               .effect = fileMonitor.effect,
               .displayFolder = warp::GetDisplayFolder(fileMonitor.path)
            });
            LogMonitorAdded(fileMonitor.scanName, newPath);
         }
      }
      else
      {
         // Brand new monitor entry
         auto& newMonitor = activeMonitors_.emplace_back();
         newMonitor.scanName = fileMonitor.scanName;
         newMonitor.time = std::chrono::system_clock::now();
         newMonitor.lastPath = fileMonitor.path;

         auto& newPath = newMonitor.paths.emplace_back(ActiveMonitorPath{
            .path = fileMonitor.path,
            .fileName = GetStrippedFileName(fileMonitor.filename),
            .effect = fileMonitor.effect,
            .displayFolder = warp::GetDisplayFolder(fileMonitor.path)
         });
         LogMonitorAdded(fileMonitor.scanName, newPath);
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

   bool FileMonitor::GetFileImage(const std::filesystem::path& filename)
   {
      auto ext = filename.extension();
      if (ext.empty()) return false;

      std::string lowerExt = warp::ToLower(ext.string());
      return validImageExtensions_.contains(lowerExt);
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
          && (fileMonitor.effect == EffectType::DESTROY
              || fileMonitor.isDirectory
              || GetFileExtensionValid(fileMonitor.filename)
              || GetFileImage(fileMonitor.filename)))
      {
         AddFileMonitor(fileMonitor);
      }
   }
}