#include "update-listener.h"

#include <warp/log.h>

namespace remote_scan
{
   UpdateListener::UpdateListener(const std::function<void(std::string_view dir, std::string_view filename)>& updateFunc)
      : updateFunc_(updateFunc)
   {
   }

   void UpdateListener::handleFileAction([[maybe_unused]] efsw::WatchID watchid,
                                         const std::string& dir,
                                         const std::string& filename,
                                         efsw::Action action,
                                         [[maybe_unused]] std::string oldFilename)
   {
      switch (action)
      {
         case efsw::Actions::Add:
         case efsw::Actions::Delete:
         case efsw::Actions::Modified:
         case efsw::Actions::Moved:
            if (filename.empty() == false)
            {
               updateFunc_(dir, filename);
            }
            break;
         default:
            warp::log::Warning("Recieved an unknown file action {} for file {} and directory {}", static_cast<int>(action), filename, dir);
      }
   }
};