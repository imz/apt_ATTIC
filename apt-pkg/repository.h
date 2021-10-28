// CNC:2002-07-03

#ifndef PKGLIB_REPOSITORY_H
#define PKBLIB_REPOSITORY_H

#include <string>
#include <map>
#include <optional>

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cksum.h>

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

   virtual string GetCheckMethod() {return CheckMethod;};

   public:

   string URI;
   string Dist;
   vector<string> FingerPrintList;
   string RootURI;

   bool Acquire;

   // LORG:2006-02-21 make these methods virtual
   virtual bool ParseRelease(string File);
   virtual bool HasRelease() const { return GotRelease; }

   virtual bool IsAuthenticated() const { return !FingerPrintList.empty(); }
   std::optional<Cksum> FindChecksums(string URI);
   // LORG:2006-02-23
   virtual string GetComprMethod() {return ComprMethod;};

   pkgRepository(string URI,string Dist, const pkgSourceList::Vendor *Vendor,
		 string RootURI)
      : GotRelease(0), URI(URI), Dist(Dist), RootURI(RootURI),
	Acquire(1)
   {
      if (Vendor) FingerPrintList = Vendor->FingerPrintList;
      ComprMethod = "bz2";
      CheckMethod = "MD5-Hash";
   };

};

#endif

// vim:sts=3:sw=3
