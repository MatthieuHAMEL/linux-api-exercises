## Learning approach

I peek at exercises from "The Linux Programming Interface" (M. Kerrisk)

Unlike the book, I use modern C++ (23) for many reasons:
  - I try to produce memory-safe and/or type-safe wrappers (I don't just call the linux API in raw C); 
    - This is a chance to think deeper about how things work.
  - Take profit of g++
  - Use features like modules, namespaces, templates, [[(un)likely]], RAII/move semantics, constexpr computation, string_views
  - It is an opportunity to learn more about modern C++ & its ecosystem 
    - (Specifically, get C++23/26 to work on debian stable with CMAKE was challenging but interesting)

The primary goal is to work with the Linux API, therefore the use of the C++ standard library is limited.

(For instance, the goal of the "File IO" chapter is to use read, write and lseek; obviously not std::ofstream or std::filesystem)
