#include "notify.h"

#include "config-reader/config-reader.h"
#include "types.h"
#include "version.h"

#include <warp/log/log.h>
#include <warp/log/log-utils.h>
#include <warp/utils.h>

#include <algorithm>
#include <cctype>
#include <ranges>

namespace remote_scan
{
   Notify::Notify(std::shared_ptr<ConfigReader> configReader,
                  std::function<bool(const std::filesystem::path)> getImageFunc)
      : configReader_(configReader)
      , getImageFunc_(std::move(getImageFunc))
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
   }

   void Notify::GetTasks(std::vector<warp::Task>& tasks)
   {
      apiManager_->GetTasks(tasks);
   }

   void Notify::LogServerLibraryIssue(std::string_view serverType, const ScanLibraryConfig& library)
   {
      warp::log::Warning("{}({}) {} not found ... Skipped notify",
                         serverType,
                         library.server,
                         warp::GetTag("library", library.library));
   }

   void Notify::LogServerNotAvailable(std::string_view serverType, const ScanLibraryConfig& library)
   {
      warp::log::Warning("{}({}) server not available ... Skipped notify for {}",
                         serverType,
                         library.server,
                         warp::GetTag("library", library.library));
   }

   bool Notify::NotifyPlex(const ActiveMonitor& monitor,
                           const std::filesystem::path& basePath,
                           const ScanLibraryConfig& library,
                           bool dryRun)
   {
      if (basePath.empty()) return false;

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

      // Gather all raw paths we intend to notify (adjusting for DESTROY events)
      std::vector<std::filesystem::path> rawPaths;
      for (const auto& path : monitor.paths)
      {
         std::error_code ec;
         if (std::filesystem::exists(path.path, ec))
         {
            rawPaths.emplace_back(path.path);
         }
         else
         {
            // Path is gone (like "New Folder"). Notify the parent so Plex sees it's missing.
            rawPaths.emplace_back(path.path.parent_path());
         }
      }

      // Remove exact duplicates and sort them by length (shallowest first)
      std::ranges::sort(rawPaths);
      auto [newEnd, _] = std::ranges::unique(rawPaths);
      rawPaths.erase(newEnd, rawPaths.end());

      // Filter out children. If a path is a sub-path of one we've already kept, skip it.
      std::vector<std::filesystem::path> optimizedPaths;
      for (auto& current : rawPaths)
      {
         bool isChild = std::ranges::any_of(optimizedPaths, [&](const auto& parent) {
            // Check if 'current' starts with 'parent'
            auto [root, child] = std::mismatch(parent.begin(), parent.end(), current.begin(), current.end());
            return root == parent.end();
         });

         if (!isChild)
         {
            optimizedPaths.emplace_back(std::move(current));
         }
      }

      // Notify the optimized list
      for (const auto& pathToNotify : optimizedPaths)
      {
         auto libraryScanPath = warp::ReplaceMediaPath(pathToNotify, basePath, library.mediaPath);

         if (!dryRun)
         {
            plexApi->SetLibraryScanPath(*libraryId, libraryScanPath);
         }

         warp::log::Trace("{} refresh library {} path {}",
                          plexApi->GetPrettyName(),
                          *libraryId,
                          libraryScanPath.generic_string());
      }

      return true;
   }

   bool Notify::NotifyEmby(const ActiveMonitor& monitor,
                           const std::filesystem::path& basePath,
                           const ScanLibraryConfig& library,
                           bool dryRun)
   {
      if (basePath.empty()) return false;

      auto* embyApi = apiManager_->GetEmbyApi(library.server);
      if (!embyApi || embyApi->GetValid() == false)
      {
         LogServerNotAvailable(warp::GetFormattedEmby(), library);
         return false;
      }

      // If any of the paths are a directory or an image file, we need to trigger a full library scan
      bool needsLibraryScan = std::ranges::any_of(monitor.paths, [this](const auto& p) {
         bool isImage = getImageFunc_ ? getImageFunc_(p.fileName) : false;
         return p.fileName.empty() || isImage;
      });

      if (needsLibraryScan)
      {
         auto libraryId{embyApi->GetLibraryId(library.library)};
         if (!libraryId)
         {
            LogServerLibraryIssue(warp::GetFormattedEmby(), library);
            return false;
         }

         if (!dryRun)
            embyApi->SetLibraryScan(*libraryId);

         warp::log::Trace("Notified {} to refresh library {}", embyApi->GetPrettyName(), *libraryId);
      }
      else
      {
         std::vector<warp::EmbyMediaUpdate> mediaUpdates;
         mediaUpdates.reserve(monitor.paths.size());

         for (const auto& path : monitor.paths)
         {
            warp::EmbyUpdateType embyUpdateType;
            switch (path.effect)
            {
               case EffectType::MODIFY:
                  embyUpdateType = warp::EmbyUpdateType::MODIFIED;
                  break;
               case EffectType::DESTROY:
                  embyUpdateType = warp::EmbyUpdateType::DELETED;
                  break;
               default:
                  // For CREATE and RENAME, we want to trigger a CREATED update. Emby will handle the rest.
                  embyUpdateType = warp::EmbyUpdateType::CREATED;
                  break;
            }

            mediaUpdates.emplace_back(warp::EmbyMediaUpdate{
               .path = warp::ReplaceMediaPath(path.path / path.fileName, basePath, library.mediaPath),
               .type = embyUpdateType
            });
         }

         if (!dryRun)
            embyApi->SetMediaScan(mediaUpdates);

         for (const auto& update : mediaUpdates)
         {
            warp::log::Trace("Notified {} of media update type:{} path:{}", embyApi->GetPrettyName(), static_cast<int>(update.type), update.path.generic_string());
         }
      }

      return true;
   }

   void Notify::NotifyMediaServers(const ActiveMonitor& monitor)
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
         if (NotifyPlex(monitor, scan.basePath, plexLibrary, scanConfig.dryRun))
         {
            syncServers = warp::BuildSyncServerString(syncServers, warp::GetFormattedPlex(), plexLibrary.server);
         }
      }

      for (const auto& embyLibrary : scan.embyLibraries)
      {
         if (NotifyEmby(monitor, scan.basePath, embyLibrary, scanConfig.dryRun))
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
                            warp::GetTag("media", path.displayFullPath.generic_string()));
         }
      }
      else
      {
         warp::log::Warning("No Servers Notified for monitor {}", monitor.scanName);
      }
   }
}