// my own malloc

import std;

namespace Reimpl
{
  void* FREELIST = nullptr;

  void* malloc(size_t size)
  {
    // TODO
    return nullptr;
  }

  void free(void* ptr)
  {
    // TODO
  }
}

// Basic test program ...
// !! I obviously cannot use any C++ code allocating with the real malloc() or new !
// ... it would badly conflict with Reimpl::malloc() and Reimpl::free() which uses its own freelist.

struct Chunk // "metainformation"
{
  void* ptr = nullptr;
  size_t sz = 0;
};

auto main() -> int
{
  std::array<Chunk, 500> CHUNKS;

  auto randomNumber = [](size_t n) // allocation free rng (thx chatgpt !)
  {
    static uint64_t x = 0x9e3779b97f4a7c15ULL;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    x *= 0x2545F4914F6CDD1DULL;
    return x % CHUNKS.length();
  };

  // Random sequence of malloc or free of random sizes, on random chunks
  for (int i = 0; i < 10000; ++i)
  {
    // Pick a random chunk among the 500 available chunks
    auto& chunk = CHUNKS[randomNumber(CHUNKS.length())];

    // If it is allocated, free it
    if (chunk.ptr)
    {
      Reimpl::free(chunk.ptr);
      chunk.ptr = nullptr;
    }
    else // allocate it
    {
      chunk.sz = randomNumber(10000)
      chunk.ptr = Reimpl::malloc(chunk.sz);
      assert(chunk.ptr);
    }
  }

  // Play with small-sized mallocs
  for (size_t i = 0; i <= 64; ++i)
  {
    auto& someChunk = CHUNKS[12 + i];
    if (someChunk.ptr)
    {
      Reimpl::free(someChunk.ptr);
      someChunk.ptr = nullptr;
    }
    // 0 bytes to 64 bytes allocation
    someChunk.sz = i;
    someChunk.ptr = Reimpl::malloc(i);
  }
  
  // Free everything
  for (auto& chunk : CHUNKS)
  {
    if (chunk.ptr)
    {
      Reimpl::free(chunk.ptr);
      chunk.ptr = nullptr;
    }
  }
  return 0;
}
