#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cassert>

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

  int setenv(const char *name, const char *value, int overwrite)
  {
    if (!name || name[0] == '\0' || !value || strchr(name, '='))
    {
      errno = EINVAL;
      return -1;
    }

    if (overwrite == 0 && getenv(name))
      return 0; // nothing to do
    
    char* newstr = static_cast<char*>(malloc(strlen(name) + strlen(value) + 2)); // name=value + \0
    if (!newstr)
    {
      errno = ENOMEM;
      return -1;
    }
    sprintf(newstr, "%s=%s", name, value);
    return ::putenv(newstr); // leaky by design; the real setenv probably tracks the allocated strings somewhere
    return 0;
  }

  /* As said by the assignment, and just like in glibc, unsetenv does not free the removed strings since it cannot
     know how they were allocated. */
  int unsetenv(const char *name)
  {
    if (!name || *name == '\0' || strchr(name, '=')) [[unlikely]]
    {
      errno = EINVAL;
      return -1;
    }

    size_t const nameLen = strlen(name);
    // if !getenv(name) then there is nothing to do
    // but since I may directly modify environ anyway, it is more efficient to loop manually once
    for (char** ppEnvVar = environ; *ppEnvVar != nullptr;)
    {
      const char* pEnviron = *ppEnvVar;      
      if (!strncmp(name, pEnviron, nameLen) && pEnviron[nameLen] == '=') [[unlikely]] // found!
      {
        // Shift everyone left
        for (char** ppTmp = ppEnvVar; *ppTmp != nullptr; ++ppTmp)
          *ppTmp = *(ppTmp + 1); // will include the null slot at the end

        // the assignment says I should do it until the end if there are multiple identical keys... so no break.
      }
      else // Increment only if the rest wasn't shifted!
      {
        ++ppEnvVar;
      }
    }
    return 0;
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

  /////////// Chapter 6 (processes) ///////////////
  // Quick tests for setenv
  assert(0 == Reimpl::setenv("bonjour", "martin", 0));
  assert(0 == Reimpl::setenv("bonjour", "florence", 0));
  assert(0 == Reimpl::setenv("bonjour", "pierre", /*overwrite*/1));
  assert(-1 == Reimpl::setenv(nullptr, "toto", 1));
  assert(-1 == Reimpl::setenv("tata", nullptr, 0));
  assert(-1 == Reimpl::setenv("", "babar", 0));

  assert(0 == Reimpl::unsetenv("bonjour"));
  assert(0 == Reimpl::unsetenv("IAmNotAnEnvironmentVariable"));
  assert(-1 == Reimpl::unsetenv(nullptr));
  
  // Read the current environment and print it:
  for (char** ppEnvVar = environ; *ppEnvVar != nullptr; ++ppEnvVar)
    printf("--) %s\n", *ppEnvVar);
  
  return EXIT_SUCCESS;
}
