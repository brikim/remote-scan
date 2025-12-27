#include "remote-scan.h"

#include "api/api-plex.h"
#include "logger/logger.h"
#include "logger/log-utils.h"

#include <cctype>
#include <filesystem>
#include <format>
#include <ranges>

namespace remote_scan
{
   RemoteScan::RemoteScan(std::shared_ptr<ConfigReader> configReader)
      : configReader_(configReader)
      , apiManager_(configReader)
   {
   }

   void RemoteScan::CleanupShutdown()
   {
      Logger::Instance().Info("Removing directory watches");
      for (auto& watcherPair : watchers_)
      {
         watcherPair.first.reset();
      }

      // Notify the monitor thread for shutdown
      Logger::Instance().Info("Requesting monitor thread to exit");
      {
         std::unique_lock<std::mutex> cvUniqueLock(cvLock_);
         cv_.notify_all();
      }

      // Wait for the thread to complete
      monitorThread_->join();
   }

   void RemoteScan::Run()
   {
      if (configReader_->IsConfigValid() == false)
      {
         Logger::Instance().Info("Config file not valid shutting down");
         return;
      }

      const auto& config{configReader_->GetRemoteScanConfig()};
      for (const auto& scan : config.scans)
      {
         auto& watcherPair{watchers_.emplace_back(std::make_pair(std::make_unique<efsw::FileWatcher>(),
                                                                 std::make_unique<UpdateListener>([this, scanName = std::string(scan.name)](std::string_view path, std::string_view filename) { this->ProcessFileUpdate(scanName, path, filename); })))};

         // For each path in this scan add a watch
         for (const auto& path : scan.paths)
         {
            std::filesystem::path fsPath(path);
            if (std::filesystem::exists(fsPath))
            {
               watcherPair.first->addWatch(path, watcherPair.second.get(), true);
            }
            else
            {
               Logger::Instance().Warning(std::format("{} contains {} that does not exist", GetTag("scan", scan.name), GetTag("path", path)));
            }
         }
      };

      // Start all the watchers
      for (auto& watcherPair : watchers_)
      {
         watcherPair.first->watch();
      }

      // Create the thread to monitor active scans
      monitorThread_ = std::make_unique<std::jthread>([this](std::stop_token stopToken) {
         this->Monitor(stopToken);
      });

      // Hold the main thread until shutdown is requested
      while (!shutdownRemotescan_)
      {
         std::this_thread::sleep_for(std::chrono::seconds(1));
      }

      // Clean up all threads before shutting down
      CleanupShutdown();

      Logger::Instance().Info("Run has completed");
   }

