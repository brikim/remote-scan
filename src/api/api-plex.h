#pragma once

#include "src/api/api-base.h"
#include "src/config-reader/config-reader-types.h"

#include <httplib.h>
#include <list>
#include <string>

namespace remote_scan
{
   class PlexApi : public ApiBase
   {
   public:
      PlexApi(const ServerConfig& serverConfig);
      virtual ~PlexApi() = default;

      // Returns true if the server is reachable and the API key is valid
      [[nodiscard]] bool GetValid() override;
      [[nodiscard]] std::optional<std::string> GetServerReportedName() override;
      [[nodiscard]] std::optional<std::string> GetLibraryId(std::string_view libraryName) override;
      void SetLibraryScan(std::string_view libraryId) override;

   private:
      std::string BuildApiPath(std::string_view path);

      httplib::Client client_;
   };
}