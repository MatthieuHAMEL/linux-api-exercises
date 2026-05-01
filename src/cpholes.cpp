#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

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
  if (argc != 3)
  {
    usage();
    return EXIT_FAILURE;
  }

  // Open file1 and file2
  auto f1 = Ham::File(argv[1], O_RDONLY | O_CLOEXEC);
  if (!f1.is_open()) [[unlikely]]
    return Ham::Diag("Error opening file1 for reading", EXIT_FAILURE);

  // I don't support the -f mode
  // => I check the file does not exist to avoid accidents
  auto f2 = Ham::File(argv[2], O_CREAT | O_WRONLY | O_EXCL | O_CLOEXEC);
  if (!f2.is_open()) [[unlikely]]
    return Ham::Diag("Error opening file2 for writing", EXIT_FAILURE);

  // Get the full size of the file by seeking its end
  off_t const fullsize = ::lseek(f1.fd(), 0, SEEK_END);
  if (-1 == fullsize) [[unlikely]]
    return Ham::Diag("Couldn't get file1 size", EXIT_FAILURE);

  // Reposition to the beginning
  if (-1 == ::lseek(f1.fd(), 0, SEEK_SET)) [[unlikely]]
    return Ham::Diag("Unexpected lseek(set) error", EXIT_FAILURE);
    
  // Now get data and holes alternatively until fullsize is reached
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

    off_t rc = ::lseek(f1.fd(), total, SEEK_DATA); // SEEK_DATA: linux 3.1
    if (-1 == rc) [[unlikely]]
    {
      if (errno == ENXIO) break; // trailing hole!
      return Ham::Diag("Unexpected lseek (file1) error", EXIT_FAILURE);
    }
    total += rc;

    // Create the hole in file2
    if (-1 == ::lseek(f2.fd(), rc, SEEK_CUR))
      return Ham::Diag("Unexpected lseek (file2) error", EXIT_FAILURE);
  }

  // Give file2 its final size with respect to file1's trailing hole
  if (::ftruncate(f2.fd(), fullsize) == -1)
    return Ham::Diag("ftruncate file2 failed", EXIT_FAILURE);

  if (-1 == f1.close()) [[unlikely]]
    return Ham::Diag("close file1 failed", EXIT_FAILURE);
  if (-1 == f2.close()) [[unlikely]]
    return Ham::Diag("close file2 failed", EXIT_FAILURE);

  return 0;
}
