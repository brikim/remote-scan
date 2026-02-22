#pragma once

#include "config-reader/config-reader-types.h"

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
      [[nodiscard]] const GotifyLoggingConfig& GetGotifyLogging() const;
      [[nodiscard]] const RemoteScanConfig& GetRemoteScanConfig() const;
      [[nodiscard]] const std::vector<RemoteScanIgnoreFolder>& GetIgnoreFolders() const;
      [[nodiscard]] const std::vector<RemoteScanFileExtension>& GetImageExtensions() const;
      [[nodiscard]] const std::vector<RemoteScanFileExtension>& GetValidFileExtensions() const;
      [[nodiscard]] const std::vector<RemoteScanFileExtension>& GetStripExtensions() const;

   private:
      void ReadConfigFile(const char* path);

      bool configValid_{false};
      ConfigData configData_;
   };
}