/* my own malloc (and free)
 - not atomic, not threadsafe
 - does not return 16-bytes aligned memory
 - homemade and certainly not as efficient as glibc

 In the rest of the program, STBY designates the size of a size_t (8 bytes on a 64 bits system)
 
 I implemented the freelist as a single linked list.
 Each free block consists in a size (taking STBY bytes) and a pointer to the next block
 The size represents the area between the zone right after it (overlapping the 'next pointer'),
 and the next block address, or, if there is no next block, the program break.

 When the user requests N bytes, that size is rounded up to the next STBY multiple. If a freeblock
 is big enough, it is returned: its size set to the requested size, another freeblock is created
 for the remainder if needed. The return value points on the zone after the size.
*/

#include <cassert>
#include <unistd.h>
#include <cstdint>
#include <cstdio>

import std;

constexpr size_t STBY = sizeof(size_t); // size_t bytes : 8 on 64 bits

namespace Ham
{
  struct FreeBlock
  {
    size_t sz = 0;                // STBY bytes before the pointer returned by malloc() (the STBY other bytes occupied by 'next' are included in sz)
    FreeBlock* next = nullptr;    // coincides with the pointer returned by malloc()
  };
  
  FreeBlock* FREELIST = nullptr; // Sorted (by construction) single linked list
  const int PAGESIZE = getpagesize();
  const char* HAMTRACE = std::getenv("HAMTRACE"); // simple stdout tracing. Beware: printf is not guaranteed to be malloc-free...

  int i = 0;
  void DumpFreeList()
  {
    puts("FREELIST AT STEP");
    printf("%d\n",i++);
    // print the freelist
    int j = 0;
    for (FreeBlock** ppFreeBlock = &FREELIST; *ppFreeBlock; ppFreeBlock = &(*ppFreeBlock)->next)
    {
      printf("B%d[sz=", j++); // actually dangerous because printf may allocate
      printf("%zu",(*ppFreeBlock)->sz);
      printf("; next=");
      printf("%p] (at %p)\n",(*ppFreeBlock)->next, *ppFreeBlock); fflush(stdout);
    }
    
  }
  
