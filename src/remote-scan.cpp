#include "remote-scan.h"

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
   namespace
   {
      constexpr std::string_view APP_NAME("Remote-Scan");
   };

   RemoteScan::RemoteScan(std::shared_ptr<ConfigReader> configReader)
      : scanConfig_(configReader->GetRemoteScanConfig())
   {
      std::vector<warp::ServerConfig> plexConfigs;
      for (const auto& plexServer : configReader->GetPlexServers())
      {
         plexConfigs.emplace_back(warp::ServerConfig{
            .server_name = plexServer.name,
            .url = plexServer.url,
            .api_key = plexServer.apiKey,
            .tracker_url = "",
            .tracker_api_key = "",
            .media_path = ""});
      }

      std::vector<warp::ServerConfig> embyConfigs;
      for (const auto& embyServer : configReader->GetEmbyServers())
      {
         embyConfigs.emplace_back(warp::ServerConfig{
            .server_name = embyServer.name,
            .url = embyServer.url,
            .api_key = embyServer.apiKey,
            .tracker_url = "",
            .tracker_api_key = "",
            .media_path = ""});
      }
      apiManager_ = std::make_unique<warp::ApiManager>(APP_NAME, REMOTE_SCAN_VERSION, plexConfigs, embyConfigs);

      // Sort the scans by longest first
      std::ranges::for_each(scanConfig_.scans, [](auto& scan) {
         std::ranges::sort(scan.paths, [](const auto& a, const auto& b) {
            return a.path.length() > b.path.length();
         });
      });

      for (const auto& ignoreFolder : configReader->GetIgnoreFolders())
      {
         ignoreFolders_.emplace_back(ignoreFolder.folder);
      }

      for (const auto& ext : configReader->GetValidFileExtensions())
      {
         auto lowerExt = warp::ToLower(ext.extension);
         if (!lowerExt.empty() && lowerExt[0] != '.')
         {
            lowerExt = "." + lowerExt;
         }
         validExtensions_.insert(lowerExt);
      }

      if (scanConfig_.dryRun)
      {
         warp::log::Info("[DRY RUN MODE] Remote Scan will not notify media servers of changes");
      }
   }

   void RemoteScan::CleanupShutdown()
   {
      warp::log::Info("Removing directory watches");
      activeWatches_.clear();

      if (monitorThread_ && monitorThread_->joinable())
      {
         warp::log::Info("Waiting for monitor thread to finish...");
         monitorThread_->join();
      }
   }

   void RemoteScan::SetupScans()
   {
      bool testLogEnabled = false;
      if (std::getenv("REMOTE_SCAN_TEST_LOGS")) testLogEnabled = true;

      const auto& config{scanConfig_};
      for (const auto& scan : config.scans)
      {
         for (const auto& pathConfig : scan.paths)
         {
            if (std::filesystem::exists(pathConfig.path))
            {
               activeWatches_.emplace_back(pathConfig.path, [this, testLogEnabled, scanName = scan.name](const wtr::event& e) {
                  if (e.effect_type == wtr::event::effect_type::rename ||
                      e.effect_type == wtr::event::effect_type::create ||
                      e.effect_type == wtr::event::effect_type::modify ||
                      e.effect_type == wtr::event::effect_type::destroy)
                  {
                     if (testLogEnabled)
                     {
                        warp::log::Info("[TEST] {} {} {}",
                                        warp::GetTag("effect", wtr::to<std::string>(e.effect_type)),
                                        warp::GetTag("type", wtr::to<std::string>(e.path_type)),
                                        warp::GetTag("path", e.path_name.string()));

                     }
                     this->ProcessFileUpdate(scanName,
                                             e.path_name.parent_path().string(),
                                             e.path_name.filename().string(),
                                             e.path_type == wtr::event::path_type::dir,
                                             e.effect_type == wtr::event::effect_type::destroy);
                  }
                  return true;
               });

               warp::log::Trace("Started watch for {} on path {}", scan.name, pathConfig.path);
            }
         }
      }
   }

   void RemoteScan::Run()
   {
      // Setup all the scans from the configuration
      SetupScans();

      // Create the thread to monitor active scans
      monitorThread_ = std::make_unique<std::jthread>([this](std::stop_token stopToken) {
         this->Monitor(stopToken);
      });

      std::mutex m;
      std::unique_lock lk(m);
      std::condition_variable_any().wait(lk, stopSource_.get_token(), [] { return false; });

      // Clean up all threads before shutting down
      CleanupShutdown();

      warp::log::Info("Run has completed");
   }

   void RemoteScan::LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library)
   {
      warp::log::Warning("{}({}) {} not found ... Skipped notify",
                         serverType,
                         library.server,
                         warp::GetTag("library", library.library));
   }

   void RemoteScan::LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library)
   {
      warp::log::Warning("{}({}) server not available ... Skipped notify for {}",
                         serverType,
                         library.server,
                         warp::GetTag("library", library.library));
   }

   void RemoteScan::NotifyMediaServers(const ActiveMonitor& monitor)
   {
      const auto& scanConfig{scanConfig_};

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
                            scanConfig_.dryRun ? "[DRY RUN] " : "",
                            warp::GetAnsiText(">>>", ANSI_MONITOR_PROCESSED),
                            warp::GetTag("monitor", monitor.scanName),
                            syncServers,
                            warp::GetTag("folder", path.displayFolder));
         }
      }
      else
      {
         warp::log::Warning("No Servers Notified for monitor {}", monitor.scanName);
      }
   }

   void RemoteScan::Monitor(std::stop_token stopToken)
   {
      warp::log::Info("Monitor thread started");

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

            if (secondsSinceLastGlobalNotify >= scanConfig_.secondsBetweenNotifies)
            {
               auto oldestIter = std::ranges::min_element(activeMonitors_, {}, &ActiveMonitor::time);

               auto secondsSinceEvent = std::chrono::duration_cast<std::chrono::seconds>(
                   currentTime - oldestIter->time).count();

               if (secondsSinceEvent >= scanConfig_.secondsBeforeNotify)
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

   void RemoteScan::LogMonitorAdded(std::string_view scanName, std::string_view displayFolder)
   {
      warp::log::Info("{} Scan moved to {} {}",
                      warp::GetAnsiText("-->", ANSI_MONITOR_ADDED),
                      warp::GetTag("monitor", scanName),
                      warp::GetTag("folder", displayFolder));
   }

   void RemoteScan::AddFileMonitor(std::string_view scanName, std::string_view path, bool destroy)
   {
      std::unique_lock lock(monitorLock_);

      auto monitorIter = std::ranges::find_if(activeMonitors_,
          [scanName](const auto& monitor) { return monitor.scanName == scanName; });

      if (monitorIter != activeMonitors_.end())
      {
         auto now = std::chrono::system_clock::now();
         auto msSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
             now - monitorIter->time).count();

         monitorIter->time = now;
         monitorIter->destroy = monitorIter->destroy || destroy;

         if (!destroy && msSinceLastUpdate < 500 && path == monitorIter->lastPath)
         {
            return;
         }

         monitorIter->lastPath = path;

         auto pathIter = std::ranges::find_if(monitorIter->paths,
             [path](const auto& monitorPath) { return monitorPath.path == path; });

         if (pathIter == monitorIter->paths.end())
         {
            auto& newPath = monitorIter->paths.emplace_back(std::string(path), warp::GetDisplayFolder(path));
            LogMonitorAdded(scanName, newPath.displayFolder);
         }
      }
      else
      {
         // Brand new monitor entry
         auto& newMonitor = activeMonitors_.emplace_back();
         newMonitor.scanName = scanName;
         newMonitor.time = std::chrono::system_clock::now();
         newMonitor.destroy = destroy;
         newMonitor.lastPath = path;

         auto& newPath = newMonitor.paths.emplace_back(std::string(path), warp::GetDisplayFolder(path));
         LogMonitorAdded(scanName, newPath.displayFolder);
      }

      lock.unlock();
      monitorCv_.notify_one();
   }

   bool RemoteScan::GetScanPathValid(std::string_view path)
   {
      return !std::ranges::any_of(ignoreFolders_, [path](const auto& ignore) {
         return path.find(ignore) != std::string_view::npos;
      });
   }

   bool RemoteScan::GetFileExtensionValid(std::string_view filename)
   {
      if (validExtensions_.empty()) return true;

      const auto dotPos = filename.find_last_of('.');
      if (dotPos == std::string_view::npos) return false;

      auto extView = filename.substr(dotPos);
      std::string lowerExt(extView);
      std::ranges::transform(lowerExt, lowerExt.begin(), [](unsigned char c) {
         return static_cast<char>(std::tolower(c));
      });

      return validExtensions_.contains(lowerExt);
   }

   void RemoteScan::ProcessFileUpdate(std::string_view scanName,
                                      std::string_view path,
                                      std::string_view filename,
                                      bool isFolder,
                                      bool destroy)
   {
      // Is the scan path valid and this is a destroy or the file being added has a valid extension
      if (GetScanPathValid(path) && (destroy || isFolder || GetFileExtensionValid(filename)))
      {
         AddFileMonitor(scanName, path, destroy);
      }
   }

   void RemoteScan::ProcessShutdown()
   {
      warp::log::Info("Shutdown request received");

      if (monitorThread_ && monitorThread_->joinable())
      {
         monitorThread_->request_stop();
      }
      stopSource_.request_stop();
   }
}