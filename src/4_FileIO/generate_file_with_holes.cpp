#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

import hamio;
import hamutil;
import std;

auto main() -> int
{
  auto f = Ham::File("foo.txt", O_WRONLY|O_CREAT);
  if (!f.is_open()) [[unlikely]]
    return Ham::Diag("Error opening file for writing", EXIT_FAILURE);

  constexpr std::string_view sv = "Hello I am a text";
  if (!f.write_all(sv.data(), sv.length()))
    return Ham::Diag("Error writing the file", EXIT_FAILURE);

  ::lseek(f.fd(), sv.length() + 150, SEEK_SET);
  
  if (!f.write_all(sv.data(), sv.length()))
    return Ham::Diag("Error writing the file (2)", EXIT_FAILURE);

  ::lseek(f.fd(), sv.length() + 5, SEEK_CUR);

  if (!f.write_all(sv.data(), sv.length()))
    return Ham::Diag("Error writing the file", EXIT_FAILURE);

  pwrite(f.fd(), "toto", 4, 0);
  
  return 0;
}
