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

  template <typename BufType, ssize_t ReadOrWrite(int fd, BufType buf, size_t cnt)>
  ssize_t ReadOrWritev(int fd, const struct iovec *iov, int iovcnt)
  {
    if (!iov || iovcnt < 0) [[unlikely]]
    {
      errno = EINVAL;
      return -1;
    }

    ssize_t total = 0;
    for (int i = 0; i < iovcnt; ++i)
    {
      if (iov[i].iov_len == 0) [[unlikely]]
        continue;

      if (iov[i].iov_len > static_cast<size_t>(std::numeric_limits<ssize_t>::max() - total)) [[unlikely]]
      {
        errno = EINVAL;
        return -1;
      }
      ssize_t rc = ReadOrWrite(fd, iov[i].iov_base, iov[i].iov_len);
      if (rc == -1) [[unlikely]] // propagate
        return -1;
      total += rc;
      if (static_cast<size_t>(rc) != iov[i].iov_len) // not everything has been read/written so no need to write iov[i+1], etc.
        break;
    }
    return total;
  }

  ssize_t writev(int fd, const struct iovec* iov, int iovcnt)
  {
    return ReadOrWritev<const void*, &::write>(fd, iov, iovcnt);
  }
  ssize_t readv(int fd, const struct iovec* iov, int iovcnt)
  {
    return ReadOrWritev<void*, &::read>(fd, iov, iovcnt);
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

  constexpr std::string_view STR0 = "first ";
  constexpr std::string_view STR1 = "second \n";
  struct iovec iov[2];
  iov[0].iov_base = const_cast<void*>(reinterpret_cast<const void*>(STR0.data()));
  iov[0].iov_len = STR0.length();
  iov[1].iov_base = const_cast<void*>(reinterpret_cast<const void*>(STR1.data()));
  iov[1].iov_len = STR1.length();

  ssize_t nwritten = Reimpl::writev(fd1, iov, 2);
  if (nwritten == -1)
    return Ham::Diag("Error in writev", EXIT_FAILURE);
  
  printf("written -> %zu\n", static_cast<size_t>(nwritten));

  
  return EXIT_SUCCESS;
}
