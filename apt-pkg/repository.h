// CNC:2002-07-03

#ifndef PKGLIB_REPOSITORY_H
#define PKBLIB_REPOSITORY_H

#include <string>
#include <map>

#include <apt-pkg/sourcelist.h>

using std::map;

class pkgRepository
{
   protected:


   struct Checksum {
      unsigned long Size;
      string MD5;
      string SHA1;
   };

   map<string,Checksum> IndexChecksums; // path -> checksum data

   bool GotRelease;

   public:

   string URI;
   string Dist;
   vector<string> FingerPrint;
   string RootURI;

   bool Acquire;

   bool ParseRelease(const string &File);
   bool HasRelease() const { return GotRelease; }

   bool IsAuthenticated() const { return !FingerPrint.empty(); }
   bool FindChecksums(const string &URI,unsigned long &Size, string &MD5);

   pkgRepository(const string &URI, const string &Dist, const pkgSourceList::Vendor *Vendor,
		 const string &RootURI)
      : GotRelease(0), URI(URI), Dist(Dist), RootURI(RootURI),
	Acquire(1)
   {
      if (Vendor) FingerPrint = Vendor->FingerPrint;
   }

};

#endif

// vim:sts=3:sw=3
