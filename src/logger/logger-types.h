#pragma once

#include "logger/log-utils.h"

#include <format>
#include <string>

namespace remote_scan
{
   // Log pattern to be used by the logger
   const std::string LOG_PATTERN{std::format("{}{}{}{}", utils::ANSI_CODE_LOG_HEADER, "%m/%d/%Y %T ", utils::ANSI_CODE_LOG, "[%^%l%$] %v")};
}