#include <unistd.h>
#include <cstdio>

void usage()
{
  fputs("cph file1 file2\n", stderr);
}

/* cph : copy file1 to file2 including the holes
   Equivalent to "cp --sparse=always" */
auto main(int argc, char *argv[]) -> int
{
  // Parse command line : a first sanity check...
  if (argc > 3)
  {
    usage();
    return EXIT_FAILURE;
  }

  
  off_t const fullsize = ::lseek(int fd, off_t offset, int whence);

  return 0;
}
