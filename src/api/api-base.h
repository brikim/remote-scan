#pragma once

#include "src/config-reader/config-reader-types.h"

#include <optional>
#include <string>

namespace remote_scan
{
   class ApiBase
   {
   public:
      ApiBase(const ServerConfig& serverConfig, std::string_view className, std::string_view ansiiCode);
      virtual ~ApiBase() = default;

      [[nodiscard]] const std::string& GetName() const;
      [[nodiscard]] const std::string& GetUrl() const;
      [[nodiscard]] const std::string& GetApiKey() const;

      [[nodiscard]] virtual bool GetValid() = 0;
      [[nodiscard]] virtual std::optional<std::string> GetServerReportedName() = 0;
      [[nodiscard]] virtual std::optional<std::string> GetLibraryId(std::string_view libraryName) = 0;
      virtual void SetLibraryScan(std::string_view libraryId) = 0;

   protected:
      [[nodiscard]] const std::string& GetLogHeader() const;

   private:
      std::string name_;
      std::string url_;
      std::string apiKey_;
      std::string logHeader_;
   };
}