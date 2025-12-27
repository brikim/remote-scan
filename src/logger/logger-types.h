#pragma once

#include "logger/log-utils.h"

#include <format>
#include <string>

namespace remote_scan
{
   // Log pattern to be used by the logger
   const std::string LOG_PATTERN{"%m/%d/%Y %T [%^%l%$] %v"};
   const std::string LOG_PATTERN_ANSII_INFO{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v", utils::ANSI_CODE_LOG_HEADER, utils::ANSI_CODE_LOG, utils::ANSI_CODE_LOG_INFO, utils::ANSI_CODE_LOG)};
   const std::string LOG_PATTERN_ANSII_WARNING{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v", utils::ANSI_CODE_LOG_HEADER, utils::ANSI_CODE_LOG, utils::ANSI_CODE_LOG_WARNING, utils::ANSI_CODE_LOG)};
   const std::string LOG_PATTERN_ANSII_ERROR{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v", utils::ANSI_CODE_LOG_HEADER, utils::ANSI_CODE_LOG, utils::ANSI_CODE_LOG_ERROR, utils::ANSI_CODE_LOG)};
   const std::string LOG_PATTERN_ANSII_CRITICAL{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v", utils::ANSI_CODE_LOG_HEADER, utils::ANSI_CODE_LOG, utils::ANSI_CODE_LOG_CRITICAL, utils::ANSI_CODE_LOG)};
   const std::string LOG_PATTERN_ANSII_DEFAULT{std::format("{}%m/%d/%Y %T {}[{}%l{}] %v", utils::ANSI_CODE_LOG_HEADER, utils::ANSI_CODE_LOG, utils::ANSI_CODE_LOG_DEFAULT, utils::ANSI_CODE_LOG)};
}