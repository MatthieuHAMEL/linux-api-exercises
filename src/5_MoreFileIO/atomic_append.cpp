/*
  Run :
 atomic_append f1 1000000 & atomic_append f1 1000000
 atomic_append f2 1000000 x & atomic_append f2 1000000 x
  Then compare the sizes of f1 and f2.
  
 When a file is opened with O_APPEND, lseek(fd, 0, SEEK_END) is performed atomically before any write().
 This programs shows that two concurrent processes are able to append data to the file without conflicting
 in O_APPEND mode. But if the lseek is not done atomically, they conflict with each other.
*/

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

import std;
import hamio;
import hamutil;
using namespace std;
using namespace Ham;

void usage()
{
  fputs("atomic_append file num-bytes [x]\n  \
        x means non-atomic append.", stderr);
}

auto main(int argc, char* argv[]) -> int
{
  if (argc < 3 || argc > 4) [[unlikely]]
  {
    usage();
    return EXIT_FAILURE;
  }

  // x given as the last arg == don't open in append mode
  int const appendMode = (argc == 4 && argv[3] && string_view(argv[3]) == "x"sv) ? 0 : O_APPEND;
  int const numBytes = atoi(argv[2]); // large files not supported
  if (0 == numBytes) [[unlikely]] // error or nonsense anyway
    return Diag("num-bytes is 0");
  
  // Open the given file, create it if necessary
  auto f = File(argv[1], appendMode | O_WRONLY | O_CREAT);
  if (!f.is_open()) [[unlikely]]
    return Diag("Couldn't open the file", EXIT_FAILURE);

  for (int i = 0; i < numBytes; ++i)
  {
    if (!appendMode) // try to do a manual append by seeking the end of the file before writing
    {
      if (-1 == ::lseek(f.fd(), 0, SEEK_END)) [[unlikely]]
        return Diag("Couldn't seek for end of file", EXIT_FAILURE);
    }
    constexpr char a = 'a'; // a
    f.write_all(&a, 1);
  }

  if (-1 == f.close()) [[unlikely]]
    return Diag("Couldn't close the file", EXIT_FAILURE);

  return EXIT_SUCCESS;
}
