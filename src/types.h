#pragma once

#include <warp/log/log-types.h>

#include <chrono>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

namespace remote_scan
{
   inline const std::string ANSI_MONITOR_ADDED{std::format("{}33{}", warp::ANSI_CODE_START, warp::ANSI_CODE_END)};
   inline const std::string ANSI_MONITOR_PROCESSED{std::format("{}34{}", warp::ANSI_CODE_START, warp::ANSI_CODE_END)};

   enum class EffectType
   {
      RENAME,
      CREATE,
      MODIFY,
      DESTROY
   };

   struct FileMonitorData
   {
      std::string_view scanName;
      std::filesystem::path path;
      std::filesystem::path filename;
      bool isDirectory;
      EffectType effect;
   };

   struct ActiveMonitorPath
   {
      std::filesystem::path path;
      std::filesystem::path fileName;
      EffectType effect{};
      std::filesystem::path displayFullPath;
   };

   struct ActiveMonitor
   {
      std::string scanName;
      std::chrono::system_clock::time_point time;
      std::vector<ActiveMonitorPath> paths;
      std::filesystem::path lastPath;
   };
}