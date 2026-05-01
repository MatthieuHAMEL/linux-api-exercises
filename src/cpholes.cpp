#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

import hamio;
import hamutil;
import hampath;

void usage()
{
  fputs("cpholes file1 file2\n", stderr);
}

/* cph : copy file1 to file2 including the holes
   Equivalent to "cp --sparse=always" */
auto main(int argc, char *argv[]) -> int
{
  if (argc > 3)
  {
    usage();
    return EXIT_FAILURE;
  }

  // Open file1 and file2
  auto f1 = Ham::File(argv[1], O_RDONLY);
  if (!f1.is_open()) [[unlikely]]
    return Ham::Diag("Error opening file1 for reading", EXIT_FAILURE);

  // I don't support the -f mode
  // => I check the file does not exist to avoid accidents
  auto path_f2 = Ham::Path(argv[2]);
  if (path_f2.exists(F_OK)) [[unlikely]]
    return Ham::Diag("file2 already exists", EXIT_FAILURE);
  
  auto f2 = path_f2.open(O_CREAT | O_WRONLY);
  if (!f2.is_open()) [[unlikely]]
    return Ham::Diag("Error opening file2 for writing", EXIT_FAILURE);

  // Get the full size of the file by seeking its end
  off_t const fullsize = ::lseek(f1.fd(), 0, SEEK_END);
  if (-1 == fullsize) [[unlikely]]
    return Ham::Diag("Couldn't get file1 size", EXIT_FAILURE);

  // Reposition to the beginning
  if (-1 == ::lseek(f1.fd(), 0, SEEK_SET)) [[unlikely]]
    return Ham::Diag("Unexpected lseek(set) error", EXIT_FAILURE);
    
  // Now I use the linux >= 3.1 API to get data and holes alternatively
  // until fullsize is reached
  off_t total = 0;
  char buf[4096];
  while (true)
  {
    // Read as much as possible from file1 and write it to file2
    while (true)
    {
      ssize_t rc = f1.read(buf, sizeof(buf));
      if (-1 == rc) [[unlikely]]
        return Ham::Diag("Unexpected read error", EXIT_FAILURE);
      if (0 == rc)
        break;
      total += rc;
      
      if (!f2.write_all(buf, rc)) [[unlikely]]
        return Ham::Diag("Unexpected write(data) error", EXIT_FAILURE);
    }

    if (total == fullsize) // done; else there's a hole
      break;

    // rc - total is the size of the hole
    off_t rc = ::lseek(f1.fd(), fullsize, SEEK_DATA);
    if (-1 == rc) [[unlikely]]
      return Ham::Diag("Unexpected lseek error", EXIT_FAILURE);
    
    memset(buf, '\0', rc - total);
    if (!f2.write_all(buf, rc - total)) [[unlikely]]
      return Ham::Diag("Unexpected write(hole) error", EXIT_FAILURE);
  }

  if (-1 == f1.close()) [[unlikely]]
    return Ham::Diag("close file1 failed", EXIT_FAILURE);
  if (-1 == f2.close()) [[unlikely]]
    return Ham::Diag("close file2 failed", EXIT_FAILURE);

  return 0;
}
