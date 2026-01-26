#pragma once

#include <warp/log/log-types.h>

#include <filesystem>
#include <format>
#include <string>

namespace remote_scan
{
   inline const std::string ANSI_MONITOR_ADDED{std::format("{}33{}", warp::ANSI_CODE_START, warp::ANSI_CODE_END)};
   inline const std::string ANSI_MONITOR_PROCESSED{std::format("{}34{}", warp::ANSI_CODE_START, warp::ANSI_CODE_END)};

   struct FileMonitorData
   {
      std::string_view scanName;
      std::filesystem::path path;
      std::filesystem::path filename;
      bool isDirectory;
      bool destroy;
   };
}