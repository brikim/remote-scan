#include "config-reader/config-reader.h"
#include "remote-scan.h"
#include "version.h"

#include <warp/log.h>

#include <atomic>
#include <csignal>
#include <memory>

std::unique_ptr<remote_scan::RemoteScan> REMOTE_SCAN;

void signal_handler(int signal_num)
{
   if ((signal_num == SIGINT || signal_num == SIGTERM)
       && REMOTE_SCAN)
   {
      REMOTE_SCAN->ProcessShutdown();
   }
}

void init_logging(const std::shared_ptr<remote_scan::ConfigReader>& configReader)
{
   if (const auto* logPath = std::getenv("LOG_PATH");
       logPath)
   {
      warp::log::InitFileLogging(logPath, "remote-scan.log");
   }

   // Initialize Apprise logging if configured
   const auto& appriseConfig = configReader->GetAppriseLogging();
   if (appriseConfig.enabled)
   {
      warp::AppriseLoggingConfig warpAppriseConfig{
         .url = appriseConfig.url,
         .key = appriseConfig.key,
         .message_title = appriseConfig.title
      };
      warp::log::InitApprise(warpAppriseConfig);
   }

   const auto& gotifyConfig = configReader->GetGotifyLogging();
   if (gotifyConfig.enabled)
   {
      warp::GotifyLoggingConfig warpGotifyConfig{
         .url = gotifyConfig.url,
         .key = gotifyConfig.key,
         .message_title = gotifyConfig.title,
         .priority = gotifyConfig.priority
      };
      warp::log::InitGotify(warpGotifyConfig);
   }
}

int main()
{
   auto configReader{std::make_shared<remote_scan::ConfigReader>()};
   if (configReader->IsConfigValid())
   {
      // Initialize the logging system
      init_logging(configReader);
   }
   else
   {
      // If not valid exit logging the error.
      // This file is required for this application to run
      warp::log::Critical("Config file not valid shutting down");
      return 1;
   }

   warp::log::Info("Remote Scan {} Starting", remote_scan::REMOTE_SCAN_VERSION);

   REMOTE_SCAN = std::make_unique<remote_scan::RemoteScan>(configReader);

   // Register to handle the required signals
   std::signal(SIGINT, signal_handler);
   std::signal(SIGTERM, signal_handler);

   REMOTE_SCAN->Run();

   return 0;
}