#include "src/types.h"

#include <format>
#include <string>

namespace remote_scan
{
   constexpr const char* const ANSII_CODE_START{"\33[38;5;"};
   constexpr const char* const ANSII_CODE_END{"m"};

   const std::string ANSII_CODE_LOG{std::format("{}15{}", ANSII_CODE_START, ANSII_CODE_END)};
   const std::string ANSII_CODE_TAG{std::format("{}37{}", ANSII_CODE_START, ANSII_CODE_END)};
   const std::string ANSII_CODE_PLEX{std::format("{}220{}", ANSII_CODE_START, ANSII_CODE_END)};
   const std::string ANSII_CODE_EMBY{std::format("{}77{}", ANSII_CODE_START, ANSII_CODE_END)};
   const std::string ANSII_CODE_JELLYFIN{std::format("{}134{}", ANSII_CODE_START, ANSII_CODE_END)};

   const std::string ANSII_MONITOR_ADDED{std::format("{}33{}", ANSII_CODE_START, ANSII_CODE_END)};
   const std::string ANSII_MONITOR_PROCESSED{std::format("{}34{}", ANSII_CODE_START, ANSII_CODE_END)};

   const std::string ANSII_FORMATTED_UNKNOWN("Unknown Server");
   const std::string ANSII_FORMATTED_PLEX(std::format("{}Plex{}", ANSII_CODE_PLEX, ANSII_CODE_LOG));
   const std::string ANSII_FORMATTED_EMBY(std::format("{}Emby{}", ANSII_CODE_EMBY, ANSII_CODE_LOG));
   const std::string ANSII_FORMATTED_JELLYFIN(std::format("{}Jellyfin{}", ANSII_CODE_JELLYFIN, ANSII_CODE_LOG));

   inline std::string GetTag(std::string_view tag, std::string_view value)
   {
      return std::format("{}{}{}={}", ANSII_CODE_TAG, tag, ANSII_CODE_LOG, value);
   }

   inline std::string GetAnsiiText(std::string_view text, std::string_view ansiiCode)
   {
      return std::format("{}{}{}", ansiiCode, text, ANSII_CODE_LOG);
   }

   inline std::string_view GetFormattedPlex()
   {
      return ANSII_FORMATTED_PLEX;
   }

   inline std::string_view GetFormattedEmby()
   {
      return ANSII_FORMATTED_EMBY;
   }

   inline std::string_view GetFormattedJellyfin()
   {
      return ANSII_FORMATTED_JELLYFIN;
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
            return ANSII_FORMATTED_UNKNOWN;
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