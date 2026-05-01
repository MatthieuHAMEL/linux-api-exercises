module;

#include <string>

export module hampath;

import hamio;
#include <unistd.h>

export namespace Ham
{
  class Path
  {
  public:
    Path(std::string iPath) : _path(std::move(iPath)) {
    }
    
    Path(const char* iPath) : _path(iPath) {
    }

    // iMode = F_OK (does file exist?) or a combination {X,R,W}_OK
    bool exists(int iMode)
    {
      return (0 == access(_path.c_str(), iMode));
    }

    File open(int iFlags)
    {
      return File(_path.c_str(), iFlags);
    }
  private:
    std::string _path;
  };
}
