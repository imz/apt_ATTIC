// Description								/*{{{*/
// $Id: cachefile.h,v 1.2 2002/07/25 18:07:18 niemeyer Exp $
/* ######################################################################

   CacheFile - Simple wrapper class for opening, generating and whatnot

   This class implements a simple 2 line mechanism to open various sorts
   of caches. It can operate as root, as not root, show progress and so on,
   it transparently handles everything necessary.

   This means it can rebuild caches from the source list and instantiates
   and prepares the standard policy mechanism.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_CACHEFILE_H
#define PKGLIB_CACHEFILE_H

#include <apt-pkg/depcache.h>
#include <apt-pkg/cmndline.h>

#include <memory>

class pkgPolicy;
class pkgSourceList;

/* An abstract class that owns pkgCache & its dependents (and some requisites).
   It formally imposes the property (on any derived class) that once an owned
   resource is "built" it doesn't ever change.

   Internally, it's lazy: the owned resources are built on demand.

   From outside, it looks like an immutable (lazy) structure.
 */

class lazyCacheFile
{
   private:

   bool Lock;
   pkgSourceList *SrcList;
   MMap *Map;
   pkgCache *Cache;
   pkgPolicy *Policy;
   pkgDepCache *DCache;

   public:

   // These methods demand the resources; so, if needed, they are built.
   // They are not safe; dangerous to use if you don't check for
   // runtime errors/NULL return values. Regrettably...
   inline bool GetLock() { BuildLock(); return Lock; }
   inline MMap* GetMap(OpProgress &Progress) { BuildMap(Progress); return Map; }
   inline pkgCache* GetPkgCache(OpProgress &Progress) { BuildCaches(Progress); return Cache; }
   inline pkgDepCache* GetDepCache(OpProgress &Progress) { BuildDepCache(Progress); return DCache; }
   inline pkgPolicy* GetPolicy(OpProgress &Progress) { BuildPolicy(Progress); return Policy; }
   inline pkgSourceList* GetSourceList() { BuildSourceList(); return SrcList; }

   // These methods build the members and never recreate anything that exists.
   bool BuildLock();
   bool BuildMap(OpProgress &Progress);
   bool BuildCaches(OpProgress &Progress);
   bool BuildSourceList(OpProgress *Progress = NULL);
   bool BuildPolicy(OpProgress &Progress);
   bool BuildDepCache(OpProgress &Progress);

   inline bool IsLockBuilt() const { return Lock; }
   inline bool IsMapBuilt() const { return (Map != NULL); }
   inline bool IsPkgCacheBuilt() const { return (Cache != NULL); }
   inline bool IsDepCacheBuilt() const { return (DCache != NULL); }
   inline bool IsPolicyBuilt() const { return (Policy != NULL); }
   inline bool IsSrcListBuilt() const { return (SrcList != NULL); }

   lazyCacheFile();
   virtual ~lazyCacheFile();
   /* The owner must be unique.

      Prohibit copying (just in case the compiler doesn't see that
      it's bad to implicitly define copy ctor and assignment for this class).
   */
   lazyCacheFile(const lazyCacheFile &) = delete;
   lazyCacheFile & operator=(const lazyCacheFile &) = delete;

   // These methods are implemented by subclasses. They can't break our rule.
   virtual bool MakeLock() = 0;
   virtual std::unique_ptr<MMap> MakeMap(OpProgress &Progress) = 0;
   virtual std::unique_ptr<pkgCache> MakePkgCache(OpProgress &Progress) = 0;
   virtual std::unique_ptr<pkgDepCache> MakeDepCache(OpProgress &Progress) = 0;
   virtual std::unique_ptr<pkgPolicy> MakePolicy(OpProgress &Progress) = 0;
   virtual std::unique_ptr<pkgSourceList> MakeSrcList(OpProgress *Progress) = 0;

  /* Analogues of std::unique_ptr::get().

     Some functions in subclasses access the members directly (without
     supplying a Progress object); so these methods help them.

     These methods are not safe; dangerous to use if you are not sure that
     the accessed members have already been built successfully.
   */

   protected:

   //MMap * getMap() const { return Map; }
   pkgCache * getCache() const { return Cache; }
   //pkgPolicy * getPolicy() const { return Policy; }
   pkgDepCache * getDCache() const { return DCache; }
   pkgSourceList* getSrcList() const { return SrcList; }

};

class pkgCacheFile: public lazyCacheFile
{
   protected:

