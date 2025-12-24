#pragma once

#include "config-reader/config-reader-types.h"

#include <httplib/httplib.h>
#include <string>

namespace remote_scan
{
   class LogApprise
   {
   public:
      explicit LogApprise(const AppriseLoggingConfig& config);
      virtual ~LogApprise() = default;

      void Send(const std::string& msg);

   private:
      httplib::Client client_;
      std::string key_;
      std::string title_;
   };
}