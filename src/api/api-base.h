#pragma once

#include "config-reader/config-reader-types.h"

#include <warp/log.h>

#include <format>
#include <optional>
#include <string>

namespace remote_scan
{
   using ApiParams = std::vector<std::pair<std::string_view, std::string_view>>;

   class ApiBase
   {
   public:
      ApiBase(const ServerConfig& serverConfig,
              std::string_view className,
              std::string_view ansiiCode);
      virtual ~ApiBase() = default;

      [[nodiscard]] const std::string& GetName() const;
      [[nodiscard]] const std::string& GetUrl() const;
      [[nodiscard]] const std::string& GetApiKey() const;

      [[nodiscard]] virtual bool GetValid() = 0;
      [[nodiscard]] virtual std::optional<std::string> GetServerReportedName() = 0;
      [[nodiscard]] virtual std::optional<std::string> GetLibraryId(std::string_view libraryName) = 0;
      virtual void SetLibraryScan(std::string_view libraryId) = 0;

   protected:
      [[nodiscard]] virtual std::string_view GetApiBase() const = 0;
      [[nodiscard]] virtual std::string_view GetApiTokenName() const = 0;

      [[nodiscard]] const std::string& GetLogHeader() const;
      void AddApiParam(std::string& url, const ApiParams& params) const;
      [[nodiscard]] std::string BuildApiPath(std::string_view path) const;
      [[nodiscard]] std::string BuildApiParamsPath(std::string_view path, const ApiParams& params) const;
      [[nodiscard]] std::string GetPercentEncoded(std::string_view src) const;

      // Returns if the http request was successful and outputs to the log if not successful
      bool IsHttpSuccess(std::string_view name, const httplib::Result& result, bool log = true);

      template<typename... Args>
      void LogTrace(std::format_string<Args...> fmt, Args &&...args)
      {
         warp::log::TraceWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void LogInfo(std::format_string<Args...> fmt, Args &&...args)
      {
         warp::log::InfoWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void LogWarning(std::format_string<Args...> fmt, Args&&... args)
      {
         warp::log::WarningWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

      template<typename... Args>
      void LogError(std::format_string<Args...> fmt, Args &&...args)
      {
         warp::log::ErrorWithHeader(header_, fmt, std::forward<Args>(args)...);
      }

   private:
      std::string header_;
      std::string name_;
      std::string url_;
      std::string apiKey_;
      std::string logHeader_;
   };
}