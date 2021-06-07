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
#include <apt-pkg/policy.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/cmndline.h>

#include <memory>

/* This class serves as the owner of a pkgCache object with
   its common dependents (and with one dependency, namely, an MMap).
   Its idea similar to std::unique_ptr, but it is concerned
   not just with a single pointer, but also with dependent objects
   that depend on one pointer or another owned by an instance of this class.

   We code this class to make explicit the common invariants concerning the
   lifetime of this kind of interdependent objects. Having this explicit code
   may help a future maintainer better than just having the idea in one's mind,
   and guard him from potential errors (breaking the invariants) when
   implementing more complex behavior on the base of this class (e.g., how
   the objects are built).
*/
class pkgCacheWithDependents
{
   /* Hide the members, so that no one can set them directly and thereby
      accidentally break the dependents.

      The dependee pointer must be alive (and not change) for a longer time
      than any dependent. (The dependents save the pointer, and it mustn't
      become dangling.)
   */
   MMap *Map;
   pkgCache *Cache; // depends on Map
   pkgPolicy *Policy; // depends on Cache
   pkgDepCache *DCache; // depends on Cache and Policy

   protected:

   /* Perhaps, not inlining the methods would allow more freedom
      in changing the internal structure of the class without modifying the ABI.
   */

   /* POD members (the pointers) would be in indeterminate state
      if not explicitly initialized.
   */
   pkgCacheWithDependents()
      : Map(nullptr)
      , Cache(nullptr)
      , Policy(nullptr)
      , DCache(nullptr)
   {
   }

   /* Analogues of std::unique_ptr::reset(T*)
      which respect the common dependencies.
   */

   /* A helper which performs the actions in the same order
      as std::unique_ptr::reset().
   */
   private:
   template<class T>
   static inline void reset(T* & var, T* const ptr)
   {
      T* const old_ptr = var;
      var = ptr;
      delete old_ptr;
   }

   protected:

   void resetMap(MMap * const ptr)
   {
      clearCache();
      reset(Map,ptr);
   }

   void resetCache(pkgCache * const ptr)
   {
      clearDCache();
      clearPolicy();
      reset(Cache,ptr);
   }

   void resetPolicy(pkgPolicy * const ptr)
   {
      clearDCache();
      reset(Policy,ptr);
   }

   void resetDCache(pkgDepCache * const ptr)
   {
      reset(DCache,ptr);
   }

   /* Analogues of std::unique_ptr::release()
      which respect the common dependencies.

      Make clear with the return type that now the caller becomes the owner
      (i.e., is reponsible for managing the object's memory).
   */

   std::unique_ptr<MMap> relaseMap()
   {
      clearCache();
      std::unique_ptr<MMap> R(Map);
      Map = nullptr;
      return R;
   }

   std::unique_ptr<pkgCache> relaseCache()
   {
      clearDCache();
      clearPolicy();
      std::unique_ptr<pkgCache> R(Cache);
      Cache = nullptr;
      return R;
   }

   std::unique_ptr<pkgPolicy> relasePolicy()
   {
      clearDCache();
      std::unique_ptr<pkgPolicy> R(Policy);
      Policy = nullptr;
      return R;
   }

   std::unique_ptr<pkgDepCache> relaseDCache()
   {
      std::unique_ptr<pkgDepCache> R(DCache);
      DCache = nullptr;
      return R;
   }

   /* Analogues of std::unique_ptr::reset(nullptr)
      which respect the common dependencies.
   */

   void clearMap()
   {
      resetMap(nullptr);
   }

   void clearCache()
   {
      resetCache(nullptr);
   }

   void clearPolicy()
   {
      resetPolicy(nullptr);
   }

   void clearDCache()
   {
      resetDCache(nullptr);
   }

   void clear()
   {
      //clearDCache();
      //clearPolicy();
      //clearCache();
      clearMap(); // this should be enough, since the others depend on it
   }

   // virtual for just the case if someone would work
   // with a pointer to the base class
   virtual ~pkgCacheWithDependents()
   {
      clear();
   }

   /* The owner must be unique.

      Prohibit copying (just in case the compiler doesn't see that
      it's bad to implicitly define copy ctor and assignment for this class).
   */
   pkgCacheWithDependents(const pkgCacheWithDependents &) = delete;
   pkgCacheWithDependents & operator=(const pkgCacheWithDependents &) = delete;

   /* Analogues of std::unique_ptr::get(). */

   public:

   MMap * getMap() const
   {
      return Map;
   }

   pkgCache * getCache() const
   {
      return Cache;
   }

   pkgPolicy * getPolicy() const
   {
      return Policy;
   }

   pkgDepCache * getDCache() const
   {
      return DCache;
   }

};

class pkgCacheFile: public pkgCacheWithDependents
{
   protected:

   // FIXME: does anyone depend after BuildCaches()/Open() on it?
   // If so, protect it similarly to pkgCacheWithDependents.
   pkgSourceList *SrcList;

   public:

   // We look pretty much exactly like a pointer to a dep cache
   inline operator pkgCache &() const {return *getCache();}
   inline operator pkgCache *() const {return getCache();}
   inline operator pkgDepCache &() const {return *getDCache();}
   inline operator pkgDepCache *() const {return getDCache();}
   inline operator pkgSourceList &() const {return *SrcList;}
   inline operator pkgSourceList *() const {return SrcList;}
   inline pkgDepCache *operator ->() const {return getDCache();}
   inline pkgDepCache &operator *() const {return *getDCache();}
   inline pkgDepCache::StateCache &operator [](pkgCache::PkgIterator const &I) const {return (*getDCache())[I];}
   inline unsigned char &operator [](pkgCache::DepIterator const &I) const {return (*getDCache())[I];}

   bool BuildCaches(OpProgress &Progress,bool WithLock);
   bool BuildSourceList(OpProgress *Progress = NULL);
   bool Open(OpProgress &Progress,bool WithLock);
   static void RemoveCaches();
   void Close();

   inline pkgSourceList* GetSourceList() { BuildSourceList(); return SrcList; }

   inline bool IsSrcListBuilt() const { return (SrcList != NULL); }

   pkgCacheFile();
   // Since we inherit from pkgCacheWithDependents with virtual destructor,
   // now there is no need to declare it virtual. (It is so automatically.)
   virtual ~pkgCacheFile();
};

// class CacheFile - Cover class for some dependency cache functions	/*{{{*/
// ---------------------------------------------------------------------
/* */
class CacheFile: public pkgCacheFile
{
public:
   std::unique_ptr<pkgCache::Package*[]> List;

   explicit CacheFile(std::ostream &c1out);

   void Sort();

   // CacheFile::CheckDeps - Open the cache file
   // ---------------------------------------------------------------------
   /* This routine generates the caches and then opens the dependency cache
      and verifies that the system is OK. */
   bool CheckDeps(bool AllowBroken = false);
   bool BuildCaches(bool WithLock);
   bool Open(bool WithLock);
   bool OpenForInstall();

   bool CanCommit() const;

private:
   std::ostream &m_c1out;
   bool m_is_root;

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
