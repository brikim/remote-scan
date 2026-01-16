#pragma once

#include <warp/log-types.h>

#include <format>
#include <string>

namespace remote_scan
{
   static constexpr int VALID_HTTP_RESPONSE_MAX{300};

   inline const std::string ANSI_MONITOR_ADDED{std::format("{}33{}", warp::ANSI_CODE_START, warp::ANSI_CODE_END)};
   inline const std::string ANSI_MONITOR_PROCESSED{std::format("{}34{}", warp::ANSI_CODE_START, warp::ANSI_CODE_END)};
}