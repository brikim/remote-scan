#include "remote-scan.h"

#include "api/api-plex.h"
#include "config-reader/config-reader.h"
#include "types.h"

#include <warp/log.h>
#include <warp/log-utils.h>
#include <warp/utils.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <ranges>

namespace remote_scan
{
   RemoteScan::RemoteScan(std::shared_ptr<ConfigReader> configReader)
      : configReader_(configReader)
      , apiManager_(configReader)
   {
      for (const auto& ext : configReader_->GetValidFileExtensions())
      {
         std::string lowerExt = warp::ToLower(ext.extension);
         if (!lowerExt.empty() && lowerExt[0] != '.')
         {
            lowerExt = "." + lowerExt;
         }
         validExtensions_.insert(lowerExt);
      }

      if (configReader_->GetRemoteScanConfig().dryRun)
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
      const auto& config{configReader_->GetRemoteScanConfig()};
      for (const auto& scan : config.scans)
      {
         for (const auto& pathConfig : scan.paths)
         {
            if (std::filesystem::exists(pathConfig.path))
            {
               activeWatches_.emplace_back(pathConfig.path, [this, scanName = scan.name](const wtr::event& e) {
                  if (e.effect_type == wtr::event::effect_type::rename ||
                      e.effect_type == wtr::event::effect_type::create ||
                      e.effect_type == wtr::event::effect_type::modify ||
                      e.effect_type == wtr::event::effect_type::destroy)
                  {
                     std::filesystem::path p(e.path_name);
                     this->ProcessFileUpdate(scanName, p.parent_path().string(), p.filename().string());
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

   bool RemoteScan::NotifyServer(warp::ApiType type, const ScanLibraryConfig& library)
   {
      if (configReader_->GetRemoteScanConfig().dryRun) return true;

      auto* api{apiManager_.GetApi(type, library.server)};
      if (api != nullptr)
      {
         if (api->GetValid())
         {
            auto libraryId{api->GetLibraryId(library.library)};
            if (libraryId.has_value())
            {
               api->SetLibraryScan(libraryId.value());
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
      }
      else
      {
         warp::log::Warning("Notify Server called but no valid API found for {}({})", warp::GetFormattedApiName(type), library.server);
      }
      return false;
   }

   void RemoteScan::NotifyMediaServers(const ActiveMonitor& monitor)
   {
      const auto& scanConfig{configReader_->GetRemoteScanConfig()};
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
         if (NotifyServer(warp::ApiType::PLEX, plexLibrary))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedApiName(warp::ApiType::PLEX), plexLibrary.server);
         }
      }

      for (const auto& embyLibrary : scan.embyLibraries)
      {
         if (NotifyServer(warp::ApiType::EMBY, embyLibrary))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedApiName(warp::ApiType::EMBY), embyLibrary.server);
         }
      }

      if (syncServers.empty() == false)
      {
         for (auto& path : monitor.paths)
         {
            warp::log::Info("{}{} Monitor moved to target {} {}",
                            configReader_->GetRemoteScanConfig().dryRun ? "[DRY RUN] " : "",
                            warp::GetAnsiText(">>>", ANSI_MONITOR_PROCESSED),
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

         {
            std::unique_lock lock(monitorLock_);

            monitorCv_.wait(lock, stopToken, [this] {
               return !activeMonitors_.empty();
            });

            if (stopToken.stop_requested()) break;

            auto currentTime = std::chrono::system_clock::now();
            auto secondsSinceLastGlobalNotify = std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - lastNotifyTime_).count();

            if (secondsSinceLastGlobalNotify >= configReader_->GetRemoteScanConfig().secondsBetweenNotifies)
            {
               auto oldestIter = std::ranges::min_element(activeMonitors_, {}, &ActiveMonitor::time);

               auto secondsSinceEvent = std::chrono::duration_cast<std::chrono::seconds>(
                   currentTime - oldestIter->time).count();

               if (secondsSinceEvent >= configReader_->GetRemoteScanConfig().secondsBeforeNotify)
               {
                  monitorToProcess = std::move(*oldestIter);
                  activeMonitors_.erase(oldestIter);
                  lastNotifyTime_ = currentTime;
               }
            }
         }

         if (monitorToProcess.has_value())
         {
            warp::log::Trace("Throttle passed. Notifying for: {}", monitorToProcess->scanName);
            NotifyMediaServers(monitorToProcess.value());
         }
         else
         {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
         }
      }

      warp::log::Info("Monitor thread has exited");
   }

   std::string RemoteScan::GetDisplayFolder(std::string_view path)
   {
      namespace fs = std::filesystem;
      fs::path p(path);

      // If path ends in a slash, some implementations return an empty filename.
      // We want the actual last directory component.
      if (p.has_relative_path() && p.filename().empty())
      {
         p = p.parent_path();
      }

      auto lastElement = p.filename();
      auto lastStr = lastElement.string();

      // Check for "Season" in the last folder name
      // (Matches "Season 01", "Specials", etc. if they contain "Season")
      if (lastStr.find("Season") != std::string::npos)
      {
         if (p.has_parent_path())
         {
            // Returns "ShowName/Season 01"
            return (p.parent_path().filename() / lastElement).generic_string();
         }
      }

      return lastStr.empty() ? std::string(path) : lastStr;
   }

   void RemoteScan::LogMonitorAdded(std::string_view scanName, std::string_view displayFolder)
   {
      warp::log::Info("{} Scan moved to monitor {} {}", warp::GetAnsiText("-->", ANSI_MONITOR_ADDED), warp::GetTag("name", scanName), warp::GetTag("folder", displayFolder));
   }

   void RemoteScan::AddFileMonitor(std::string_view scanName, std::string_view path)
   {
      std::unique_lock lock(monitorLock_);

      auto monitorIter = std::ranges::find_if(activeMonitors_,
          [scanName](const auto& monitor) { return monitor.scanName == scanName; });

      if (monitorIter != activeMonitors_.end())
      {
         auto pathIter = std::ranges::find_if(monitorIter->paths,
             [path](const auto& monitorPath) { return monitorPath.path == path; });

         if (pathIter == monitorIter->paths.end())
         {
            auto& newPath = monitorIter->paths.emplace_back(std::string(path), GetDisplayFolder(path));
            LogMonitorAdded(scanName, newPath.displayFolder);
         }

         monitorIter->time = std::chrono::system_clock::now();
         warp::log::Trace("Updated existing monitor {} updating time", scanName);
      }
      else
      {
         // Create new monitor
         auto& newMonitor = activeMonitors_.emplace_back();
         newMonitor.scanName = scanName;
         newMonitor.time = std::chrono::system_clock::now();

         auto& newPath = newMonitor.paths.emplace_back(std::string(path), GetDisplayFolder(path));
         LogMonitorAdded(scanName, newPath.displayFolder);
      }

      lock.unlock();
      monitorCv_.notify_one();
   }

   bool RemoteScan::GetScanPathValid(std::string_view path)
   {
      return !std::ranges::any_of(configReader_->GetIgnoreFolders(), [path](const auto& ignore) {
         return path.find(ignore.folder) != std::string_view::npos;
      });
   }

   bool RemoteScan::GetFileExtensionValid(std::string_view filename)
   {
      if (validExtensions_.empty()) return true;

      // Use filesystem to extract the extension part only
      std::filesystem::path p(filename);
      if (!p.has_extension()) return false;

      // Convert only the extension to lowercase
      std::string ext = p.extension().string();
      std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
         return std::tolower(c);
      });

      return validExtensions_.contains(ext);
   }

   void RemoteScan::ProcessFileUpdate(std::string_view scanName, std::string_view path, std::string_view filename)
   {
      if (GetScanPathValid(path) && GetFileExtensionValid(filename))
      {
         AddFileMonitor(scanName, path);
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