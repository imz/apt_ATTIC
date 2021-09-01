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
   string ComprMethod;
   string CheckMethod;

   public:

   string URI;
   string Dist;
   vector<string> FingerPrint;
   string RootURI;

   bool Acquire;

   // LORG:2006-02-21 make these methods virtual
   virtual bool ParseRelease(string File);
   virtual bool HasRelease() const { return GotRelease; }

   virtual bool IsAuthenticated() const { return !FingerPrint.empty(); };
   virtual bool FindChecksums(string URI,unsigned long &Size, string &MD5);
   // LORG:2006-02-23
   virtual string GetComprMethod() {return ComprMethod;};
   virtual string GetCheckMethod() {return CheckMethod;};

   pkgRepository(string URI,string Dist, const pkgSourceList::Vendor *Vendor,
		 string RootURI)
      : GotRelease(0), URI(URI), Dist(Dist), RootURI(RootURI),
	Acquire(1)
   {
      if (Vendor) FingerPrint = Vendor->FingerPrint;
      ComprMethod = "bz2";
      CheckMethod = "SHA1-Hash";
   };

};

#endif

// vim:sts=3:sw=3