   void RemoteScan::LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library)
   {
      Logger::Instance().Warning(std::format("{}({}) {} not found ... Skipped notify",
                                             serverType,
                                             library.server,
                                             GetTag("library", library.library)));
   }

   void RemoteScan::LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library)
   {
      Logger::Instance().Warning(std::format("{}({}) server not available ... Skipped notify for {}",
                                             serverType,
                                             library.server,
                                             GetTag("library", library.library)));
   }

   bool RemoteScan::NotifyServer(ApiType type, const ScanLibraryConfig& library)
   {
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
               LogServerLibraryIssue(GetFormattedApiName(type), library);
            }
         }
         else
         {
            LogServerNotAvailable(GetFormattedApiName(type), library);
         }
      }
      return false;
   }

   void RemoteScan::NotifyMediaServers(const ActiveMonitor& monitor)
   {
      const auto& scanConfig{configReader_->GetRemoteScanConfig()};
      auto scanIter{std::ranges::find_if(scanConfig.scans, [&monitor](const auto& scan) { return scan.name == monitor.scanName; })};
      if (scanIter == scanConfig.scans.end())
      {
         Logger::Instance().Error(std::format("Attempting to notify media servers but {} not found!", monitor.scanName));
         return;
      }

      const auto& scan{*scanIter};
      std::string target;

      for (const auto& plexLibrary : scan.plexLibraries)
      {
         if (NotifyServer(ApiType::PLEX, plexLibrary))
         {
            target = BuildTargetString(target, GetFormattedApiName(ApiType::PLEX), plexLibrary.server);
         }
      }

      for (const auto& embyLibrary : scan.embyLibraries)
      {
         if (NotifyServer(ApiType::EMBY, embyLibrary))
         {
            target = BuildTargetString(target, GetFormattedApiName(ApiType::EMBY), embyLibrary.server);
         }
      }

      if (target.empty() == false)
      {
         for (auto& path : monitor.paths)
         {
            Logger::Instance().Info(std::format("{} Monitor moved to target {} {}", GetAnsiText(">>>", ANSI_MONITOR_PROCESSED), target, GetTag("folder", GetFolderName(path))));
         }
      }
      else
      {
         Logger::Instance().Warning(std::format("No Servers Notified for monitor {}", monitor.scanName));
      }
   }

   void RemoteScan::Monitor(std::stop_token stopToken)
   {
      while (!shutdownRemotescan_)
      {
         bool shouldMonitorWait{false};
         {
            std::scoped_lock<std::mutex> scopedLock(monitorLock_);
            shouldMonitorWait = activeMonitors_.empty();
         }

         // If there are no active monitors go to sleep and wait to be signalled
         if (shouldMonitorWait)
         {
            Logger::Instance().Trace("Monitor thread going to sleep");

            std::unique_lock<std::mutex> cvUniqueLock(cvLock_);
            cv_.wait(cvUniqueLock, [this] { return (runMonitor_ || shutdownRemotescan_); });

            Logger::Instance().Trace("Monitor thread waking up");
         }

         bool shouldMonitorSleep{false};
         if (auto currentTime{std::chrono::system_clock::now()};
             std::chrono::duration_cast<std::chrono::seconds>(currentTime - lastNotifyTime_).count() >= configReader_->GetRemoteScanConfig().secondsBetweenNotifies)
         {
            std::scoped_lock<std::mutex> scopedLock(monitorLock_);

            // Loop through all the active monitors and check if it is time to notify
            for (auto iter{activeMonitors_.begin()}; iter != activeMonitors_.end(); ++iter)
            {
               auto& monitor{*iter};
               if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - monitor.time).count() >= configReader_->GetRemoteScanConfig().secondsBeforeNotify)
               {
                  lastNotifyTime_ = currentTime;
                  NotifyMediaServers(monitor);

                  Logger::Instance().Trace(std::format("Processed & Removed monitor {}", monitor.scanName));

                  activeMonitors_.erase(iter);
                  break;
               }
            }

            shouldMonitorSleep = activeMonitors_.empty() == false;
         }

         // If active monitors are being processed sleep for the designated time
         if (shouldMonitorSleep)
         {
            std::this_thread::sleep_for(std::chrono::seconds(1));
         }

         runMonitor_ = false;
      }

      Logger::Instance().Info("Monitor thread has exited");
   }

   std::string RemoteScan::GetFolderName(std::string_view path)
   {
      // For now based on OS support the correct directory separator
#ifdef _WIN32
      constexpr std::string_view DIRECTORY_FLAG{"\\"};
#else
      constexpr std::string_view DIRECTORY_FLAG{"/"};
#endif

      // Extra bit to modifier end location. Used if path ends with DIRECTORY_FLAG
      auto endIndexModifier{path.ends_with(DIRECTORY_FLAG) ? 1 : 0};

      // Find the name of the folder unless it is a TV season folder then also return the folder before
      if (auto startIndex{path.rfind(DIRECTORY_FLAG, path.size() - endIndexModifier - 1)};
          startIndex != std::string_view::npos)
      {
         auto subStrStart{startIndex + 1};
         auto subStrCount{path.size() - subStrStart - endIndexModifier};
         auto returnFolder{path.substr(subStrStart, subStrCount)};

         if (returnFolder.length() < 10 && returnFolder.find("Season") != std::string::npos)
         {
            auto seasonStartIndex{path.rfind(DIRECTORY_FLAG, startIndex - 1)};
            if (seasonStartIndex != std::string_view::npos)
            {
               subStrStart = seasonStartIndex + 1;
               subStrCount = path.size() - subStrStart - endIndexModifier;
               returnFolder = path.substr(subStrStart, subStrCount);
            }
         }
         return std::string(returnFolder);
      }
      return std::string(path);
   }

   void RemoteScan::LogMonitorAdded(std::string_view scanName, std::string_view path)
   {
      Logger::Instance().Info(std::format("{} Scan moved to monitor {} {}", GetAnsiText("-->", ANSI_MONITOR_ADDED), GetTag("name", scanName), GetTag("path", GetFolderName(path))));
   }

   void RemoteScan::AddFileMonitor(std::string_view scanName, std::string_view path)
   {
      std::scoped_lock scopedMonitorLock(monitorLock_);

      auto monitorIter{std::ranges::find_if(activeMonitors_, [scanName](auto& monitor) { return monitor.scanName == scanName; })};
      if (monitorIter != activeMonitors_.end())
      {
         auto& currentMonitor{*monitorIter};
         auto pathIter{std::ranges::find_if(currentMonitor.paths, [path](auto& monitorPath) { return monitorPath == path; })};
         if (pathIter == currentMonitor.paths.end())
         {
            currentMonitor.paths.emplace_back(path);
            LogMonitorAdded(scanName, path);
         }
         (*monitorIter).time = std::chrono::system_clock::now();
         Logger::Instance().Trace(std::format("Found existing monitor {} updating time", currentMonitor.scanName));
      }
      else
      {
         auto& newMonitor{activeMonitors_.emplace_back()};
         newMonitor.scanName = scanName;
         newMonitor.time = std::chrono::system_clock::now();
         newMonitor.paths.emplace_back(path);

         // Notify the monitor thread that there is work to do
         {
            std::unique_lock<std::mutex> cvUniqueLock(cvLock_);
            runMonitor_ = true;
            cv_.notify_all();
         }
         LogMonitorAdded(scanName, path);
      }
   }

   std::string RemoteScan::GetLowercase(std::string_view name)
   {
      std::string returnValue{name};
      std::ranges::transform(returnValue, returnValue.begin(), [](unsigned char c) { return std::tolower(c); });
      return returnValue;
   }

   bool RemoteScan::GetScanPathValid(std::string_view path)
   {
      for (const auto& ignoreFolder : configReader_->GetIgnoreFolders())
      {
         if (path.find(ignoreFolder) != std::string_view::npos)
         {
            return false;
         }
      }
      return true;
   }

   bool RemoteScan::GetFileExtensionValid(std::string_view filename)
   {
      const auto& validFileExtensions{configReader_->GetValidFileExtensions()};

      auto lowercaseFilename{GetLowercase(filename)};
      return validFileExtensions.empty() ? true : std::ranges::any_of(validFileExtensions, [this, &lowercaseFilename](const auto& extension) {
         return lowercaseFilename.ends_with(GetLowercase(extension));
      });
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
      Logger::Instance().Info("Shutdown request received");

      shutdownRemotescan_ = true;
   }
}