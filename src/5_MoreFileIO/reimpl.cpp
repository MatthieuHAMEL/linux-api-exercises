#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>

import std;
import hamio;
import hamutil;
using namespace std;
using namespace Ham;

namespace Reimpl
{
  int dup(int fd)
  {
    return fcntl(fd, F_DUPFD, /*min*/0);
  }

  /* (!) Unlike dup2, the close(newfd) and fcntl does not form an atomical operation here! */
  int dup2(int oldfd, int newfd)
  {
    // Check oldfd is valid
    if (-1 == ::fcntl(oldfd, F_GETFL)) [[unlikely]] 
    {
      errno = EBADF;
      return -1;
    }

    if (newfd == oldfd) [[unlikely]] // Return newfd, we know it's valid
      return newfd;

    // If newfd is opened, close it
    if (-1 != ::fcntl(newfd, F_GETFL)) // I might not even need to check that...
      close(newfd); // silent close, as spec says

    return ::fcntl(oldfd, F_DUPFD, newfd);
  }
}
auto main() -> int
{
  int fd1 = open("babar.txt", O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  int fd2 = Reimpl::dup(fd1);
  int fd3 = open("babar.txt", O_RDWR);
  write(fd1, "Hello,", 6);
  write(fd2, "world", 6);
  lseek(fd2, 0, SEEK_SET);
  write(fd1, "HELLO,", 6);
  write(fd3, "Gidday", 6);
  return EXIT_SUCCESS;
}
