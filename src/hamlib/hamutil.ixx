export module hamutil;

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>

export namespace Ham
{
  // todo allow msg to be a formatted string.
  // (how do I do with the default ret arg?)
  int Diag(const char* msg, int ret=EXIT_FAILURE)
  {
    int e = errno;
    fprintf(stderr, "%s: %s\n", msg, strerror(e));
    return ret;
  }
}
