#ifndef PKGLIB_CKSUM_H
#define PKGLIB_CKSUM_H

#include <string>

class Cksum {
   public:
   string method;
   string hash;

   Cksum(const string & m, const string & h):
      method(m), hash(h)
   {}
};

#endif
