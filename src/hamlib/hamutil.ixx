export module hamutil;

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>

export namespace Ham
{
  int Diag(const char* msg, int ret=EXIT_FAILURE)
  {
    int e = errno;
    fprintf(stderr, "%s: %s\n", msg, strerror(e));
    return ret;
  }
}
