#pragma once

namespace remote_scan
{
   static constexpr int VALID_HTTP_RESPONSE_MAX{300};

   enum class ApiType
   {
      PLEX,
      EMBY,
      JELLYFIN
   };
}