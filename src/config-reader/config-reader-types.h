#pragma once

#include <glaze/glaze.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace remote_scan
{
   struct ServerConfig
   {
      std::string name;
      std::string url;
      std::string apiKey;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "server_name", &ServerConfig::name,
            "url", &ServerConfig::url,
            "api_key", &ServerConfig::apiKey
         );
      };
   };

   struct AppriseLoggingConfig
   {
      bool enabled{false};
      std::string url;
      std::string key;
      std::string title;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "enabled", &AppriseLoggingConfig::enabled,
            "url", &AppriseLoggingConfig::url,
            "key", &AppriseLoggingConfig::key,
            "message_title", &AppriseLoggingConfig::title
         );
      };
   };

   struct GotifyLoggingConfig
   {
      bool enabled{false};
      std::string url;
      std::string key;
      std::string title;
      int32_t priority{0};

      struct glaze
      {
         static constexpr auto value = glz::object(
            "enabled", &GotifyLoggingConfig::enabled,
            "url", &GotifyLoggingConfig::url,
            "key", &GotifyLoggingConfig::key,
            "message_title", &GotifyLoggingConfig::title,
            "priority", &GotifyLoggingConfig::priority
         );
      };
   };

   struct ScanLibraryConfig
   {
      std::string server;
      std::string library;
      std::string mediaPath;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "server_name", &ScanLibraryConfig::server,
            "library", &ScanLibraryConfig::library,
            "media_path", &ScanLibraryConfig::mediaPath
         );
      };
   };

   struct ScanConfigPath
   {
      std::filesystem::path path;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "path", &ScanConfigPath::path
         );
      };
   };

   struct ScanConfig
   {
      std::string name;
      std::vector<ScanLibraryConfig> plexLibraries;
      std::vector<ScanLibraryConfig> embyLibraries;
      std::vector<ScanLibraryConfig> jellyfinLibraries;
      std::filesystem::path basePath;
      std::vector<ScanConfigPath> pathsFromBase;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "name", &ScanConfig::name,
            "plex", &ScanConfig::plexLibraries,
            "emby", &ScanConfig::embyLibraries,
            "jellyfin", &ScanConfig::jellyfinLibraries,
            "base_path", &ScanConfig::basePath,
            "paths_from_base", &ScanConfig::pathsFromBase
         );
      };
   };

   struct RemoteScanIgnoreFolder
   {
      std::filesystem::path folder;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "ignore_folder", &RemoteScanIgnoreFolder::folder
         );
      };
   };

   struct RemoteScanFileExtension
   {
      std::string extension;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "extension", &RemoteScanFileExtension::extension
         );
      };
   };

   struct RemoteScanConfig
   {
      bool dryRun{false};
      int secondsBeforeNotify{90};
      int secondsBetweenNotifies{15};
      std::vector<ScanConfig> scans;
      std::vector<RemoteScanIgnoreFolder> ignoreFolders;
      std::vector<RemoteScanFileExtension> validFileExtensions;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "dry_run", &RemoteScanConfig::dryRun,
            "seconds_before_notify", &RemoteScanConfig::secondsBeforeNotify,
            "seconds_between_notifies", &RemoteScanConfig::secondsBetweenNotifies,
            "scans", &RemoteScanConfig::scans,
            "ignore_folders", &RemoteScanConfig::ignoreFolders,
            "valid_file_extensions", &RemoteScanConfig::validFileExtensions
         );
      };
   };

   struct ConfigData
   {
      std::vector<ServerConfig> plexServers;
      std::vector<ServerConfig> embyServers;
      std::vector<ServerConfig> jellyfinServers;
      AppriseLoggingConfig appriseLogging;
      GotifyLoggingConfig gotifyLogging;
      RemoteScanConfig remoteScan;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "plex", &ConfigData::plexServers,
            "emby", &ConfigData::embyServers,
            "jellyfin", &ConfigData::jellyfinServers,
            "apprise_logging", &ConfigData::appriseLogging,
            "gotify_logging", &ConfigData::gotifyLogging,
            "remote_scan", &ConfigData::remoteScan
         );
      };
   };
}