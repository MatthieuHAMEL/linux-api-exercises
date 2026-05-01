#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cerrno>
#include <cstdio>
#include <cstring>

import hamio;
import hamutil;
import std;
using namespace std;

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
  Ham::File outfile;
  if (outputFilePath && -1 == outfile.open(outputFilePath, fileflags))
      return Ham::Diag("file open failed", EXIT_FAILURE);

  Ham::StdStream const StdIn{STDIN_FILENO}, StdOut{STDOUT_FILENO};
  while(true) // Read stdin in loop
  {
    char buf[4096];
    ssize_t rc = StdIn.read(buf, sizeof(buf));
    if (rc == -1)
      return Ham::Diag("read stdin failed", EXIT_FAILURE);
    else if (rc == 0) break; // EOF reached

    // Print the text on stdout and in the file (if there is one open)
    if (!StdOut.write_all(buf, rc))
      return Ham::Diag("write stdout failed", EXIT_FAILURE);
    if (outfile.is_open() && !outfile.write_all(buf, rc))
      return Ham::Diag("write outfile failed", EXIT_FAILURE);
  }

  // normal close
  if (outfile.is_open() && -1 == outfile.close())
    return Ham::Diag("close outfile failed", EXIT_FAILURE);
  return 0;
}
