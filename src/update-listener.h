#include "config-reader/config-reader-types.h"

#include <efsw/efsw.hpp>

#include <functional>
#include <string>

namespace remote_scan
{
   class UpdateListener : public efsw::FileWatchListener
   {
   public:
      UpdateListener(const std::function<void(std::string_view dir, std::string_view filename)>& updateFunc);

      virtual ~UpdateListener() = default;

      void handleFileAction(efsw::WatchID watchid,
                            const std::string& dir,
                            const std::string& filename,
                            efsw::Action action,
                            std::string oldFilename) override;

   private:
      std::function<void(std::string_view dir, std::string_view filename)> updateFunc_;
   };
}