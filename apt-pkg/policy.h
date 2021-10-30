// Description								/*{{{*/
// $Id: policy.h,v 1.1 2002/07/23 17:54:50 niemeyer Exp $
/* ######################################################################

   Package Version Policy implementation

   This implements the more advanced 'Version 4' APT policy engine. The
   standard 'Version 0' engine is included inside the DepCache which is
   it's historical location.

   The V4 engine allows the user to completly control all aspects of
   version selection. There are three primary means to choose a version
    * Selection by version match
    * Selection by Release file match
    * Selection by origin server

   Each package may be 'pinned' with a single criteria, which will ultimately
   result in the selection of a single version, or no version, for each
   package.

   Furthermore, the default selection can be influenced by specifying
   the ordering of package files. The order is derived by reading the
   package file preferences and assigning a priority to each package
   file.

   A special flag may be set to indicate if no version should be returned
   if no matching versions are found, otherwise the default matching
   rules are used to locate a hit.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_POLICY_H
#define PKGLIB_POLICY_H

#include <apt-pkg/depcache.h>
#include <apt-pkg/versionmatch.h>
#include <vector>

using std::vector;

class pkgPolicy : public pkgDepCache::Policy
{
   struct Pin
   {
      pkgVersionMatch::MatchType Type;
      string Data;
      signed short Priority;
      Pin() : Type(pkgVersionMatch::None), Priority(0) {}
   };

   struct PkgPin : Pin
   {
      string Pkg;
   };

   protected:

   Pin *Pins;
   signed short *PFPriority;
   vector<Pin> Defaults;
   vector<PkgPin> Unmatched;
   pkgCache *Cache;
   bool StatusOverride;

   public:

   // Things for manipulating pins
   void CreatePin(pkgVersionMatch::MatchType Type,const string &Pkg,
		  const string &Data,signed short Priority);
   inline signed short GetPriority(pkgCache::PkgFileIterator const &File)
       {return PFPriority[File->ID];}
   signed short GetPriority(pkgCache::PkgIterator const &Pkg);
   pkgCache::VerIterator GetMatch(pkgCache::PkgIterator Pkg);

   // CNC:2003-03-06
   virtual signed short GetPkgPriority(const pkgCache::PkgIterator &Pkg) override;

   // Things for the cache interface.
   virtual pkgCache::VerIterator GetCandidateVer(pkgCache::PkgIterator Pkg) override;
   // CNC:2002-03-17 - Every place that uses this function seems to
   //		       currently check for IsCritical() as well. Since
   //		       this is a virtual (heavy) function, we'll try
   //		       not to use it while not necessary.
   virtual bool IsImportantDep(pkgCache::DepIterator Dep) override {return pkgDepCache::Policy::IsImportantDep(Dep);}
   bool InitDefaults();

   pkgPolicy(pkgCache *Owner);
   virtual ~pkgPolicy() {delete [] PFPriority; delete [] Pins;}
};

bool ReadPinFile(pkgPolicy &Plcy,string File = "");
bool ReadPinDir(pkgPolicy &Plcy,string Dir = "");

#endif
