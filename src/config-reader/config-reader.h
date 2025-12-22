#pragma once

#include "src/config-reader/config-reader-types.h"

#include <external/json/json.hpp>
#include <span>
#include <vector>

namespace remote_scan
{
   class ConfigReader
   {
   public:
      ConfigReader();
      virtual ~ConfigReader() = default;

      [[nodiscard]] bool IsConfigValid() const;

      [[nodiscard]] const std::vector<ServerConfig>& GetPlexServers() const;
      [[nodiscard]] const std::vector<ServerConfig>& GetEmbyServers() const;
      [[nodiscard]] const std::vector<ServerConfig>& GetJellyfinServers() const;
      [[nodiscard]] const AppriseLoggingConfig& GetAppriseLogging() const;
      [[nodiscard]] const RemoteScanConfig& GetRemoteScanConfig() const;
      [[nodiscard]] const std::vector<std::string>& GetIgnoreFolders() const;
      [[nodiscard]] const std::vector<std::string>& GetValidFileExtensions() const;

   private:
      void ReadConfigFile(const char* path);

      void ReadServerConfig(const nlohmann::json& jsonData, std::vector<ServerConfig>& serverVec);
      bool ReadServers(const nlohmann::json& jsonData);

      void ReadAppriseLogging(const nlohmann::json& jsonData);

      void ReadIndividualScanConfig(const nlohmann::json& jsonData);
      void ReadIgnoreFolder(const nlohmann::json& jsonData);
      void ReadValidExtension(const nlohmann::json& jsonData);
      void ReadScanConfig(const nlohmann::json& jsonData);

      bool configValid_{false};
      ConfigData configData_;
   };
}