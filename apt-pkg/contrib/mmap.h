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

#include <string>
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
   size_t iSize;
   void *Base;

   bool Map(FileFd &Fd);
   bool Close(bool DoSync = true);

   public:

   enum OpenFlags {NoImmMap = (1<<0),Public = (1<<1),ReadOnly = (1<<2),
                   UnMapped = (1<<3)};

   // Simple accessors
   inline operator void *() {return Base;}
   inline void *Data() {return Base;}
   inline size_t Size() {return iSize;}

   // File manipulators
   bool Sync();
   bool Sync(unsigned long Start,unsigned long Stop);

   MMap(FileFd &F,unsigned long Flags);
   MMap(unsigned long Flags);
   virtual ~MMap();
};

class DynamicMMap : public MMap
{
   public:

   // This is the allocation pool structure
   struct Pool
   {
      size_t ItemSize;
      unsigned long Start;
      unsigned long Count;
   };

   protected:

   FileFd *Fd;
   size_t WorkSpace;
   Pool *Pools;
   unsigned int PoolCount;

   public:

   // Allocation
   unsigned long RawAllocate(size_t Size,size_t Aln = 0);
   unsigned long Allocate(size_t ItemSize);
   unsigned long WriteString(const char *String,size_t Len);
   inline unsigned long WriteString(string const S) {return WriteString(S.c_str(),S.length());}
   void UsePools(Pool &P,unsigned int const Count) {Pools = &P; PoolCount = Count;}

   DynamicMMap(FileFd &F,unsigned long Flags,size_t RequestedWorkSpace = 2*1024*1024);
   DynamicMMap(unsigned long Flags,size_t WorkSpace = 2*1024*1024);
   virtual ~DynamicMMap();
};

#endif