  void* malloc(size_t requested)
  {
    // change the input size to a multiple of 8 bytes so that when it becomes a freeblock there is room for the 'next' pointer
    size_t size = (requested / STBY) * STBY;
    if (!size || requested != size) [[likely]] size += STBY; // round up!
        
    // Try to find a suitable block in the freelist
    FreeBlock** ppFreeBlock = &FREELIST;
    for (; *ppFreeBlock; ppFreeBlock = &(*ppFreeBlock)->next)
    {
      if ((*ppFreeBlock)->sz >= size) // The requested size fits in the block
      {
        // result == the zone after the size (size_t). In the code I avoid pointer arithmetics by casting the pointers to size_t
        void* result = reinterpret_cast<void*>(reinterpret_cast<size_t>(*ppFreeBlock) + STBY);
        if ((*ppFreeBlock)->sz == size)
        {
          // cool: it fits exactly. Just remove the block and return
          *ppFreeBlock = (*ppFreeBlock)->next; // (cur == prev->next now points to pNewFreeblock)
          return result;
        }
        else if ((*ppFreeBlock)->sz >= size + 2*STBY)
        {
          // Split the block, the first sz part is returned, the other is a new freeblock
          // I ask for sz > size + 2*STBY since we need 2*STBY (size, next) bytes for the new freeblock representing remaining memory
          FreeBlock* pNewFreeBlock = reinterpret_cast<FreeBlock*>(reinterpret_cast<size_t>(result) + size); // size_t cast to avoid ptr arithmetic

          // size is rounded to 8N bytes, so it is guaranteed that pNewfreeblock->sz does not overlap with (*ppFreeblock)->next even when requested < 8
          pNewFreeBlock->sz = (*ppFreeBlock)->sz - STBY - size;
          pNewFreeBlock->next = (*ppFreeBlock)->next;
          
          // The current freeblock is shrinked and returned
          (*ppFreeBlock)->sz = size; // the next field is part of the 'payload' and does not matter anymore (I could zero it. or not :-))
          *ppFreeBlock = pNewFreeBlock;
          return result; // old *ppFreeBlock + STBY
        } // if sz was precisely size+STBY, nevermind, I just continue, it was not a good block (it would have lead to an orphaned STBY too smol to be a free block)
      }
    }

    /* At this point, I didn't find any matching block on the freelist. I increase the program break.
       Unless size is a multiple of PAGESIZE (I ignore that case for now (cf. (1)) I will need a trailing freeblock.
       So in addition to 'size' I still need a size_t for the size of the returned block
       +2 size_t at least to represent a FreeBlock following the returned memory

       (1) The worst thing that could happen is a loop of malloc(size) with size+STBY multiple of PAGESIZE
       ... instead of allocating 1 page at a time, I'll allocate 2 pages, the 2nd one would contain an empty freeblock of 4088 bytes (if STBY=8)
       Though the libc malloc() may need more anyway or may not be aligned on a 4096B boundary at all. */
    const size_t allocated = (((size + 3*STBY) / PAGESIZE) + 1) * PAGESIZE;
    FreeBlock* formerPBRK = reinterpret_cast<FreeBlock*>(sbrk(allocated));
    if (reinterpret_cast<void*>(-1) == formerPBRK) // allocation failed
      return nullptr; // propagate errno
    formerPBRK->sz = size; // next doesn't matter; this freeblock will not be in the list, it'll be returned.
    
    // NB: the last freeblock may (or may not) be adjacent to the newly alloc'd space. It's ignored here for time-efficiency purposes.
    // Memory-efficiency idea:
    // IFF the last free block is adjacent to the program break, increase the program break by only (size - LastFreeBlockSize) upper PAGESIZE multiple
    // This could save a lot of memory, e.g the requested size is 1GB, the last freeblock is 999 MB long;
    // we'd only need like 2 pages instead; in the current implementation I allocate 1GB (a few hundreds pages). Of course this is an extreme case and the 999 MB in the middle are not lost for future allocations!
    FreeBlock* newLastFreeBlock = reinterpret_cast<FreeBlock*>(reinterpret_cast<size_t>(formerPBRK) + STBY + size); // size_t cast == avoid pointer arithmetic
    newLastFreeBlock->sz = allocated - size - 2*STBY; // substract the size of the 1st block (size+data) and the newLast size
    newLastFreeBlock->next = nullptr;

    // Have the former last free block point on the new last free block
    *ppFreeBlock = newLastFreeBlock;
    
    return reinterpret_cast<void*>(reinterpret_cast<size_t>(formerPBRK) + STBY);
  }

  void free(void* ptr)
  {
    if (!ptr) [[unlikely]]
      return;
    
    // If a freeblock is pointing on the newly released zone, it could be merged with the current one
    FreeBlock* pNewFreeBlock = reinterpret_cast<FreeBlock*>(reinterpret_cast<size_t>(ptr) - STBY);

    // Loop on existing freeblocks; find the block that will be before the new block
    FreeBlock* pFreeBlockPrev = nullptr;
    if (FREELIST && FREELIST < pNewFreeBlock)
    {
      for (pFreeBlockPrev = FREELIST; pFreeBlockPrev && pFreeBlockPrev->next < pNewFreeBlock; pFreeBlockPrev = pFreeBlockPrev->next)
        ;
    }

    // Find the free block that will follow pNewFreeBlock
    FreeBlock** ppFreeBlockNext = pFreeBlockPrev ? &(pFreeBlockPrev->next) : &FREELIST;

    // Insert the block
    pNewFreeBlock->next = *ppFreeBlockNext;
    *ppFreeBlockNext = pNewFreeBlock;

    // Right collapse ?
    if (reinterpret_cast<size_t>(pNewFreeBlock) + STBY + pNewFreeBlock->sz == reinterpret_cast<size_t>(pNewFreeBlock->next))
    {
      // The "next" block is integrated to the newly added block.
      pNewFreeBlock->sz += pNewFreeBlock->next->sz + STBY;
      pNewFreeBlock->next = pNewFreeBlock->next->next;
    }

    // Left collapse ?
    if (pFreeBlockPrev && reinterpret_cast<size_t>(pFreeBlockPrev) + STBY + pFreeBlockPrev->sz == reinterpret_cast<size_t>(pNewFreeBlock))
    {
      // The newly added block is integrated to the "prev" block.
      pFreeBlockPrev->sz += pNewFreeBlock->sz + STBY;
      pFreeBlockPrev->next = pNewFreeBlock->next;
    }

    // By construction there cannot be "recursive collapse" i.e. two just-collapsed blocks that would collapse with the next one.
  }
}

