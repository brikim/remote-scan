#include "config-reader.h"

#include "src/logger/logger.h"
#include "src/logger/log-utils.h"

#include <cstdlib>
#include <format>
#include <fstream>

namespace remote_scan
{
   constexpr const std::string_view PLEX{"plex"};
   constexpr const std::string_view EMBY{"emby"};
   constexpr const std::string_view JELLYFIN{"jellyfin"};
   constexpr const std::string_view SERVER_NAME{"server_name"};
   constexpr const std::string_view URL{"url"};
   constexpr const std::string_view API_KEY{"api_key"};
   constexpr const std::string_view ENABLED("enabled");
   constexpr const std::string_view KEY("key");
   constexpr const std::string_view MESSAGE_TITLE("message_title");
   constexpr const std::string_view APPRISE_LOGGING("apprise_logging");
   constexpr const std::string_view REMOTE_SCAN("remote_scan");
   constexpr const std::string_view SECONDS_BEFORE_NOTIFY("seconds_before_notify");
   constexpr const std::string_view SECONDS_BETWEEN_NOTIFIES("seconds_between_notifies");
   constexpr const std::string_view SCANS("scans");
   constexpr const std::string_view NAME("name");
   constexpr const std::string_view LIBRARY("library");
   constexpr const std::string_view PATHS("paths");
   constexpr const std::string_view PATH("path");
   constexpr const std::string_view IGNORE_FOLDERS("ignore_folders");
   constexpr const std::string_view IGNORE_FOLDER("ignore_folder");
   constexpr const std::string_view VALID_FILE_EXTENSIONS("valid_file_extensions");
   constexpr const std::string_view EXTENSION("extension");


   ConfigReader::ConfigReader()
   {
      //_putenv_s("CONFIG_PATH", "../config");
      if (const auto* configPath = std::getenv("CONFIG_PATH");
          configPath != nullptr)
      {
         ReadConfigFile(configPath);
      }
      else
      {
         Logger::Instance().Error("CONFIG_PATH environment variable not found!");
      }
   }

   void ConfigReader::ReadConfigFile(const char* path)
   {
      std::string pathFileName{path};
      pathFileName.append("/config.conf");

      std::ifstream f(pathFileName);
      if (f.is_open() == false)
      {
         Logger::Instance().Error(std::format("Config file {} not found!", pathFileName));
         return;
      }

      auto jsonData = nlohmann::json::parse(f);

      if (ReadServers(jsonData) == false)
      {
         Logger::Instance().Error("No Servers loaded exiting");
         return;
      }

      // The configuration file is valid
      configValid_ = true;

      ReadAppriseLogging(jsonData);
      ReadScanConfig(jsonData);
   }

   void ConfigReader::ReadServerConfig(const nlohmann::json& jsonData, std::vector<ServerConfig>& serverVec)
   {
      for (auto& serverConfig : jsonData)
      {
         if (serverConfig.contains(SERVER_NAME) == false
             || serverConfig.contains(URL) == false
             || serverConfig.contains(API_KEY) == false)
         {
            Logger::Instance().Error(std::format("{} server config invalid server_name:{} url:{} api_key:{}",
                                                 GetFormattedPlex(),
                                                 serverConfig[SERVER_NAME].is_null() ? "ERROR" : serverConfig[SERVER_NAME].get<std::string>(),
                                                 serverConfig[URL].is_null() ? "ERROR" : serverConfig[URL].get<std::string>(),
                                                 serverConfig[API_KEY].is_null() ? "ERROR" : serverConfig[API_KEY].get<std::string>()));
            break;
         }

         serverVec.emplace_back(serverConfig[SERVER_NAME].get<std::string>(), serverConfig[URL].get<std::string>(), serverConfig[API_KEY].get<std::string>());
      }
   }

   bool ConfigReader::ReadServers(const nlohmann::json& jsonData)
   {
      if (jsonData.contains(PLEX))
      {
         ReadServerConfig(jsonData[PLEX], configData_.plexServers);
      }

      if (jsonData.contains(EMBY))
      {
         ReadServerConfig(jsonData[EMBY], configData_.embyServers);
      }

      if (jsonData.contains(JELLYFIN))
      {
         ReadServerConfig(jsonData[JELLYFIN], configData_.jellyfinServers);
      }

      return (configData_.plexServers.size() + configData_.embyServers.size() + configData_.jellyfinServers.size()) > 0;
   }

