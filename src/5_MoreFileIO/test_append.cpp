/* This program illustrates how using O_APPEND "protects" the opened file existing data
   by seeking EOF before every write. (erasing any other lseek done by the user) */

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
  fputs("test_append [file]", stderr);
}

auto main(int argc, char* argv[]) -> int
{
  if (argc != 2)
  {
    usage();
    return EXIT_FAILURE;
  }
  
  auto f = File(argv[1], O_APPEND | O_WRONLY);
  if (!f.is_open())
    return Diag("Couldn't open the file", EXIT_FAILURE);
  
  if (-1 == ::lseek(f.fd(), 0, SEEK_SET))
    return Diag("Couldn't seek at the beginning of the file", EXIT_FAILURE);
  
  constexpr string_view text = "Hello World!";
  f.write_all(text.data(), text.length());
  
  if (-1 == f.close())
    return Diag("Couldn't close the file", EXIT_FAILURE);

  return EXIT_SUCCESS;
}
