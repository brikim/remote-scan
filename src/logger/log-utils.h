#include "types.h"

#include <format>
#include <string>

namespace remote_scan
{
   constexpr const char* const ANSI_CODE_START{"\33[38;5;"};
   constexpr const char* const ANSI_CODE_END{"m"};

   const std::string ANSI_CODE_LOG{std::format("{}15{}", ANSI_CODE_START, ANSI_CODE_END)};
   const std::string ANSI_CODE_TAG{std::format("{}37{}", ANSI_CODE_START, ANSI_CODE_END)};
   const std::string ANSI_CODE_PLEX{std::format("{}220{}", ANSI_CODE_START, ANSI_CODE_END)};
   const std::string ANSI_CODE_EMBY{std::format("{}77{}", ANSI_CODE_START, ANSI_CODE_END)};
   const std::string ANSI_CODE_JELLYFIN{std::format("{}134{}", ANSI_CODE_START, ANSI_CODE_END)};

   const std::string ANSI_MONITOR_ADDED{std::format("{}33{}", ANSI_CODE_START, ANSI_CODE_END)};
   const std::string ANSI_MONITOR_PROCESSED{std::format("{}34{}", ANSI_CODE_START, ANSI_CODE_END)};

   const std::string ANSI_FORMATTED_UNKNOWN("Unknown Server");
   const std::string ANSI_FORMATTED_PLEX(std::format("{}Plex{}", ANSI_CODE_PLEX, ANSI_CODE_LOG));
   const std::string ANSI_FORMATTED_EMBY(std::format("{}Emby{}", ANSI_CODE_EMBY, ANSI_CODE_LOG));
   const std::string ANSI_FORMATTED_JELLYFIN(std::format("{}Jellyfin{}", ANSI_CODE_JELLYFIN, ANSI_CODE_LOG));

   inline std::string GetTag(std::string_view tag, std::string_view value)
   {
      return std::format("{}{}{}={}", ANSI_CODE_TAG, tag, ANSI_CODE_LOG, value);
   }

   inline std::string GetAnsiText(std::string_view text, std::string_view ansiCode)
   {
      return std::format("{}{}{}", ansiCode, text, ANSI_CODE_LOG);
   }

   inline std::string_view GetFormattedPlex()
   {
      return ANSI_FORMATTED_PLEX;
   }

   inline std::string_view GetFormattedEmby()
   {
      return ANSI_FORMATTED_EMBY;
   }

   inline std::string_view GetFormattedJellyfin()
   {
      return ANSI_FORMATTED_JELLYFIN;
   }

   inline std::string_view GetFormattedApiName(ApiType type)
   {
      switch (type)
      {
         case ApiType::PLEX:
            return GetFormattedPlex();
         case ApiType::EMBY:
            return GetFormattedEmby();
         case ApiType::JELLYFIN:
            return GetFormattedJellyfin();
         default:
            return ANSI_FORMATTED_UNKNOWN;
      }
   }

   inline std::string BuildTargetString(std::string_view currentTarget, std::string_view newTarget, std::string_view targetInstance)
   {
      if (currentTarget.empty())
      {
         return targetInstance.empty() ? std::string(newTarget) : std::format("{}({})", newTarget, targetInstance);
      }
      else
      {
         return targetInstance.empty() ? std::format("{},{}", currentTarget, newTarget) : std::format("{},{}({})", currentTarget, newTarget, targetInstance);
      }
   }
}