   void ConfigReader::ReadAppriseLogging(const nlohmann::json& jsonData)
   {
      if (jsonData.contains(APPRISE_LOGGING) == false
          || jsonData[APPRISE_LOGGING].contains(ENABLED) == false
          || jsonData[APPRISE_LOGGING][ENABLED].get<std::string>() != "True"
          || jsonData[APPRISE_LOGGING].contains(URL) == false
          || jsonData[APPRISE_LOGGING].contains(KEY) == false
          || jsonData[APPRISE_LOGGING].contains(MESSAGE_TITLE) == false)
      {
         return;
      }

      configData_.appriseLogging.enabled = true;
      configData_.appriseLogging.url = jsonData[APPRISE_LOGGING][URL].get<std::string>();
      configData_.appriseLogging.key = jsonData[APPRISE_LOGGING][KEY].get<std::string>();
      configData_.appriseLogging.title = jsonData[APPRISE_LOGGING][MESSAGE_TITLE].get<std::string>();
   }

   void ConfigReader::ReadIndividualScanConfig(const nlohmann::json& jsonData)
   {
      auto readServerLibrary = [](const nlohmann::json& config, std::vector<ScanLibraryConfig>& libraryConfigs) {
         if (config.contains(SERVER_NAME) == false
             || config.contains(LIBRARY) == false)
         {
            return;
         }

         libraryConfigs.emplace_back(config[SERVER_NAME], config[LIBRARY]);
      };

      ScanConfig scanConfig;

      if (jsonData.contains(NAME) == false)
      {
         return;
      }
      scanConfig.name = jsonData[NAME].get<std::string>();

      if (jsonData.contains(PLEX))
      {
         for (const auto& plexScan : jsonData[PLEX])
         {
            readServerLibrary(plexScan, scanConfig.plexLibraries);
         }
      }


      if (jsonData.contains(EMBY))
      {
         for (const auto& embyScan : jsonData[EMBY])
         {
            readServerLibrary(embyScan, scanConfig.embyLibraries);
         }
      }

      if (jsonData.contains(JELLYFIN))
      {
         for (const auto& jellyfinScan : jsonData[JELLYFIN])
         {
            readServerLibrary(jellyfinScan, scanConfig.jellyfinLibraries);
         }
      }

      if (jsonData.contains(PATHS))
      {
         for (const auto& path : jsonData[PATHS])
         {
            if (path.contains(PATH))
            {
               scanConfig.paths.emplace_back(path[PATH].get<std::string>());
            }
         }
      }

      if ((scanConfig.plexLibraries.size() + scanConfig.embyLibraries.size() + scanConfig.jellyfinLibraries.size()) > 0 && scanConfig.paths.size() > 0)
      {
         configData_.remoteScan.scans.emplace_back(scanConfig);
      }
   }

   void ConfigReader::ReadIgnoreFolder(const nlohmann::json& jsonData)
   {
      if (jsonData.contains(IGNORE_FOLDER))
      {
         configData_.ignoreFolders.emplace_back(jsonData[IGNORE_FOLDER].get<std::string>());
      }
   }

   void ConfigReader::ReadValidExtension(const nlohmann::json& jsonData)
   {
      if (jsonData.contains(EXTENSION))
      {
         configData_.validFileExtensions.emplace_back(jsonData[EXTENSION].get<std::string>());
      }
   }

   void ConfigReader::ReadScanConfig(const nlohmann::json& jsonData)
   {
      if (jsonData.contains(REMOTE_SCAN) == false)
      {
         Logger::Instance().Error(std::format("{} settings not found in config file", REMOTE_SCAN));
         return;
      }

      const auto& remoteScanJson = jsonData[REMOTE_SCAN];
      if (remoteScanJson.contains(SECONDS_BEFORE_NOTIFY))
      {
         configData_.remoteScan.secondsBeforeNotify = remoteScanJson[SECONDS_BEFORE_NOTIFY].get<int>();
      }

      if (remoteScanJson.contains(SECONDS_BETWEEN_NOTIFIES))
      {
         configData_.remoteScan.secondsBetweenNotifies = remoteScanJson[SECONDS_BETWEEN_NOTIFIES].get<int>();
      }

      for (const auto& scan : remoteScanJson[SCANS])
      {
         ReadIndividualScanConfig(scan);
      }

      for (const auto& ignoreFolder : remoteScanJson[IGNORE_FOLDERS])
      {
         ReadIgnoreFolder(ignoreFolder);
      }

      for (const auto& extension : remoteScanJson[VALID_FILE_EXTENSIONS])
      {
         ReadValidExtension(extension);
      }
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

   const RemoteScanConfig& ConfigReader::GetRemoteScanConfig() const
   {
      return configData_.remoteScan;
   }

   const std::vector<std::string>& ConfigReader::GetIgnoreFolders() const
   {
      return configData_.ignoreFolders;
   }

   const std::vector<std::string>& ConfigReader::GetValidFileExtensions() const
   {
      return configData_.validFileExtensions;
   }
}