   const bool WithLock;

   public:

   // We look pretty much exactly like a pointer to a dep cache
   inline operator pkgCache &() const {return *getCache();}
   inline operator pkgCache *() const {return getCache();}
   inline operator pkgDepCache &() const {return *getDCache();}
   inline operator pkgDepCache *() const {return getDCache();}
   inline operator pkgSourceList &() const {return *getSrcList();}
   inline operator pkgSourceList *() const {return getSrcList();}
   inline pkgDepCache *operator ->() const {return getDCache();}
   inline pkgDepCache &operator *() const {return *getDCache();}
   inline pkgDepCache::StateCache &operator [](pkgCache::PkgIterator const &I) const {return (*getDCache())[I];}
   inline unsigned char &operator [](pkgCache::DepIterator const &I) const {return (*getDCache())[I];}

   bool MakeLock() override;
   std::unique_ptr<MMap> MakeMap(OpProgress &Progress) override;
   std::unique_ptr<pkgCache> MakePkgCache(OpProgress &Progress) override;
   std::unique_ptr<pkgDepCache> MakeDepCache(OpProgress &Progress) override;
   std::unique_ptr<pkgPolicy> MakePolicy(OpProgress &Progress) override;
   std::unique_ptr<pkgSourceList> MakeSrcList(OpProgress *Progress) override;

   bool Open(OpProgress &Progress);
   static void RemoveCaches();

   explicit pkgCacheFile(bool WithLock);
};

// Helper for apt-get and apt-mark
bool ShouldLockForInstall();

// class CacheFile - Cover class for some dependency cache functions	/*{{{*/
// ---------------------------------------------------------------------
/* */
class CacheFile: public pkgCacheFile
{
public:
   std::unique_ptr<pkgCache::Package*[]> List;

   CacheFile(std::ostream &c1out, bool WithLock);

   void Sort();

   // CacheFile::CheckDeps - Open the cache file
   // ---------------------------------------------------------------------
   /* This routine generates the caches and then opens the dependency cache
      and verifies that the system is OK. */
   bool CheckDeps(bool AllowBroken = false);
   bool BuildCaches();
   bool Open();

   bool CanCommit() const;

private:
   std::ostream &m_c1out;

   static pkgCache *SortCache;

   // CacheFile::NameComp - QSort compare by name
   static int NameComp(const void *a, const void *b);
};
									/*}}}*/

// ShowList - Show a list
// ---------------------------------------------------------------------
/* This prints out a string of space separated words with a title and
a two space indent line wraped to the current screen width. */
bool ShowList(std::ostream &out, const std::string &Title, std::string List, const std::string &VersionsList, size_t l_ScreenWidth);

// ShowBroken - Debugging aide
// ---------------------------------------------------------------------
/* This prints out the names of all the packages that are broken along
with the name of each each broken dependency and a quite version
description.

The output looks like:
The following packages have unmet dependencies:
exim: Depends: libc6 (>= 2.1.94) but 2.1.3-10 is to be installed
Depends: libldap2 (>= 2.0.2-2) but it is not going to be installed
Depends: libsasl7 but it is not going to be installed
*/
void ShowBroken(std::ostream &out, CacheFile &Cache, bool Now);

// ShowNew - Show packages to newly install
void ShowNew(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// ShowDel - Show packages to delete
void ShowDel(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// ShowKept - Show kept packages
void ShowKept(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// ShowUpgraded - Show upgraded packages
void ShowUpgraded(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// ShowDowngraded - Show downgraded packages
bool ShowDowngraded(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// ShowHold - Show held but changed packages
bool ShowHold(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// ShowEssential - Show an essential package warning
// ---------------------------------------------------------------------
/* This prints out a warning message that is not to be ignored. It shows
all essential packages and their dependents that are to be removed.
It is insanely risky to remove the dependents of an essential package! */
bool ShowEssential(std::ostream &out, std::ostream &l_c3out, CacheFile &Cache, pkgDepCache::State *State, size_t l_ScreenWidth);

// Stats - Show some statistics
void Stats(std::ostream &out, std::ostream &l_c3out, pkgDepCache &Dep, pkgDepCache::State *State);

bool pkgDoAuto(std::ostream &c1out, const CommandLine &CmdL, int &auto_mark_changed, pkgDepCache &Dep);
bool pkgDoShowAuto(std::ostream &cout, const CommandLine &CmdL, pkgDepCache &Dep);

#endif
