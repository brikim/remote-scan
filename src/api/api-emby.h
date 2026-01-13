#pragma once

#include "api/api-base.h"
#include "config-reader/config-reader-types.h"

#include <httplib.h>

#include <list>
#include <string>

namespace remote_scan
{
   class EmbyApi : public ApiBase
   {
   public:
      EmbyApi(const ServerConfig& serverConfig);
      virtual ~EmbyApi() = default;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;
      [[nodiscard]] std::optional<std::string> GetLibraryId(std::string_view libraryName) override;
      void SetLibraryScan(std::string_view libraryId) override;

   private:
      std::string_view GetApiBase() const override;
      std::string_view GetApiTokenName() const override;

      httplib::Client client_;
      httplib::Headers emptyHeaders_;
   };
}