/* Basic test program ...
   !! I obviously cannot use any C++ code allocating with the real malloc() or new !
   ... it would badly conflict with Ham::malloc() and Ham::free() which uses its own freelist.
   ... At least while my allocator runs. Any new/malloc/... happening BEFORE any Ham::malloc/free work
   do not conflict since my freelist starts empty : at its first call Ham::malloc will go past the program break, hence
   ignoring any existing allocated zone.
*/

struct Chunk // "metainformation"
{
  void* ptr = nullptr;
  size_t sz = 0;
};


constexpr size_t NB_CHUNKS = 500;
constexpr size_t NB_ITER = 10000;

auto main() -> int
{
  std::array<Chunk, NB_CHUNKS> CHUNKS;

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
  for (size_t i = 0; i < NB_ITER; ++i)
  {
    if (Ham::HAMTRACE) // temp trace...
    {
      Ham::DumpFreeList();
    }
    
    // Pick a random chunk among the 500 available chunks
    auto& chunk = CHUNKS[randomNumber(CHUNKS.size())];

    // If it is allocated, free it
    if (chunk.ptr)
    {
      printf("step %zu frees addr %p\n", i, chunk.ptr);
      Ham::free(chunk.ptr);
      chunk.ptr = nullptr;
    }
    else // allocate it
    {
      chunk.sz = randomNumber(10000);
      if (Ham::HAMTRACE)
      {
        printf("step %zu mallocs %zu\n", i, chunk.sz);
      }

      chunk.ptr = Ham::malloc(chunk.sz);
      assert(chunk.ptr);
    }

    printf("-----------end step\n\n\n");
  }

  // Play with small-sized mallocs
  for (size_t i = 0; i <= 64; ++i)
  {
    Ham::DumpFreeList();
    auto& someChunk = CHUNKS[12 + i];
    if (someChunk.ptr)
    {
      printf("small free of %p\n", someChunk.ptr); fflush(stdout);
      Ham::free(someChunk.ptr);
      someChunk.ptr = nullptr;
    }
    // 0 bytes to 64 bytes allocation
    someChunk.sz = i;
    someChunk.ptr = Ham::malloc(i);
    printf("small malloc of %zu at %p\n", i, someChunk.ptr);
    printf("-----------end step\n\n\n"); fflush(stdout);
  }
  
  // Free everything
  for (auto& chunk : CHUNKS)
  {
    if (chunk.ptr)
    {
      Ham::DumpFreeList();
      printf("final free of %p\n", chunk.ptr);
      Ham::free(chunk.ptr);
      chunk.ptr = nullptr;
      printf("-----------end step\n\n\n"); fflush(stdout);
    }
  }

  printf("Final free list :\n");
  Ham::DumpFreeList();

  // There should be one big block in the final freelist.
  assert(Ham::FREELIST && !(Ham::FREELIST->next));
  return 0;
}
