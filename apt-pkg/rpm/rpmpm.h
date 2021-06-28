// Description								/*{{{*/
// $Id: rpmpm.h,v 1.4 2003/01/29 13:52:32 niemeyer Exp $
/* ######################################################################

   rpm Package Manager - Provide an interface to rpm

   #####################################################################
 */
									/*}}}*/
#ifndef PKGLIB_rpmPM_H
#define PKGLIB_rpmPM_H

#include <rpm/rpmlib.h>
#include <rpm/rpmts.h>
#include <rpm/rpmcli.h>
									/*}}}*/
typedef Header rpmHeader;

#include <apt-pkg/packagemanager.h>
#include <vector>

using namespace std;

class pkgRPMPM : public pkgPackageManager
{
   protected:

   struct Item
   {
      enum Ops {Install, Configure, Remove, Purge} Op;
      enum RPMOps {RPMInstall, RPMUpgrade, RPMErase};
      string File;
      PkgIterator Pkg;
      Item(Ops Op,PkgIterator Pkg,const string &File = "")
	 : Op(Op), File(File), Pkg(Pkg) {}
      Item() {}

   };
   vector<Item> List;

   // Helpers
   bool RunScripts(const char *Cnf);
   bool RunScriptsWithPkgs(const char *Cnf);

   // The Actuall installation implementation
   virtual bool Install(PkgIterator Pkg,const string &File) override;
   virtual bool Configure(PkgIterator Pkg) override;
   virtual bool Remove(PkgIterator Pkg,bool Purge = false) override;

   virtual bool Process(const std::vector<apt_item> &install,
		const std::vector<apt_item> &upgrade,
		const std::vector<apt_item> &uninstall,
		PackageManagerCallback_t callback, void *callbackData) {return false;}

   virtual bool Go(PackageManagerCallback_t callback, void *callbackData) override;
   virtual void Reset() override;

   public:

   pkgRPMPM(pkgDepCache *Cache);
   virtual ~pkgRPMPM();
};

class pkgRPMExtPM : public pkgRPMPM
{
   protected:
   bool ExecRPM(Item::RPMOps op, const std::vector<apt_item> &files);
   virtual bool Process(const std::vector<apt_item> &install,
		const std::vector<apt_item> &upgrade,
		const std::vector<apt_item> &uninstall,
		PackageManagerCallback_t callback, void *callbackData) override;

   public:
   pkgRPMExtPM(pkgDepCache *Cache);
   virtual ~pkgRPMExtPM();
};

class pkgRPMLibPM : public pkgRPMPM
{
   protected:
   rpmts TS;

   bool ParseRpmOpts(const char *Cnf, int *tsFlags, int *probFilter);
   bool AddToTransaction(Item::RPMOps op, const std::vector<apt_item> &files);
   static void * customCallback(const void * h, const rpmCallbackType what,
				const uint64_t amount, const uint64_t total,
				const void * pkgKey, void * data);
   virtual bool Process(const std::vector<apt_item> &install,
		const std::vector<apt_item> &upgrade,
		const std::vector<apt_item> &uninstall,
		PackageManagerCallback_t callback, void *callbackData) override;

   public:

   virtual bool UpdateMarks() override;

   pkgRPMLibPM(pkgDepCache *Cache);
   virtual ~pkgRPMLibPM();
};

#endif

// vim:sts=3:sw=3
