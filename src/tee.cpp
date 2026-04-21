#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstring>

import std;
using namespace std;

class FD
{
public:
  FD() = default;
  
  bool write_all(const char* buf, size_t cnt) const
  {
    if (_fd == -1) [[unlikely]] return false; // how to report the error?

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

class File : public FD // to be put in a module
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
    _fd = ::open(iFilePath, iFlags, S_IRUSR | S_IWUSR);
    return _fd;
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

int diagErr(const char* msg, int ret=EXIT_FAILURE)
{
  int e = errno;
  fprintf(stderr, "%s: %s\n", msg, strerror(e));
  return ret;
}

// Not used anymore here...
[[noreturn]] void diagErrAndExit(const char* msg)
{
  diagErr(msg);
  std::exit(EXIT_FAILURE);
}

void usage()
{
  fputs("tee [-a] [file]\n", stderr);
}

/* My tee implementation in C++ */
auto main(int argc, char *argv[]) -> int
{
  // Parse command line : a first sanity check...
  if (argc > 3)
  {
    usage();
    return EXIT_FAILURE;
  }

  int fileflags = O_CREAT|O_WRONLY|O_TRUNC;
  const char* outputFilePath = nullptr;
  for (int i = 1; i < argc; ++i)
  {
    string_view opt = argv[i];
    if (opt == "-a") // append
    {
      fileflags |= O_APPEND;
      fileflags &= ~O_TRUNC;
    }
    else if ((!opt.empty() && opt[0] == '-') || outputFilePath) // Unknown opt OR something that could be a path but it is set already
    {
      usage();
      return EXIT_FAILURE;
    }
    else // filename
    {
      outputFilePath = argv[i];
    }
  }

  // If there is a specified file, open it
  File outfile;
  if (outputFilePath && -1 == outfile.open(outputFilePath, fileflags))
      return diagErr("file open failed", EXIT_FAILURE);

  StdStream const StdIn{STDIN_FILENO}, StdOut{STDOUT_FILENO};
  while(true) // Read stdin in loop
  {
    char buf[4096];
    ssize_t rc = StdIn.read(buf, sizeof(buf));
    if (rc == -1)
      return diagErr("read stdin failed", EXIT_FAILURE);
    else if (rc == 0) break; // EOF reached

    // Print the text on stdout and in the file (if there is one open)
    if (!StdOut.write_all(buf, rc))
      return diagErr("write stdout failed", EXIT_FAILURE);
    if (outfile.is_open() && !outfile.write_all(buf, rc))
      return diagErr("write outfile failed", EXIT_FAILURE);
  }

  // normal close
  if (outfile.is_open() && -1 == outfile.close())
    return diagErr("close outfile failed", EXIT_FAILURE);
  return 0;
}
