#pragma once

#include <glaze/glaze.hpp>

#include <string>
#include <vector>

namespace remote_scan
{
   struct ServerConfig
   {
      std::string name;
      std::string address;
      std::string apiKey;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "server_name", &ServerConfig::name,
            "url", &ServerConfig::address,
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

   struct ScanLibraryConfig
   {
      std::string server;
      std::string library;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "server_name", &ScanLibraryConfig::server,
            "library", &ScanLibraryConfig::library
         );
      };
   };

   struct ScanConfigPath
   {
      std::string path;

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
      std::vector<ScanConfigPath> paths;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "name", &ScanConfig::name,
            "plex", &ScanConfig::plexLibraries,
            "emby", &ScanConfig::embyLibraries,
            "jellyfin", &ScanConfig::jellyfinLibraries,
            "paths", &ScanConfig::paths
         );
      };
   };

   struct RemoteScanIgnoreFolder
   {
      std::string folder;

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
      RemoteScanConfig remoteScan;

      struct glaze
      {
         static constexpr auto value = glz::object(
            "plex", &ConfigData::plexServers,
            "emby", &ConfigData::embyServers,
            "jellyfin", &ConfigData::jellyfinServers,
            "apprise_logging", &ConfigData::appriseLogging,
            "remote_scan", &ConfigData::remoteScan
         );
      };
   };
}