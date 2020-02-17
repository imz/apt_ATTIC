// Description								/*{{{*/
// $Id: mmap.h,v 1.1 2002/07/23 17:54:51 niemeyer Exp $
/* ######################################################################

   MMap Class - Provides 'real' mmap or a faked mmap using read().

   The purpose of this code is to provide a generic way for clients to
   access the mmap function. In enviroments that do not support mmap
   from file fd's this function will use read and normal allocated
   memory.

   Writing to a public mmap will always fully comit all changes when the
   class is deleted. Ie it will rewrite the file, unless it is readonly

   The DynamicMMap class is used to help the on-disk data structure
   generators. It provides a large allocated workspace and members
   to allocate space from the workspace in an effecient fashion.

   This source is placed in the Public Domain, do with it what you will
   It was originally written by Jason Gunthorpe.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_MMAP_H
#define PKGLIB_MMAP_H

#ifdef __GNUG__
#pragma interface "apt-pkg/mmap.h"
#endif

#include <string>
#include <limits>
#include <experimental/optional>

#include <apt-pkg/fileutl.h>

using std::string;

/* This should be a 32 bit type, larger tyes use too much ram and smaller
   types are too small. Where ever possible 'unsigned long' should be used
   instead of this internal type */
typedef unsigned int map_ptrloc;

class MMap
{
   protected:

   unsigned long Flags;
   unsigned long iSize;
   void *Base;

   bool Map(FileFd &Fd);
   bool Close(bool DoSync = true);

   public:

   enum OpenFlags {NoImmMap = (1<<0),Public = (1<<1),ReadOnly = (1<<2),
                   UnMapped = (1<<3)};

   // Simple accessors
   inline operator void *() {return Base;};
   inline void *Data() {return Base;};
   inline unsigned long Size() {return iSize;};

   // File manipulators
   bool Sync();
   bool Sync(unsigned long Start,unsigned long Stop);

   MMap(FileFd &F,unsigned long Flags);
   explicit MMap(unsigned long Flags);
   virtual ~MMap();
};

class DynamicMMap : public MMap
{
   public:

   // This is the allocation pool structure
   struct Pool
   {
      unsigned long ItemSize;
      unsigned long Start;
      unsigned long Count;
   };

   protected:

   FileFd *Fd;
   unsigned long WorkSpace;
   Pool *Pools;
   unsigned int PoolCount;

   public:

   // Allocation
   std::experimental::optional<unsigned long> RawAllocate(unsigned long Size,unsigned long Aln = 0);
   std::experimental::optional<unsigned long> Allocate(unsigned long ItemSize);
   std::experimental::optional<unsigned long> WriteString(const char *String,unsigned long Len = std::numeric_limits<unsigned long>::max());
   inline std::experimental::optional<unsigned long> WriteString(const string &S) {return WriteString(S.c_str(),S.length());};
   void UsePools(Pool &P,unsigned int Count) {Pools = &P; PoolCount = Count;};

   DynamicMMap(FileFd &F,unsigned long Flags,unsigned long WorkSpace = 2*1024*1024);
   DynamicMMap(unsigned long Flags,unsigned long WorkSpace = 2*1024*1024);
   virtual ~DynamicMMap();
};

/* Templates */

#include <apt-pkg/error.h>
#include <cassert>

// DynamicMMap::Allocate - Pooled aligned allocation			/*{{{*/
// ---------------------------------------------------------------------
/* This allocates an Item of size ItemSize so that it is aligned to its
   size in the file. */
std::experimental::optional<unsigned long> DynamicMMap::Allocate(unsigned long ItemSize)
{
   assert(ItemSize != 0); /* Actually, we are always called with sizeof(...)
                             compile-time non-zero constant as the argument.
                          */

   // Look for a matching pool entry
   Pool *I;
   Pool *Empty = 0;
   for (I = Pools; I != Pools + PoolCount; I++)
   {
      if (I->ItemSize == 0)
	 Empty = I;
      if (I->ItemSize == ItemSize)
	 break;
   }

   // No pool is allocated, use an unallocated one
   if (I == Pools + PoolCount)
   {
      // Woops, we ran out, the calling code should allocate more.
      if (Empty == 0)
      {
	 _error->Error("Ran out of allocation pools");
	 return std::experimental::nullopt;
      }

      I = Empty;
      I->ItemSize = ItemSize;
      I->Count = 0;
   }

   // Out of space, allocate some more
   if (I->Count == 0)
   {
      I->Count = 20*1024/ItemSize;
      auto idxResult = RawAllocate(I->Count*ItemSize,ItemSize);

      // Has the allocation failed?
      if (!idxResult)
         return std::experimental::nullopt;

      I->Start = *idxResult;
   }

   I->Count--;
   unsigned long Result = I->Start;
   I->Start += ItemSize;
   return Result/ItemSize;
}
									/*}}}*/

#endif
