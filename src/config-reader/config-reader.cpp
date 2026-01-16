#include "config-reader.h"

#include <warp/log.h>
#include <warp/log-utils.h>

#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>

namespace remote_scan
{
   ConfigReader::ConfigReader()
   {
      //_putenv_s("CONFIG_PATH", "../config");
      if (const auto* configPath = std::getenv("CONFIG_PATH");
          configPath)
      {
         ReadConfigFile(configPath);
      }
      else
      {
         warp::log::Error("CONFIG_PATH environment variable not found!");
      }
   }

   void ConfigReader::ReadConfigFile(const char* path)
   {
      std::filesystem::path pathFileName = std::filesystem::path(path) / "config.conf";
      std::ifstream file(pathFileName, std::ios::in | std::ios::binary);

      if (!file.is_open())
      {
         warp::log::Error("Config file {} not found!", pathFileName.string());
         return;
      }

      if (auto ec = glz::read_file_json < glz::opts{.error_on_unknown_keys = false} > (
         configData_,
         pathFileName.string(),
         std::string{}))
      {
         warp::log::Warning("{} - Glaze Error: {} (File: {})",
                            __func__, static_cast<int>(ec.ec), pathFileName.string());
         return;
      }

      configValid_ = true;
   }

   bool ConfigReader::IsConfigValid() const
   {
      return configValid_;
   }

   const std::vector<ServerConfig>& ConfigReader::GetPlexServers() const
   {
      return configData_.plexServers;
   }

   const std::vector<ServerConfig>& ConfigReader::GetEmbyServers() const
   {
      return configData_.embyServers;
   }

   const std::vector<ServerConfig>& ConfigReader::GetJellyfinServers() const
   {
      return configData_.jellyfinServers;
   }

   const AppriseLoggingConfig& ConfigReader::GetAppriseLogging() const
   {
      return configData_.appriseLogging;
   }

   const GotifyLoggingConfig& ConfigReader::GetGotifyLogging() const
   {
      return configData_.gotifyLogging;
   }

   const RemoteScanConfig& ConfigReader::GetRemoteScanConfig() const
   {
      return configData_.remoteScan;
   }

   const std::vector<RemoteScanIgnoreFolder>& ConfigReader::GetIgnoreFolders() const
   {
      return configData_.remoteScan.ignoreFolders;
   }

   const std::vector<RemoteScanFileExtension>& ConfigReader::GetValidFileExtensions() const
   {
      return configData_.remoteScan.validFileExtensions;
   }
}