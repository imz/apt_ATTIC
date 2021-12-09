// Description								/*{{{*/
// $Id: algorithms.h,v 1.4 2003/01/29 13:04:48 niemeyer Exp $
/* ######################################################################

   Algorithms - A set of misc algorithms

   This simulate class displays what the ordering code has done and
   analyses it with a fresh new dependency cache. In this way we can
   see all of the effects of an upgrade run.

   pkgDistUpgrade computes an upgrade that causes as many packages as
   possible to move to the newest verison.

   pkgApplyStatus sets the target state based on the content of the status
   field in the status file. It is important to get proper crash recovery.

   pkgFixBroken corrects a broken system so that it is in a sane state.

   pkgAllUpgrade attempts to upgade as many packages as possible but
   without installing new packages.

   The problem resolver class contains a number of complex algorithms
   to try to best-guess an upgrade state. It solves the problem of
   maximizing the number of install state packages while having no broken
   packages.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ALGORITHMS_H
#define PKGLIB_ALGORITHMS_H

#include <apt-pkg/packagemanager.h>
#include <apt-pkg/depcache.h>

#include <iostream>
#include <set>
#include <string>
#include <utility>

using std::ostream;

#ifndef SWIG
class pkgSimulate : public pkgPackageManager
{
   protected:

   class Policy : public pkgDepCache::Policy
   {
      pkgDepCache *Cache;
      public:

      virtual VerIterator GetCandidateVer(PkgIterator Pkg) override
      {
	 return (*Cache)[Pkg].CandidateVerIter(*Cache);
      }

      Policy(pkgDepCache *Cache) : Cache(Cache) {}
   };

   unsigned char *Flags;

   Policy iPolicy;
   pkgDepCache Sim;

   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,const string &File) override;
   virtual bool Configure(PkgIterator Pkg) override;
   virtual bool Remove(PkgIterator Pkg,bool Purge) override;
   virtual bool Go(PackageManagerCallback_t /*callback*/, void * /*callbackData*/) override {return true;}
   virtual void Reset() override {}
   void ShortBreaks();
   void Describe(PkgIterator iPkg,ostream &out,bool Now);

   public:

   pkgSimulate(pkgDepCache *Cache);
};
#endif

class pkgProblemResolver
{
   pkgDepCache &Cache;
   typedef pkgCache::PkgIterator PkgIterator;
   typedef pkgCache::VerIterator VerIterator;
   typedef pkgCache::DepIterator DepIterator;
   typedef pkgCache::PrvIterator PrvIterator;
   typedef pkgCache::Version Version;
   typedef pkgCache::Package Package;

   enum Flags {Protected = (1 << 0), PreInstalled = (1 << 1),
               Upgradable = (1 << 2), ReInstateTried = (1 << 3),
               ToRemove = (1 << 4)};
   signed short *Scores;
   unsigned char *Flags;
   bool Debug;

   // Sort stuff
   static pkgProblemResolver *This;

   struct PackageKill
   {
      PkgIterator Pkg;
      DepIterator Dep;
   };

   bool DoUpgrade(pkgCache::PkgIterator Pkg);

   public:

   inline void Protect(const pkgCache::PkgIterator &Pkg) {Flags[Pkg->ID] |= Protected;}
   inline void Remove(const pkgCache::PkgIterator &Pkg) {Flags[Pkg->ID] |= ToRemove;}
   inline void Clear(const pkgCache::PkgIterator &Pkg) {Flags[Pkg->ID] &= ~(Protected | ToRemove);}

   // Try to intelligently resolve problems by installing and removing packages
   bool Resolve(bool BrokenFix = false);

   // Try to resolve problems only by using keep
   bool ResolveByKeep();

   void InstallProtect();

   bool RemoveDepends(); // CNC:2002-08-01

   pkgProblemResolver(pkgDepCache *Cache);
   ~pkgProblemResolver();

   // Sort stuff
   static int ScoreSort(const void *a,const void *b);
   void MakeScores();
};

bool pkgDistUpgrade(pkgDepCache &Cache);
bool pkgApplyStatus(pkgDepCache &Cache);
bool pkgFixBroken(pkgDepCache &Cache);
bool pkgAllUpgrade(pkgDepCache &Cache);
bool pkgMinimizeUpgrade(pkgDepCache &Cache);
bool pkgAutoremove(pkgDepCache &Cache);

bool pkgAutoremoveGetKeptAndUnneededPackages(pkgDepCache &Cache, std::set<std::string> *o_kept_packages, std::set<std::string> *o_unneeded_packages);

void pkgPrioSortList(pkgCache &Cache,pkgCache::Version **List);

#endif
