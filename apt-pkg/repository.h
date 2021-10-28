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
      unsigned long long Size;
      string MD5;
      string SHA1;
   };

   map<string,Checksum> IndexChecksums; // path -> checksum data

   bool GotRelease;
   string CheckMethod;

   public:

   string URI;
   string Dist;
   vector<string> FingerPrint;
   string RootURI;

   bool Acquire;

   // PM 2006-21-02 make these methods virtual
   virtual bool ParseRelease(const string &File);
   virtual bool HasRelease() const { return GotRelease; }

   virtual bool IsAuthenticated() const { return !FingerPrint.empty(); };
   virtual bool FindChecksums(const string &URI,decltype(Checksum::Size) &Size, string &MD5) const;
   virtual string GetCheckMethod() {return CheckMethod;};

   pkgRepository(const string &URI, const string &Dist, const pkgSourceList::Vendor *Vendor,
		 const string &RootURI)
      : GotRelease(0), URI(URI), Dist(Dist), RootURI(RootURI),
	Acquire(1)
   {
      if (Vendor) FingerPrint = Vendor->FingerPrint;
      CheckMethod = "MD5-Hash";
   };

};

#endif

// vim:sts=3:sw=3
