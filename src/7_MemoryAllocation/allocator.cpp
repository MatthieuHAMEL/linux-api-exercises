// my own malloc

#include <cassert>
#include <unistd.h>
#include <cstdint>

import std;

namespace Reimpl
{
  struct FreeBlock
  {
    size_t sz = 0;                // sizeof(size_t) bytes before the pointer returned by malloc()
    FreeBlock* next = nullptr;    // coincides with the pointer returned by malloc()
  };
  
  FreeBlock* FREELIST = nullptr; // Sorted (by construction) single linked list
  const int PAGESIZE = getpagesize();
  
  void* malloc(size_t size)
  {
    // Try to find a suitable block in the freelist
    FreeBlock** ppFreeBlock = &FREELIST;
    for (; *ppFreeBlock; ppFreeBlock = &(*ppFreeBlock)->next)
    {
      if ((*ppFreeBlock)->sz >= size) // The requested size fits in the block
      {
        void* result = static_cast<void*>(*ppFreeBlock + sizeof(size_t)); // Return the zone after the size
        if ((*ppFreeBlock)->sz == size)
        {
          // cool: it fits exactly. Just remove the block and return
          *ppFreeBlock = (*ppFreeBlock)->next; // (cur == prev->next now points to pNewFreeblock)
          return result;
        }
        else if ((*ppFreeBlock)->sz >= size + sizeof(size_t))
        {
          // Split the block, the first sz part is returned, the other is a new freeblock
          // I ask for sz > size + sizeof(size_t) since the new freeblock has to have minimum 2 bytes (sz, next)
          FreeBlock* pNewFreeBlock = reinterpret_cast<FreeBlock*>(result + size);
          pNewFreeBlock->sz = (*ppFreeBlock)->sz - sizeof(size_t) - size;
          pNewFreeBlock->next = (*ppFreeBlock)->next;
          
          // The current freeblock is shrinked and returned
          (*ppFreeBlock)->sz = size; // the next field is part of the 'payload' and does not matter anymore
          *ppFreeBlock = pNewFreeBlock;
          return result; // old *ppFreeBlock + sizeof(size_t)
        } // if sz was precisely size+1, nevermind, I just continue considering it was not a good block
      }
    }

    // At this point, I didn't find any matching block on the freelist. Increase the program break, represent its beginning as a FreeBlock
    // unless size is a multiple of PAGESIZE (which I ignore) I still need 16 bytes to represent a FreeBlock following the returned memory
    const size_t allocated = ((size + 2*sizeof(size_t) / PAGESIZE) + 1) * PAGESIZE;
    FreeBlock* formerPBRK = reinterpret_cast<FreeBlock*>(sbrk(allocated));
    if (reinterpret_cast<void*>(-1) == formerPBRK) // allocation failed
      return nullptr; // propagate errno
    formerPBRK->sz = size; // next doesn't matter; this freeblock will not be in the list, it'll be returned.
    // NB: the last freeblock may (or may not) be adjacent to the newly alloc'd space. It's ignored here for time-efficiency purposes.
    // Memory-efficiency idea:
    // IFF the last free block is adjacent to the program break, increase the program break by only (size - LastFreeBlockSize) upper PAGESIZE multiple
    // This could save a lot of memory, e.g the requested size is 1GB, the last freeblock is 999 MB long;
    // we'd only need like 2 pages instead; in the current implementation I allocate 1GB. Of course this is an extreme case and the 999 MB will not be lost in future allocations!
    FreeBlock* newLastFreeBlock = reinterpret_cast<FreeBlock*>(formerPBRK + sizeof(size_t) + size);
    newLastFreeBlock->sz = allocated - size - 2*sizeof(size_t); // substract the size of the 1st block (size+data) and the newLast size
    newLastFreeBlock->next = nullptr;

    // Have the former last free block point on the new last free block
    *ppFreeBlock = newLastFreeBlock;
    
    return formerPBRK + sizeof(size_t);
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
    return x % n;
  };

  // Random sequence of malloc or free of random sizes, on random chunks
  for (int i = 0; i < 10000; ++i)
  {
    // Pick a random chunk among the 500 available chunks
    auto& chunk = CHUNKS[randomNumber(CHUNKS.size())];

    // If it is allocated, free it
    if (chunk.ptr)
    {
      Reimpl::free(chunk.ptr);
      chunk.ptr = nullptr;
    }
    else // allocate it
    {
      chunk.sz = randomNumber(10000);
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
