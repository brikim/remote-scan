#pragma once

#include <string>
#include <vector>

namespace remote_scan
{
   struct ServerConfig
   {
      std::string name;
      std::string address;
      std::string apiKey;
   };

   struct AppriseLoggingConfig
   {
      bool enabled{false};
      std::string url;
      std::string key;
      std::string title;
   };

   struct ScanLibraryConfig
   {
      std::string server;
      std::string library;
   };

   struct ScanConfig
   {
      std::string name;
      std::vector<ScanLibraryConfig> plexLibraries;
      std::vector<ScanLibraryConfig> embyLibraries;
      std::vector<ScanLibraryConfig> jellyfinLibraries;
      std::vector<std::string> paths;
   };

   struct RemoteScanConfig
   {
      int secondsBeforeNotify{90};
      int secondsBetweenNotifies{15};
      std::vector<ScanConfig> scans;
   };

   struct ConfigData
   {
      std::vector<ServerConfig> plexServers;
      std::vector<ServerConfig> embyServers;
      std::vector<ServerConfig> jellyfinServers;
      AppriseLoggingConfig appriseLogging;
      RemoteScanConfig remoteScan;
      std::vector<std::string> ignoreFolders;
      std::vector<std::string> validFileExtensions;
   };
}