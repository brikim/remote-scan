#include "logger/logger.h"
#include "remote-scan.h"
#include "version.h"

#include <atomic>
#include <csignal>
#include <format>
#include <memory>

remote_scan::RemoteScan* GLOBAL_REMOTE_SCAN{nullptr};
std::atomic<bool> SHUTDOWN_REQUESTED(false);
void signal_handler(int signal_num)
{
   if (signal_num == SIGINT || signal_num == SIGTERM)
   {
      GLOBAL_REMOTE_SCAN->ProcessShutdown();
   }
}

int main()
{
   std::signal(SIGINT, signal_handler);
   std::signal(SIGTERM, signal_handler);

   remote_scan::Logger::Instance().Info(std::format("Remote Scan {} Starting", remote_scan::REMOTE_SCAN_VERSION));

   auto remoteScan = std::make_unique<remote_scan::RemoteScan>();
   GLOBAL_REMOTE_SCAN = remoteScan.get();

   remoteScan->Run();
   return 0;
}