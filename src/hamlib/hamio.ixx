export module hamio;

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstring>

export namespace Ham
{
  class FD
  {
  public:
    FD() = default;
  
    bool write_all(const char* buf, size_t cnt) const
    {
      if (_fd == -1) [[unlikely]] return false; // [error reporting?] TODO

      while (cnt > 0)
      {
        ssize_t rc = ::write(_fd, buf, cnt);
        if (rc == -1)
        {
          if (errno == EINTR) continue;
          return false;
        }

        buf += rc;
        cnt -= static_cast<size_t>(rc);
      }  
      return true;
    }

    ssize_t read(void* buf, size_t maxcnt) const {
      ssize_t rc;
      do
      {
        rc = ::read(_fd, buf, maxcnt);
      } while (rc == -1 && errno == EINTR);
      return rc;
    }
  
    int fd() const { return _fd; }
    bool is_open() const { return _fd != -1; }
  
  protected:
    int _fd = -1;
  };

  class File : public FD
  {
  public:
    File() = default;
  
    File(const char* iFilePath, int iFlags) {
      this->open(iFilePath, iFlags);
    }

    // Avoid double close
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    // Move
    File(File&& other) noexcept {
      _fd = other._fd;
      other._fd = -1;
    }

    File& operator=(File&& other) noexcept {
      if (this != &other) {
        close();
        _fd = other._fd;
        other._fd = -1;
      }
      return *this;
    }

    /* NB: this destructor guarantees the resource cleaning (RAII) though it does not
       allow to know whether close returned an error or not.
       => It is useful in a case of an early return (after some other error for instance), but
       in the nominal path it is better to call this->close() manually and check for errors properly. */
    ~File() {
      this->close();
    }

    int open(const char* iFilePath, int iFlags) {
      close();
      // nb: mode (the 3rd arg) is ignored if iFlags doesn't have O_CREAT or O_TMPFILE
      _fd = ::open(iFilePath, iFlags, S_IRUSR | S_IWUSR);
      return _fd; // todo in case of error ensure errno is not lost
    }

    int close() {
      if (_fd == -1) [[unlikely]] return -1;
      int rc = ::close(_fd);
      _fd = -1;
      return rc;
    }
  };

  class StdStream : public FD
  {
  public:
    StdStream(int iStdFileno) {
      _fd = iStdFileno;
    }
  };
}
