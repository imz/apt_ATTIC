#ifndef PKGLIB_CKSUM_H
#define PKGLIB_CKSUM_H

#include <string>
#include <sys/types.h>

class Cksum {
   public:

   unsigned long size;

   string method;
   string hash;

   Cksum(const unsigned long s,
         const string & m, const string & h):
      size(s),
      method(m), hash(h)
   {}

   // convenience for constructing from a result from stat
   Cksum(const off_t s,
         const string & m, const string & h):
      size(s), // FIXME: care about correct conversion
      method(m), hash(h)
   {}
};

#endif
