#ifndef PKGLIB_CKSUM_H
#define PKGLIB_CKSUM_H

#include <string>

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
};

#endif
