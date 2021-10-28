// Description								/*{{{*/
// $Id: hashes.h,v 1.1 2002/07/23 17:54:51 niemeyer Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions

   This is just used to make building the methods simpler, this is the
   only interface required..

   ##################################################################### */
									/*}}}*/
#ifndef APTPKG_HASHES_H
#define APTPKG_HASHES_H

#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>
#include <apt-pkg/rhash.h>

class Hashes
{
   public:

   MD5Summation MD5;
   raptHash BLAKE2b;

   inline bool Add(const unsigned char *Data,unsigned long Size)
   {
      return MD5.Add(Data,Size) && BLAKE2b.Add(Data,Size);
   }
   inline bool Add(const char *Data) {return Add((unsigned char *)Data,strlen(Data));}
   bool AddFD(int Fd,unsigned long Size);
   inline bool Add(const unsigned char *Beg,const unsigned char *End)
                  {return Add(Beg,End-Beg);}
   Hashes () : BLAKE2b("BLAKE2b") {}
};

#endif
