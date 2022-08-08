// Description								/*{{{*/
// $Id: algorithms.cc,v 1.11 2003/01/29 18:43:48 niemeyer Exp $
/* ######################################################################

   Algorithms - A set of misc algorithms

   The pkgProblemResolver class has become insanely complex and
   very sophisticated, it handles every test case I have thrown at it
   to my satisfaction. Understanding exactly why all the steps the class
   does are required is difficult and changing though not very risky
   may result in other cases not working.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/algorithms.h>
#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/sptr.h>

// CNC:2002-07-04
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/version.h>

#include <apti18n.h>

#include <iostream>
#include <map>
#include <set>
									/*}}}*/
using namespace std;

pkgProblemResolver *pkgProblemResolver::This = 0;

// Simulate::Simulate - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* The legacy translations here of input Pkg iterators is obsolete,
   this is not necessary since the pkgCaches are fully shared now. */
pkgSimulate::pkgSimulate(pkgDepCache *Cache) : pkgPackageManager(Cache),
		            iPolicy(Cache),
                            Sim(&Cache->GetCache(),&iPolicy)
{
   Sim.Init(0);
   // allocate and zero memory
   Flags = new unsigned char[Cache->Head().PackageCount]();

   // Fake a filename so as not to activate the media swapping
   string Jnk = "SIMULATE";
   for (unsigned int I = 0; I != Cache->Head().PackageCount; I++)
      FileNames[I] = Jnk;
}
									/*}}}*/
// Simulate::Describe - Describe a package				/*{{{*/
// ---------------------------------------------------------------------
/* Parameter Now == true gives both current and available varsion,
   Parameter Now == false gives only the available package version */
void pkgSimulate::Describe(PkgIterator Pkg,ostream &out,bool Now)
{
   VerIterator Ver(Sim);

   out << Pkg.Name();

   if (Now == true)
   {
      Ver = Pkg.CurrentVer();
      if (Ver.end() == false)
         out << " [" << Ver.VerStr() << ']';
   }

   Ver = Sim[Pkg].CandidateVerIter(Sim);
   if (Ver.end() == true)
      return;

   out << " (" << Ver.VerStr() << ' ' << Ver.RelStr() << ')';
}
									/*}}}*/
// Simulate::Install - Simulate unpacking of a package			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSimulate::Install(PkgIterator iPkg,const string &/*File*/)
{
   // Adapt the iterator
   PkgIterator Pkg = Sim.FindPkg(iPkg.Name());
   Flags[Pkg->ID] = 1;

   cout << "Inst ";
   Describe(Pkg,cout,true);
   Sim.MarkInstall(Pkg,pkgDepCache::AutoMarkFlag::DontChange,false);

   // Look for broken conflicts+predepends.
   for (PkgIterator I = Sim.PkgBegin(); I.end() == false; I++)
   {
      if (Sim[I].InstallVer == 0)
	 continue;

      for (DepIterator D = Sim[I].InstVerIter(Sim).DependsList(); D.end() == false;)
      {
	 DepIterator Start;
	 DepIterator End;
	 D.GlobOr(Start,End);
	 if (Start->Type == pkgCache::Dep::Conflicts ||
	     Start->Type == pkgCache::Dep::Obsoletes ||
	     End->Type == pkgCache::Dep::PreDepends)
         {
	    if ((Sim[End] & pkgDepCache::DepGInstall) == 0)
	    {
	       cout << " [" << I.Name() << " on " << Start.TargetPkg().Name() << ']';
	       if (Start->Type == pkgCache::Dep::Conflicts)
		  _error->Error("Fatal, conflicts violated %s",I.Name());
	    }
	 }
      }
   }

   if (Sim.BrokenCount() != 0)
      ShortBreaks();
   else
      cout << endl;
   return true;
}
									/*}}}*/
// Simulate::Configure - Simulate configuration of a Package		/*{{{*/
// ---------------------------------------------------------------------
/* This is not an acurate simulation of relatity, we should really not
   install the package.. For some investigations it may be necessary
   however. */
bool pkgSimulate::Configure(PkgIterator iPkg)
{
   // Adapt the iterator
   PkgIterator Pkg = Sim.FindPkg(iPkg.Name());

   Flags[Pkg->ID] = 2;
//   Sim.MarkInstall(Pkg,false);
   if (Sim[Pkg].InstBroken() == true)
   {
      cout << "Conf " << Pkg.Name() << " broken" << endl;

      Sim.Update();

      // Print out each package and the failed dependencies
      for (pkgCache::DepIterator D = Sim[Pkg].InstVerIter(Sim).DependsList(); D.end() == false; D++)
      {
         // CNC:2003-02-17 - IsImportantDep() currently calls IsCritical(), so
         //		     these two are currently doing the same thing. Check
         //		     comments in IsImportantDep() definition.
#if 0
	 if (Sim.IsImportantDep(D) == false ||
	     (Sim[D] & pkgDepCache::DepInstall) != 0)
	    continue;
#else
	 if (D.IsCritical() == false ||
	     (Sim[D] & pkgDepCache::DepInstall) != 0)
	    continue;
#endif

	 if (D->Type == pkgCache::Dep::Obsoletes)
	    cout << " Obsoletes:" << D.TargetPkg().Name();
	 else if (D->Type == pkgCache::Dep::Conflicts)
	    cout << " Conflicts:" << D.TargetPkg().Name();
	 else
	    cout << " Depends:" << D.TargetPkg().Name();
      }
      cout << endl;

      _error->Error("Conf Broken %s",Pkg.Name());
   }
   else
   {
      cout << "Conf ";
      Describe(Pkg,cout,false);
   }

   if (Sim.BrokenCount() != 0)
      ShortBreaks();
   else
      cout << endl;

   return true;
}
									/*}}}*/
// Simulate::Remove - Simulate the removal of a package			/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgSimulate::Remove(PkgIterator iPkg,bool Purge)
{
   // Adapt the iterator
   PkgIterator Pkg = Sim.FindPkg(iPkg.Name());

   Flags[Pkg->ID] = 3;
   Sim.MarkDelete(Pkg);
   if (Purge == true)
      cout << "Purg ";
   else
      cout << "Remv ";
   Describe(Pkg,cout,false);

   if (Sim.BrokenCount() != 0)
      ShortBreaks();
   else
      cout << endl;

   return true;
}
									/*}}}*/
// Simulate::ShortBreaks - Print out a short line describing all breaks	/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgSimulate::ShortBreaks()
{
   cout << " [";
   for (PkgIterator I = Sim.PkgBegin(); I.end() == false; I++)
   {
      if (Sim[I].InstBroken() == true)
      {
	 if (Flags[I->ID] == 0)
	    cout << I.Name() << ' ';
/*	 else
	    cout << I.Name() << "! ";*/
      }
   }
   cout << ']' << endl;
}
									/*}}}*/
// ApplyStatus - Adjust for non-ok packages				/*{{{*/
// ---------------------------------------------------------------------
/* We attempt to change the state of the all packages that have failed
   installation toward their real state. The ordering code will perform
   the necessary calculations to deal with the problems. */
bool pkgApplyStatus(pkgDepCache &Cache)
{
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if (I->VersionList == 0)
	 continue;

      // Only choice for a ReInstReq package is to reinstall
      if (I->InstState == pkgCache::State::ReInstReq ||
	  I->InstState == pkgCache::State::HoldReInstReq)
      {
	 if (I->CurrentVer != 0 && I.CurrentVer().Downloadable() == true)
	    Cache.MarkKeep(I);
	 else
	 {
	    // Is this right? Will dpkg choke on an upgrade?
	    if (Cache[I].CandidateVer != 0 &&
		 Cache[I].CandidateVerIter(Cache).Downloadable() == true)
	       Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange);
	    else
	       return _error->Error(_("The package %s needs to be reinstalled, "
				    "but I can't find an archive for it."),I.Name());
	 }

	 continue;
      }

      switch (I->CurrentState)
      {
	 /* This means installation failed somehow - it does not need to be
	    re-unpacked (probably) */
	 case pkgCache::State::UnPacked:
	 case pkgCache::State::HalfConfigured:
	 if ((I->CurrentVer != 0 && I.CurrentVer().Downloadable() == true) ||
	     I.State() != pkgCache::PkgIterator::NeedsUnpack)
	    Cache.MarkKeep(I);
	 else
	 {
	    if (Cache[I].CandidateVer != 0 &&
		 Cache[I].CandidateVerIter(Cache).Downloadable() == true)
	       Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange);
	    else
	       Cache.MarkDelete(I);
	 }
	 break;

	 // This means removal failed
	 case pkgCache::State::HalfInstalled:
	 Cache.MarkDelete(I);
	 break;

	 default:
	 if (I->InstState != pkgCache::State::Ok)
	    return _error->Error("The package %s is not ok and I "
				 "don't know how to fix it!",I.Name());
      }
   }
   return true;
}
									/*}}}*/
// FixBroken - Fix broken packages					/*{{{*/
// ---------------------------------------------------------------------
/* This autoinstalls every broken package and then runs the problem resolver
   on the result. */
bool pkgFixBroken(pkgDepCache &Cache)
{
   // Auto upgrade all broken packages
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      if (Cache[I].NowBroken() == true)
	 Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,true);

   /* Fix packages that are in a NeedArchive state but don't have a
      downloadable install version */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if (I.State() != pkgCache::PkgIterator::NeedsUnpack ||
	  Cache[I].Delete() == true)
	 continue;

      if (Cache[I].InstVerIter(Cache).Downloadable() == false)
	 continue;

      Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,true);
   }

   pkgProblemResolver Fix(&Cache);

   // CNC:2002-07-04
   _system->ProcessCache(Cache,Fix);

   // CNC:2002-08-08
   if (_config->FindB("APT::Remove-Depends",false) == true)
      Fix.RemoveDepends();

   return Fix.Resolve(true);
}

static pkgDepCache::AutoMarkFlag replacingPackageMarkedAuto(
   pkgDepCache &Cache,
   const pkgCache::PkgIterator &obsoleted_package,
   const pkgCache::PkgIterator &replacing_package)
{
   std::string apt_inheritance_value = _config->Find("APT::Get::Obsoletes::AptMarkInheritanceAuto", "all");

   pkgDepCache::AutoMarkFlag obsoleted_package_auto = Cache.getMarkAuto(obsoleted_package);
   pkgDepCache::AutoMarkFlag replacing_package_auto = Cache.getMarkAuto(replacing_package);

   if (apt_inheritance_value.compare("obsoleted") == 0)
   {
      return obsoleted_package_auto;
   }
   else if (apt_inheritance_value.compare("replacing") == 0)
   {
      return replacing_package_auto;
   }
   else if (apt_inheritance_value.compare("at_least_one") == 0)
   {
      return ((obsoleted_package_auto == pkgDepCache::AutoMarkFlag::Auto)
         || (replacing_package_auto == pkgDepCache::AutoMarkFlag::Auto))
         ? pkgDepCache::AutoMarkFlag::Auto : pkgDepCache::AutoMarkFlag::Manual;
   }
   else if (apt_inheritance_value.compare("never") == 0)
   {
      return pkgDepCache::AutoMarkFlag::Manual;
   }
   else if (apt_inheritance_value.compare("always") == 0)
   {
      return pkgDepCache::AutoMarkFlag::Auto;
   }
   else /* if (apt_inheritance_value.compare("all") == 0) */
   {
      return ((obsoleted_package_auto == pkgDepCache::AutoMarkFlag::Auto)
         && (replacing_package_auto == pkgDepCache::AutoMarkFlag::Auto))
         ? pkgDepCache::AutoMarkFlag::Auto : pkgDepCache::AutoMarkFlag::Manual;
   }
}
									/*}}}*/
// DistUpgrade - Distribution upgrade					/*{{{*/
// ---------------------------------------------------------------------
/* This autoinstalls every package and then force installs every
   pre-existing package. This creates the initial set of conditions which
   most likely contain problems because too many things were installed.

   The problem resolver is used to resolve the problems.
 */
bool pkgDistUpgrade(pkgDepCache &Cache)
{
   /* Upgrade all installed packages first without autoinst to help the resolver
      in versioned or-groups to upgrade the old solver instead of installing
      a new one (if the old solver is not the first one [anymore]) */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; ++I)
      // ALT:2014-06-25
      if (I->CurrentVer != 0)
      {
	 // Was it obsoleted?
	 bool Obsoleted = false;
	 for (pkgCache::DepIterator D = I.RevDependsList(); D.end() == false; D++)
	 {
	    if (D->Type == pkgCache::Dep::Obsoletes &&
	        Cache[D.ParentPkg()].CandidateVer != 0 &&
		Cache[D.ParentPkg()].CandidateVerIter(Cache).Downloadable() == true &&
	        (pkgCache::Version*)D.ParentVer() == Cache[D.ParentPkg()].CandidateVer &&
	        Cache.VS().CheckDep(I.CurrentVer().VerStr(), D) == true &&
		Cache.GetPkgPriority(D.ParentPkg()) >= Cache.GetPkgPriority(I))
	    {
	       Obsoleted = true;
	       break;
	    }
	 }
	 if (Obsoleted == false)
	    Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
      }

   /* Auto upgrade all installed packages, this provides the basis
      for the installation */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      // CNC:2002-07-23
      if (I->CurrentVer != 0)
      {
	 // Was it obsoleted?
	 bool Obsoleted = false;
	 for (pkgCache::DepIterator D = I.RevDependsList(); D.end() == false; D++)
	 {
	    if (D->Type == pkgCache::Dep::Obsoletes &&
	        Cache[D.ParentPkg()].CandidateVer != 0 &&
		Cache[D.ParentPkg()].CandidateVerIter(Cache).Downloadable() == true &&
	        (pkgCache::Version*)D.ParentVer() == Cache[D.ParentPkg()].CandidateVer &&
	        Cache.VS().CheckDep(I.CurrentVer().VerStr(), D) == true &&
		Cache.GetPkgPriority(D.ParentPkg()) >= Cache.GetPkgPriority(I))
	    {
	       pkgDepCache::AutoMarkFlag newMarkAuto = replacingPackageMarkedAuto(Cache, I, D.ParentPkg());
	       Cache.MarkInstall(D.ParentPkg(), newMarkAuto, true);
	       Obsoleted = true;
	       break;
	    }
	 }
	 if (Obsoleted == false)
	    Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,true);
      }
   }

   /* Now, auto upgrade all essential packages - this ensures that
      the essential packages are present and working */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      if ((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	 Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,true);

   /* We do it again over all previously installed packages to force
      conflict resolution on them all. */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      // CNC:2002-07-23
      if (I->CurrentVer != 0)
      {
	 // Was it obsoleted?
	 bool Obsoleted = false;
	 for (pkgCache::DepIterator D = I.RevDependsList(); D.end() == false; D++)
	 {
	    if (D->Type == pkgCache::Dep::Obsoletes &&
	        Cache[D.ParentPkg()].CandidateVer != 0 &&
		Cache[D.ParentPkg()].CandidateVerIter(Cache).Downloadable() == true &&
	        (pkgCache::Version*)D.ParentVer() == Cache[D.ParentPkg()].CandidateVer &&
	        Cache.VS().CheckDep(I.CurrentVer().VerStr(), D) == true &&
		Cache.GetPkgPriority(D.ParentPkg()) >= Cache.GetPkgPriority(I))
	    {
	       pkgDepCache::AutoMarkFlag newMarkAuto = replacingPackageMarkedAuto(Cache, I, D.ParentPkg());
	       Cache.MarkInstall(D.ParentPkg(),newMarkAuto,false);
	       Obsoleted = true;
	       break;
	    }
	 }
	 if (Obsoleted == false)
	    Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
      }
   }

   pkgProblemResolver Fix(&Cache);

   // CNC:2002-07-04
   _system->ProcessCache(Cache,Fix);

   // Hold back held packages.
   if (_config->FindB("APT::Ignore-Hold",false) == false)
   {
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      {
	 if (I->SelectedState == pkgCache::State::Hold)
	 {
	    Fix.Protect(I);
	    Cache.MarkKeep(I);
	 }
      }
   }

   // CNC:2002-08-08
   if (_config->FindB("APT::Remove-Depends",false) == true)
      Fix.RemoveDepends();

   // CNC:2003-03-22
   return Fix.Resolve(true);
}
									/*}}}*/
// AllUpgrade - Upgrade as many packages as possible			/*{{{*/
// ---------------------------------------------------------------------
/* Right now the system must be consistent before this can be called.
   It also will not change packages marked for install, it only tries
   to install packages not marked for install */
bool pkgAllUpgrade(pkgDepCache &Cache)
{
   pkgProblemResolver Fix(&Cache);

   if (Cache.BrokenCount() != 0)
      return false;

   // Upgrade all installed packages
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if (Cache[I].Install() == true)
	 Fix.Protect(I);

      if (_config->FindB("APT::Ignore-Hold",false) == false)
	 if (I->SelectedState == pkgCache::State::Hold)
	    continue;

      if (I->CurrentVer != 0 && Cache[I].InstallVer != 0)
	 Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
   }

   // CNC:2002-07-04
   _system->ProcessCache(Cache,Fix);

   return Fix.ResolveByKeep();
}
									/*}}}*/
// MinimizeUpgrade - Minimizes the set of packages to be upgraded	/*{{{*/
// ---------------------------------------------------------------------
/* This simply goes over the entire set of packages and tries to keep
   each package marked for upgrade. If a conflict is generated then
   the package is restored. */
bool pkgMinimizeUpgrade(pkgDepCache &Cache)
{
   if (Cache.BrokenCount() != 0)
      return false;

   // We loop for 10 tries to get the minimal set size.
   bool Change = false;
   unsigned int Count = 0;
   do
   {
      Change = false;
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      {
	 // Not interesting
	 if (Cache[I].Upgrade() == false || Cache[I].NewInstall() == true)
	    continue;

	 // Keep it and see if that is OK
	 Cache.MarkKeep(I);
	 if (Cache.BrokenCount() != 0)
	    Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
	 else
	 {
	    // If keep didnt actually do anything then there was no change..
	    if (Cache[I].Upgrade() == false)
	       Change = true;
	 }
      }
      Count++;
   }
   while (Change == true && Count < 10);

   if (Cache.BrokenCount() != 0)
      return _error->Error("Internal Error in pkgMinimizeUpgrade");

   return true;
}
									/*}}}*/

static bool find_all_required_dependencies(
   pkgDepCache &Cache,
   const pkgCache::PkgIterator &pkg_iter,
   std::set<const char*> &kept_packages,
   std::map<const char*, std::set<pkgCache::PkgIterator> > &unresolved_virtual_dependencies,
   const std::map<const char*, std::set<pkgCache::PrvIterator> > &virtual_provides_map)
{
   // don't try doing infinite loop on cyclic dependencies
   if (kept_packages.find(pkg_iter.Name()) != kept_packages.end())
   {
      return true;
   }

   kept_packages.insert(pkg_iter.Name());

   auto version_iter = pkg_iter.CurrentVer();

   for (auto deps_iter = version_iter.DependsList(); not deps_iter.end(); ++deps_iter)
   {
      // only process specific dependencies types
      switch (deps_iter->Type)
      {
      case pkgCache::Dep::Depends:
      case pkgCache::Dep::PreDepends:
         break;

      default:
         continue;
      }

      auto dependency_pkg = Cache.FindPkg(deps_iter.TargetPkg().Name());
      if (dependency_pkg.end())
      {
         return false;
      }

      // Package should not only be installed, but a version should match too if it's a dependency on specific version
      if ((dependency_pkg->CurrentState == pkgCache::State::Installed)
         && (not dependency_pkg.CurrentVer().end())
         && (Cache.VS().CheckDep(dependency_pkg.CurrentVer().VerStr(), deps_iter)))
      {
         if (!find_all_required_dependencies(Cache, dependency_pkg, kept_packages, unresolved_virtual_dependencies, virtual_provides_map))
         {
            return false;
         }
      }
      else
      {
         // probably a virtual package, check it
         auto virtual_provides_map_iter = virtual_provides_map.find(dependency_pkg.Name());
         if (virtual_provides_map_iter == virtual_provides_map.end())
         {
            return false;
         }

         // find all satisfying provides
         std::set<pkgCache::PkgIterator> matching_provides;

         for (auto provides_iter = virtual_provides_map_iter->second.begin(); provides_iter != virtual_provides_map_iter->second.end(); ++provides_iter)
         {
            if (Cache.VS().CheckDep(provides_iter->ProvideVersion(), deps_iter))
            {
               matching_provides.insert(provides_iter->OwnerPkg());
            }
         }

         switch (matching_provides.size())
         {
         case 0:
            return false;
            break;

         case 1:
            if (!find_all_required_dependencies(Cache, *(matching_provides.begin()), kept_packages, unresolved_virtual_dependencies, virtual_provides_map))
            {
               return false;
            }
            break;

         default:
            unresolved_virtual_dependencies[dependency_pkg.Name()].insert(matching_provides.begin(), matching_provides.end());
            break;
         }
      }
   }

   return true;
}

static bool find_all_kept_packages(pkgDepCache &Cache, std::set<const char*> &kept_packages)
{
   bool debug_virtuals = _config->FindB("Debug::pkgAutoremove::resolveVirtuals", false);

   // save unresolved virtual dependencies here to try resolving it
   std::map<const char*, std::set<pkgCache::PkgIterator> > unresolved_virtual_dependencies;

   std::map<const char*, std::set<pkgCache::PrvIterator> > virtual_provides_map;

   kept_packages.clear();

   // First gather all virtual provides
   for (pkgCache::PkgIterator pkg_iter = Cache.PkgBegin(); not pkg_iter.end(); ++pkg_iter)
   {
      for (auto provs_iter = pkg_iter.ProvidesList(); not provs_iter.end(); ++provs_iter)
      {
         if (provs_iter.OwnerPkg()->CurrentState == pkgCache::State::Installed)
         {
            // format: virtual dependency = providing package
            virtual_provides_map[pkg_iter.Name()].insert(provs_iter);
         }
      }
   }

   // Check every installed package
   for (pkgCache::PkgIterator pkg_iter = Cache.PkgBegin(); not pkg_iter.end(); ++pkg_iter)
   {
      // Skip packages not installed, and automatically installed ones too, and packages with pending removal
      if ((pkg_iter->CurrentState == pkgCache::State::Installed)
         && (Cache[pkg_iter].Mode != pkgDepCache::ModeList::ModeDelete)
         && (Cache.getMarkAuto(pkg_iter) == pkgDepCache::AutoMarkFlag::Manual))
      {
         if (!find_all_required_dependencies(Cache, pkg_iter, kept_packages, unresolved_virtual_dependencies, virtual_provides_map))
         {
            return _error->Error(_("Dependencies check failed"));
         }
      }
   }

   while (!unresolved_virtual_dependencies.empty())
   {
      std::map<const char*, std::set<pkgCache::PkgIterator> > new_unresolved_virtual_dependencies;

      // process every unresolved virtual dependency
      for (auto virtual_iter = unresolved_virtual_dependencies.begin();
         virtual_iter != unresolved_virtual_dependencies.end();
         ++virtual_iter)
      {
         if (debug_virtuals)
         {
            std::cerr << "Trying to resolve virtual dependency: " << virtual_iter->first << std::endl;

            for (auto provide_iter = virtual_iter->second.begin();
               provide_iter != virtual_iter->second.end();
               ++provide_iter)
            {
               std::cerr << "Candidate for " << virtual_iter->first << ": " << provide_iter->Name() << std::endl;
            }
         }

         // first check if any of providing packages already installed
         {
            auto provide_iter = virtual_iter->second.begin();

            for ( ;
               provide_iter != virtual_iter->second.end();
               ++provide_iter)
            {
               if (kept_packages.find(provide_iter->Name()) != kept_packages.end())
               {
                  break;
               }
            }

            if (provide_iter != virtual_iter->second.end())
            {
               // one or more dependencies are already installed, skip it
               if (debug_virtuals)
               {
                  std::cerr << "Package " << provide_iter->Name() << " is already required, use it to satisfy virtual dependency " << virtual_iter->first << std::endl;
               }
               continue;
            }
         }

         // now remove dependencies which have unsatisfied dependencies themselves
         {
            auto provide_iter = virtual_iter->second.begin();

            while (provide_iter != virtual_iter->second.end())
            {
               std::set<const char*> kept_packages_copy = kept_packages;
               std::map<const char*, std::set<pkgCache::PkgIterator> > temp_unresolved_virtual_dependencies;

               if (!find_all_required_dependencies(Cache, *provide_iter, kept_packages_copy, temp_unresolved_virtual_dependencies, virtual_provides_map))
               {
                  if (debug_virtuals)
                  {
                     std::cerr << "Package " << provide_iter->Name() << " has unmet dependencies, don't try using it" << std::endl;
                  }

                  provide_iter = virtual_iter->second.erase(provide_iter);
               }
               else
               {
                  ++provide_iter;
               }
            }
         }

         // if no package may be kept due to unsolved dependencies, fail
         if (virtual_iter->second.empty())
         {
            return false;
         }

         // Finally, choose one of remaining dependencies.
         // How about package with highest version?
         // If everything else fails, let's just pick first one (alphabetically)
         // TODO: consider implementing smarter choosing algorithm, some options, or even ask user to choose one
         {
            auto provide_iter = virtual_iter->second.begin();

            auto max_version = provide_iter->CurrentVer();
            size_t max_version_count = 1;
            ++provide_iter;

            while (provide_iter != virtual_iter->second.end())
            {
               auto compare_version_result = max_version.CompareVer(provide_iter->CurrentVer());

               if (compare_version_result < 0)
               {
                  max_version = provide_iter->CurrentVer();
                  max_version_count = 1;
               }
               else if (compare_version_result == 0)
               {
                  ++max_version_count;
               }

               ++provide_iter;
            }

            if (max_version_count == 1)
            {
               provide_iter = virtual_iter->second.begin();

               for ( ;
                  provide_iter != virtual_iter->second.end();
                  ++provide_iter)
               {
                  if (provide_iter->CurrentVer().CompareVer(max_version) == 0)
                  {
                     break;
                  }
               }

               if (provide_iter != virtual_iter->second.end())
               {
                  if (debug_virtuals)
                  {
                     std::cerr << "Package " << provide_iter->Name() << " is chosen to satisfy virtual dependency " << virtual_iter->first << std::endl;
                  }

                  if (!find_all_required_dependencies(Cache, *provide_iter, kept_packages, new_unresolved_virtual_dependencies, virtual_provides_map))
                  {
                     // For some reason failed when it should not
                     return false;
                  }

                  continue;
               }
            }
         }

         if (debug_virtuals)
         {
            std::cerr << "Package " << virtual_iter->second.begin()->Name() << " is chosen to satisfy virtual dependency " << virtual_iter->first << std::endl;
         }

         if (!find_all_required_dependencies(Cache, *(virtual_iter->second.begin()), kept_packages, new_unresolved_virtual_dependencies, virtual_provides_map))
         {
            // For some reason failed when it should not
            return false;
         }
      }

      unresolved_virtual_dependencies = new_unresolved_virtual_dependencies;
   }

   return true;
}

bool pkgAutoremove(pkgDepCache &Cache)
{
   if (Cache.BrokenCount() != 0)
   {
      return false;
   }

   pkgProblemResolver Fix(&Cache);

   // if package is still needed, put it into this set
   std::set<const char*> kept_packages;

   if (!find_all_kept_packages(Cache, kept_packages))
   {
      return false;
   }

   // Finally, go through all packages once more and mark them
   for (pkgCache::PkgIterator pkg_iter = Cache.PkgBegin(); not pkg_iter.end(); ++pkg_iter)
   {
      // Skip packages not installed
      if (pkg_iter->CurrentState == pkgCache::State::Installed)
      {
         if (kept_packages.find(pkg_iter.Name()) != kept_packages.end())
         {
            // Package is needed, protect it to prohibit automatic removal
            Cache.MarkKeep(pkg_iter);
            Fix.Protect(pkg_iter);
         }
         else
         {
            Cache.MarkDelete(pkg_iter, _config->FindB("APT::Get::Purge",false));
         }
      }
   }

   return Fix.Resolve(false);
}

bool pkgAutoremoveGetKeptAndUnneededPackages(pkgDepCache &Cache, std::set<std::string> *o_kept_packages, std::set<std::string> *o_unneeded_packages)
{
   // if package is still needed, put it into this set
   std::set<const char*> kept_packages;

   if (!find_all_kept_packages(Cache, kept_packages))
   {
      return false;
   }

   if (o_kept_packages)
   {
      o_kept_packages->clear();
   }

   if (o_unneeded_packages)
   {
      o_unneeded_packages->clear();
   }

   // Finally, go through all packages once more and split them into two sets
   for (pkgCache::PkgIterator pkg_iter = Cache.PkgBegin(); not pkg_iter.end(); ++pkg_iter)
   {
      // Skip packages not installed
      if (pkg_iter->CurrentState == pkgCache::State::Installed)
      {
         if (kept_packages.find(pkg_iter.Name()) != kept_packages.end())
         {
            if (o_kept_packages)
            {
               o_kept_packages->insert(pkg_iter.Name());
            }
         }
         else
         {
            if (o_unneeded_packages)
            {
               o_unneeded_packages->insert(pkg_iter.Name());
            }
         }
      }
   }

   return true;
}

// ProblemResolver::pkgProblemResolver - Constructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgProblemResolver::pkgProblemResolver(pkgDepCache *pCache) : Cache(*pCache)
{
   // Allocate memory
   unsigned long Size = Cache.Head().PackageCount;
   Scores = new signed short[Size];
   // allocate and zero memory
   Flags = new unsigned char[Size]();

   // Set debug to true to see its decision logic
   Debug = _config->FindB("Debug::pkgProblemResolver",false);
}
									/*}}}*/
// ProblemResolver::~pkgProblemResolver - Destructor			/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgProblemResolver::~pkgProblemResolver()
{
   delete [] Scores;
   delete [] Flags;
}
									/*}}}*/
// ProblemResolver::ScoreSort - Sort the list by score			/*{{{*/
// ---------------------------------------------------------------------
/* */
int pkgProblemResolver::ScoreSort(const void *a,const void *b)
{
   Package const **A = (Package const **)a;
   Package const **B = (Package const **)b;
   if (This->Scores[(*A)->ID] > This->Scores[(*B)->ID])
      return -1;
   if (This->Scores[(*A)->ID] < This->Scores[(*B)->ID])
      return 1;
   return 0;
}
									/*}}}*/
// ProblemResolver::MakeScores - Make the score table			/*{{{*/
// ---------------------------------------------------------------------
/* */

#include "rpm/rpmpackagedata.h"

void pkgProblemResolver::MakeScores()
{
   unsigned long Size = Cache.Head().PackageCount;
   memset(Scores,0,sizeof(*Scores)*Size);

   // Generate the base scores for a package based on its properties
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if (Cache[I].InstallVer == 0)
	 continue;

      signed short &Score = Scores[I->ID];

      /* This is arbitary, it should be high enough to elevate an
         essantial package above most other packages but low enough
	 to allow an obsolete essential packages to be removed by
	 a conflicts on a powerfull normal package (ie libc6) */
      if ((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	 Score += 100;

      // We transform the priority
      // Important Required Standard Optional Extra
      signed short PrioMap[] = {0,3,2,1,-1,-2};
      if (Cache[I].InstVerIter(Cache)->Priority <= 5)
	 Score += PrioMap[Cache[I].InstVerIter(Cache)->Priority];

      /* This helps to fix oddball problems with conflicting packages
         on the same level. We enhance the score of installed packages */
      if (I->CurrentVer != 0)
	 Score += 1;
   }

   // Now that we have the base scores we go and propogate dependencies
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if (Cache[I].InstallVer == 0)
	 continue;

      for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList(); D.end() == false; D++)
      {
	 if (D->Type == pkgCache::Dep::Depends || D->Type == pkgCache::Dep::PreDepends)
	    Scores[D.TargetPkg()->ID]++;
      }
   }

   // Copy the scores to advoid additive looping
   const SPtrArray<signed short> OldScores(new signed short[Size]);
   memcpy(OldScores.get(),Scores,sizeof(*Scores)*Size);

   /* Now we cause 1 level of dependency inheritance, that is we add the
      score of the packages that depend on the target Package. This
      fortifies high scoring packages */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if (Cache[I].InstallVer == 0)
	 continue;

      for (pkgCache::DepIterator D = I.RevDependsList(); D.end() == false; D++)
      {
	 // Only do it for the install version
	 if ((pkgCache::Version *)D.ParentVer() != Cache[D.ParentPkg()].InstallVer ||
	     (D->Type != pkgCache::Dep::Depends && D->Type != pkgCache::Dep::PreDepends))
	    continue;

	 Scores[I->ID] += abs(OldScores[D.ParentPkg()->ID]);
      }
   }

   /* Now we propogate along provides. This makes the packages that
      provide important packages extremely important */
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      for (pkgCache::PrvIterator P = I.ProvidesList(); P.end() == false; P++)
      {
	 // Only do it once per package
	 if ((pkgCache::Version *)P.OwnerVer() != Cache[P.OwnerPkg()].InstallVer)
	    continue;
	 Scores[P.OwnerPkg()->ID] += abs(Scores[I->ID] - OldScores[I->ID]);
      }
   }

   /* Protected things are pushed really high up. This number should put them
      ahead of everything */
   RPMPackageData *rpmdata = new RPMPackageData();
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if ((Flags[I->ID] & Protected) != 0)
	 Scores[I->ID] += 10000;
      if ((I->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
	 Scores[I->ID] += 5000;
      pkgCache::State::VerPriority p = rpmdata->VerPriority(I.Name());
      switch (p) {
         case pkgCache::State::Important:
	   Scores[I->ID] += 1000;
	   [[fallthrough]];
	 case pkgCache::State::Required:
	   Scores[I->ID] += 1000;
	   [[fallthrough]];
	 case pkgCache::State::Standard:
	   Scores[I->ID] += 1000;
	   [[fallthrough]];
	 case pkgCache::State::Optional:
	 case pkgCache::State::Extra:
	 default:
	   break;
      }
   }
   This = this;
}
									/*}}}*/
// ProblemResolver::DoUpgrade - Attempt to upgrade this package		/*{{{*/
// ---------------------------------------------------------------------
/* This goes through and tries to reinstall packages to make this package
   installable */
bool pkgProblemResolver::DoUpgrade(pkgCache::PkgIterator Pkg)
{
   if ((Flags[Pkg->ID] & Upgradable) == 0 || Cache[Pkg].Upgradable() == false)
      return false;
   if ((Flags[Pkg->ID] & Protected) == Protected)
      return false;

   Flags[Pkg->ID] &= ~Upgradable;

   bool WasKept = Cache[Pkg].Keep();
   Cache.MarkInstall(Pkg,pkgDepCache::AutoMarkFlag::DontChange,false);

   // This must be a virtual package or something like that.
   if (Cache[Pkg].InstVerIter(Cache).end() == true)
      return false;

   // Isolate the problem dependency
   bool Fail = false;
   for (pkgCache::DepIterator D = Cache[Pkg].InstVerIter(Cache).DependsList(); D.end() == false;)
   {
      // Compute a single dependency element (glob or)
      pkgCache::DepIterator Start = D;
      pkgCache::DepIterator End = D;
      unsigned char State = 0;
      for (bool LastOR = true; D.end() == false && LastOR == true;)
      {
	 State |= Cache[D];
	 LastOR = (D->CompareOp & pkgCache::Dep::Or) == pkgCache::Dep::Or;
	 D++;
	 if (LastOR == true)
	    End = D;
      }

      // We only worry about critical deps.
      if (End.IsCritical() != true)
	 continue;

      // Iterate over all the members in the or group
      while (1)
      {
	 // Dep is ok now
	 if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    break;

	 // Do not change protected packages
	 PkgIterator P = Start.SmartTargetPkg();
	 if ((Flags[P->ID] & Protected) == Protected)
	 {
	    if (Debug == true)
	       clog << "    Reinst Failed because of protected " << P.Name() << endl;
	    Fail = true;
	 }
	 else
	 {
	    // Upgrade the package if the candidate version will fix the problem.
	    if ((Cache[Start] & pkgDepCache::DepCVer) == pkgDepCache::DepCVer)
	    {
	       if (DoUpgrade(P) == false)
	       {
		  if (Debug == true)
		     clog << "    Reinst Failed because of " << P.Name() << endl;
		  Fail = true;
	       }
	       else
	       {
		  Fail = false;
		  break;
	       }
	    }
	    else
	    {
	       /* We let the algorithm deal with conflicts on its next iteration,
		it is much smarter than us */
	       if (Start->Type == pkgCache::Dep::Conflicts ||
		   Start->Type == pkgCache::Dep::Obsoletes)
		   break;

	       if (Debug == true)
		  clog << "    Reinst Failed early because of " << Start.TargetPkg().Name() << endl;
	       Fail = true;
	    }
	 }

	 if (Start == End)
	    break;
	 Start++;
      }
      if (Fail == true)
	 break;
   }

   // Undo our operations - it might be smart to undo everything this did..
   if (Fail == true)
   {
      if (WasKept == true)
	 Cache.MarkKeep(Pkg);
      else
	 Cache.MarkDelete(Pkg);
      return false;
   }

   if (Debug == true)
      clog << "  Re-Instated " << Pkg.Name() << endl;
   return true;
}
									/*}}}*/
// ProblemResolver::Resolve - Run the resolution pass			/*{{{*/
// ---------------------------------------------------------------------
/* This routines works by calculating a score for each package. The score
   is derived by considering the package's priority and all reverse
   dependents giving an integer that reflects the amount of breakage that
   adjusting the package will inflict.

   It goes from highest score to lowest and corrects all of the breaks by
   keeping or removing the dependant packages. If that fails then it removes
   the package itself and goes on. The routine should be able to intelligently
   go from any broken state to a fixed state.

   The BrokenFix flag enables a mode where the algorithm tries to
   upgrade packages to advoid problems. */
bool pkgProblemResolver::Resolve(bool BrokenFix)
{
   unsigned long Size = Cache.Head().PackageCount;

   // Record which packages are marked for install
   bool Again = false;
   do
   {
      Again = false;
      for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      {
	 if (Cache[I].Install() == true)
	    Flags[I->ID] |= PreInstalled;
	 else
	 {
	    if (Cache[I].InstBroken() == true && BrokenFix == true)
	    {
	       Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
	       if (Cache[I].Install() == true)
		  Again = true;
	    }

	    Flags[I->ID] &= ~PreInstalled;
	 }
	 Flags[I->ID] |= Upgradable;
      }
   }
   while (Again == true);

   if (Debug == true)
      clog << "Starting" << endl;

   MakeScores();

   /* We have to order the packages so that the broken fixing pass
      operates from highest score to lowest. This prevents problems when
      high score packages cause the removal of lower score packages that
      would cause the removal of even lower score packages. */
   const SPtrArray<pkgCache::Package *> PList(new pkgCache::Package *[Size]);
   pkgCache::Package **PEnd = PList.get();
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      *PEnd++ = I;
   This = this;
   qsort(PList.get(),PEnd - PList.get(),sizeof(PList[0]),&ScoreSort);

/* for (pkgCache::Package **K = PList; K != PEnd; K++)
      if (Scores[(*K)->ID] != 0)
      {
	 pkgCache::PkgIterator Pkg(Cache,*K);
	 clog << Scores[(*K)->ID] << ' ' << Pkg.Name() <<
	    ' ' << (pkgCache::Version *)Pkg.CurrentVer() << ' ' <<
	    Cache[Pkg].InstallVer << ' ' << Cache[Pkg].CandidateVer << endl;
      } */

   if (Debug == true)
      clog << "Starting 2" << endl;

   /* Now consider all broken packages. For each broken package we either
      remove the package or fix it's problem. We do this once, it should
      not be possible for a loop to form (that is a < b < c and fixing b by
      changing a breaks c) */
   bool Change = true;
   for (int Counter = 0; Counter != 10 && Change == true; Counter++)
   {
      Change = false;
      for (pkgCache::Package **K = PList.get(); K != PEnd; K++)
      {
	 pkgCache::PkgIterator I(Cache,*K);

	 /* We attempt to install this and see if any breaks result,
	    this takes care of some strange cases */
	 if (Cache[I].CandidateVer != Cache[I].InstallVer &&
	     I->CurrentVer != 0 && Cache[I].InstallVer != 0 &&
	     (Flags[I->ID] & PreInstalled) != 0 &&
	     (Flags[I->ID] & Protected) == 0 &&
	     (Flags[I->ID] & ReInstateTried) == 0)
	 {
	    if (Debug == true)
	       clog << " Try to Re-Instate " << I.Name() << endl;
	    unsigned long OldBreaks = Cache.BrokenCount();
	    pkgCache::Version *OldVer = Cache[I].InstallVer;
	    Flags[I->ID] &= ReInstateTried;

	    Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
	    if (Cache[I].InstBroken() == true ||
		OldBreaks < Cache.BrokenCount())
	    {
	       if (OldVer == 0)
		  Cache.MarkDelete(I);
	       else
		  Cache.MarkKeep(I);
	    }
	    else
	       if (Debug == true)
		  clog << "Re-Instated " << I.Name() << " (" << OldBreaks << " vs " << Cache.BrokenCount() << ')' << endl;
	 }

	 if (Cache[I].InstallVer == 0 || Cache[I].InstBroken() == false)
	    continue;

	 if (Debug == true)
	    cout << "Investigating " << I.Name() << endl;

	 // Isolate the problem dependency
	 PackageKill KillList[100];
	 PackageKill *LEnd = KillList;
	 bool InOr = false;
	 pkgCache::DepIterator Start;
	 pkgCache::DepIterator End;
	 PackageKill *OldEnd = LEnd;

	 enum {OrRemove,OrKeep} OrOp = OrRemove;
	 for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList();
	      D.end() == false || InOr == true;)
	 {
	    // Compute a single dependency element (glob or)
	    if (Start == End)
	    {
	       // Decide what to do
	       if (InOr == true)
	       {
		  if (OldEnd == LEnd && OrOp == OrRemove)
		  {
		     if ((Flags[I->ID] & Protected) != Protected)
		     {
			if (Debug == true)
			   clog << "  Or group remove for " << I.Name() << endl;
			Cache.MarkDelete(I);
			Change = true;
		     }
		  }
		  if (OldEnd == LEnd && OrOp == OrKeep)
		  {
		     if (Debug == true)
			clog << "  Or group keep for " << I.Name() << endl;
		     Cache.MarkKeep(I);
		     Change = true;
		  }
	       }

	       /* We do an extra loop (as above) to finalize the or group
		  processing */
	       InOr = false;
	       OrOp = OrRemove;
	       D.GlobOr(Start,End);
	       if (Start.end() == true)
		  break;

	       // We only worry about critical deps.
	       if (End.IsCritical() != true)
		  continue;

	       InOr = Start != End;
	       OldEnd = LEnd;
	    }
	    else
	       Start++;

	    // Dep is ok
	    if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    {
	       InOr = false;
	       continue;
	    }

	    if (Debug == true)
	       clog << "Package " << I.Name() << " has broken dep on " << Start.TargetPkg().Name() << endl;

	    /* Look across the version list. If there are no possible
	       targets then we keep the package and bail. This is necessary
	       if a package has a dep on another package that cant be found */
	    const SPtrArray<pkgCache::Version * const> VList(Start.AllTargets());
	    if (VList[0] == nullptr && (Flags[I->ID] & Protected) != Protected &&
		Start->Type != pkgCache::Dep::Conflicts &&
		Start->Type != pkgCache::Dep::Obsoletes &&
		Cache[I].NowBroken() == false)
	    {
	       if (InOr == true)
	       {
		  /* No keep choice because the keep being OK could be the
		     result of another element in the OR group! */
		  continue;
	       }

	       Change = true;
	       Cache.MarkKeep(I);
	       break;
	    }

	    bool Done = false;
	    for (pkgCache::Version * const *V = VList.get(); *V != nullptr; V++)
	    {
	       pkgCache::VerIterator Ver(Cache,*V);
	       pkgCache::PkgIterator Pkg = Ver.ParentPkg();

	       if (Debug == true)
		  clog << "  Considering " << Pkg.Name() << ' ' << (int)Scores[Pkg->ID] <<
		  " as a solution to " << I.Name() << ' ' << (int)Scores[I->ID] << endl;

	       /* Try to fix the package under consideration rather than
	          fiddle with the VList package */
	       if (Scores[I->ID] <= Scores[Pkg->ID] ||
		   ((Cache[Start] & pkgDepCache::DepNow) == 0 &&
		    End->Type != pkgCache::Dep::Conflicts &&
		    End->Type != pkgCache::Dep::Obsoletes))
	       {
		  // Try a little harder to fix protected packages..
		  if ((Flags[I->ID] & Protected) == Protected)
		  {
		     if (DoUpgrade(Pkg) == true)
		     {
			if (Scores[Pkg->ID] > Scores[I->ID])
			   Scores[Pkg->ID] = Scores[I->ID];
			break;
		     }

		     continue;
		  }

		  /* See if a keep will do, unless the package is protected,
		     then installing it will be necessary */
		  bool Installed = Cache[I].Install();
		  Cache.MarkKeep(I);
		  if (Cache[I].InstBroken() == false)
		  {
		     // Unwind operation will be keep now
		     if (OrOp == OrRemove)
			OrOp = OrKeep;

		     // Restore
		     if (InOr == true && Installed == true)
			Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);

		     if (Debug == true)
			clog << "  Holding Back " << I.Name() << " rather than change " << Start.TargetPkg().Name() << endl;
		  }
		  else
		  {
		     if (BrokenFix == false || DoUpgrade(I) == false)
		     {
			// Consider other options
			if (InOr == false)
			{
			   if (Debug == true)
			      clog << "  Removing " << I.Name() << " rather than change " << Start.TargetPkg().Name() << endl;
			   Cache.MarkDelete(I);
			   if (Counter > 1)
			   {
			      if (Scores[Pkg->ID] > Scores[I->ID])
				 Scores[I->ID] = Scores[Pkg->ID];
			   }
			}
		     }
		  }

		  Change = true;
		  Done = true;
		  break;
	       }
	       else
	       {
		  /* This is a conflicts, and the version we are looking
		     at is not the currently selected version of the
		     package, which means it is not necessary to
		     remove/keep */
		  if (Cache[Pkg].InstallVer != Ver.operator const pkgCache::Version *() &&
		      (Start->Type == pkgCache::Dep::Conflicts ||
		       Start->Type == pkgCache::Dep::Obsoletes))
		     continue;

		  // Skip adding to the kill list if it is protected
		  if ((Flags[Pkg->ID] & Protected) != 0)
		     continue;

		  // CNC:2003-03-22
		  pkgDepCache::State State(&Cache);
		  if (BrokenFix == true && DoUpgrade(Pkg) == true)
		  {
		     if (Cache[I].InstBroken() == false &&
			 State.BrokenCount() >= Cache.BrokenCount())
		     {
			if (Debug == true)
			   clog << "  Installing " << Pkg.Name() << endl;
			Change = true;
			break;
		     }
		     else
			State.Restore();
		  }

		  if (Debug == true)
		     clog << "  Added " << Pkg.Name() << " to the remove list" << endl;

		  // CNC:2002-07-09
		  if (*(V+1) != 0) //XXX Look for other solutions?
		      continue;

		  LEnd->Pkg = Pkg;
		  LEnd->Dep = End;
		  LEnd++;

		  if (Start->Type != pkgCache::Dep::Conflicts &&
		      Start->Type != pkgCache::Dep::Obsoletes)
		     break;
	       }
	    }

	    // Hm, nothing can possibly satisify this dep. Nuke it.
	    if (VList[0] == 0 &&
		Start->Type != pkgCache::Dep::Conflicts &&
		Start->Type != pkgCache::Dep::Obsoletes &&
		(Flags[I->ID] & Protected) != Protected)
	    {
	       bool Installed = Cache[I].Install();
	       Cache.MarkKeep(I);
	       if (Cache[I].InstBroken() == false)
	       {
		  // Unwind operation will be keep now
		  if (OrOp == OrRemove)
		     OrOp = OrKeep;

		  // Restore
		  if (InOr == true && Installed == true)
		     Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);

		  if (Debug == true)
		     clog << "  Holding Back " << I.Name() << " because I can't find " << Start.TargetPkg().Name() << endl;
	       }
	       else
	       {
		  if (Debug == true)
		     clog << "  Removing " << I.Name() << " because I can't find " << Start.TargetPkg().Name() << endl;
		  if (InOr == false)
		     Cache.MarkDelete(I);
	       }

	       Change = true;
	       Done = true;
	    }

	    // Try some more
	    if (InOr == true)
	       continue;

	    if (Done == true)
	       break;
	 }

	 // Apply the kill list now
	 if (Cache[I].InstallVer != 0)
	 {
	    for (PackageKill *J = KillList; J != LEnd; J++)
	    {
	       Change = true;
	       if ((Cache[J->Dep] & pkgDepCache::DepGNow) == 0)
	       {
		  if (J->Dep->Type == pkgCache::Dep::Conflicts ||
		      J->Dep->Type == pkgCache::Dep::Obsoletes)
		  {
		     if (Debug == true)
			clog << "  Fixing " << I.Name() << " via remove of " << J->Pkg.Name() << endl;
		     Cache.MarkDelete(J->Pkg);
		  }
	       }
	       else
	       {
		  if (Debug == true)
		     clog << "  Fixing " << I.Name() << " via keep of " << J->Pkg.Name() << endl;
		  Cache.MarkKeep(J->Pkg);
	       }

	       if (Counter > 1)
	       {
		  if (Scores[I->ID] > Scores[J->Pkg->ID])
		     Scores[J->Pkg->ID] = Scores[I->ID];
	       }
	    }
	 }
      }
   }

   if (Debug == true)
      clog << "Done" << endl;

   if (Cache.BrokenCount() != 0)
   {
      // See if this is the result of a hold
      pkgCache::PkgIterator I = Cache.PkgBegin();
      for (;I.end() != true; I++)
      {
	 if (Cache[I].InstBroken() == false)
	    continue;
	 if ((Flags[I->ID] & Protected) != Protected)
	    return _error->Error(_("Error, pkgProblemResolver::Resolve generated breaks, this may be caused by held packages."));
      }
      return _error->Error(_("Unable to correct problems, you have held broken packages."));
   }

   return true;
}
									/*}}}*/
// ProblemResolver::ResolveByKeep - Resolve problems using keep		/*{{{*/
// ---------------------------------------------------------------------
/* This is the work horse of the soft upgrade routine. It is very gental
   in that it does not install or remove any packages. It is assumed that the
   system was non-broken previously. */
bool pkgProblemResolver::ResolveByKeep()
{
   unsigned long Size = Cache.Head().PackageCount;

   if (Debug == true)
      clog << "Entering ResolveByKeep" << endl;

   MakeScores();

   /* We have to order the packages so that the broken fixing pass
      operates from highest score to lowest. This prevents problems when
      high score packages cause the removal of lower score packages that
      would cause the removal of even lower score packages. */
   pkgCache::Package **PList = new pkgCache::Package *[Size];
   pkgCache::Package **PEnd = PList;
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
      *PEnd++ = I;
   This = this;
   qsort(PList,PEnd - PList,sizeof(*PList),&ScoreSort);

   // Consider each broken package
   pkgCache::Package **LastStop = 0;
   for (pkgCache::Package **K = PList; K != PEnd; K++)
   {
      pkgCache::PkgIterator I(Cache,*K);

      if (Cache[I].InstallVer == 0 || Cache[I].InstBroken() == false)
	 continue;

      /* Keep the package. If this works then great, otherwise we have
	 to be significantly more agressive and manipulate its dependencies */
      if ((Flags[I->ID] & Protected) == 0)
      {
	 if (Debug == true)
	    clog << "Keeping package " << I.Name() << endl;
	 Cache.MarkKeep(I);
	 if (Cache[I].InstBroken() == false)
	 {
	    K = PList - 1;
	    continue;
	 }
      }

      // Isolate the problem dependencies
      for (pkgCache::DepIterator D = Cache[I].InstVerIter(Cache).DependsList(); D.end() == false;)
      {
	 DepIterator Start;
	 DepIterator End;
	 D.GlobOr(Start,End);

	 // We only worry about critical deps.
	 if (End.IsCritical() != true)
	    continue;

	 // Dep is ok
	 if ((Cache[End] & pkgDepCache::DepGInstall) == pkgDepCache::DepGInstall)
	    continue;

	 /* Hm, the group is broken.. I suppose the best thing to do is to
	    is to try every combination of keep/not-keep for the set, but thats
	    slow, and this never happens, just be conservative and assume the
	    list of ors is in preference and keep till it starts to work. */
	 while (true)
	 {
	    if (Debug == true)
	       clog << "Package " << I.Name() << " has broken dep on " << Start.TargetPkg().Name() << endl;

	    // Look at all the possible provides on this package
	    const SPtrArray<pkgCache::Version * const> VList(Start.AllTargets());
	    for (pkgCache::Version * const *V = VList.get(); *V != nullptr; V++)
	    {
	       pkgCache::VerIterator Ver(Cache,*V);
	       pkgCache::PkgIterator Pkg = Ver.ParentPkg();

	       // It is not keepable
	       if (Cache[Pkg].InstallVer == 0 ||
		   Pkg->CurrentVer == 0)
		  continue;

	       // CNC:2002-08-05
	       if ((Flags[Pkg->ID] & Protected) == 0)
	       {
		  if (Debug == true)
		     clog << "  Keeping Package " << Pkg.Name() << " due to dep" << endl;
		  Cache.MarkKeep(Pkg);
	       }

	       if (Cache[I].InstBroken() == false)
		  break;
	    }

	    if (Cache[I].InstBroken() == false)
	       break;

	    if (Start == End)
	       break;
	    Start++;
	 }

	 if (Cache[I].InstBroken() == false)
	    break;
      }

      if (Cache[I].InstBroken() == true)
	 continue;

      // Restart again.
      if (K == LastStop)
	 return _error->Error("Internal Error, pkgProblemResolver::ResolveByKeep is looping on package %s.",I.Name());
      LastStop = K;
      K = PList - 1;
   }

   return true;
}
									/*}}}*/
// ProblemResolver::InstallProtect - Install all protected packages	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to make sure protected packages are installed */
void pkgProblemResolver::InstallProtect()
{
   for (pkgCache::PkgIterator I = Cache.PkgBegin(); I.end() == false; I++)
   {
      if ((Flags[I->ID] & Protected) == Protected)
      {
	 if ((Flags[I->ID] & ToRemove) == ToRemove)
	    Cache.MarkDelete(I);
	 else
	    Cache.MarkInstall(I,pkgDepCache::AutoMarkFlag::DontChange,false);
      }
   }
}
									/*}}}*/
// CNC:2002-08-01
// ProblemSolver::RemoveDepends - Remove dependencies selectively	/*{{{*/
// ---------------------------------------------------------------------
// This will remove every dependency which is required only by packages
// already being removed. This will allow one to reverse the effect a
// task package, for example.
bool pkgProblemResolver::RemoveDepends()
{
   bool Debug = _config->FindB("Debug::pkgRemoveDepends",false);
   bool MoreSteps = true;
   while (MoreSteps == true)
   {
      MoreSteps = false;
      for (pkgCache::PkgIterator Pkg = Cache.PkgBegin();
	   Pkg.end() == false; Pkg++)
      {
	 if (Cache[Pkg].Delete() == false)
	    continue;
	 for (pkgCache::DepIterator D = Pkg.CurrentVer().DependsList();
	      D.end() == false; D++)
	 {
	    if (D->Type != pkgCache::Dep::Depends &&
		D->Type != pkgCache::Dep::PreDepends)
	       continue;

	    pkgCache::PkgIterator DPkg = D.TargetPkg();
	    if (DPkg->CurrentVer == 0 || Cache[DPkg].Delete() == true)
	       continue;
	    if ((Flags[DPkg->ID] & Protected) == Protected)
	       continue;

	    bool Remove = true;

	    // Check if another package not being removed or being
	    // installed requires this dependency.
	    for (pkgCache::DepIterator R = DPkg.RevDependsList();
		 R.end() == false; R++)
	    {
	       pkgCache::PkgIterator RPkg = R.ParentPkg();

	       if (R->Type != pkgCache::Dep::Depends &&
		   R->Type != pkgCache::Dep::PreDepends)
		  continue;

	       if ((Cache[RPkg].Install() &&
		    (pkgCache::Version*)R.ParentVer() == Cache[RPkg].InstallVer &&
		    Cache.VS().CheckDep(DPkg.CurrentVer().VerStr(), R) == true) ||
		   (RPkg->CurrentVer != 0 &&
		    Cache[RPkg].Install() == false &&
		    Cache[RPkg].Delete() == false &&
		    Cache.VS().CheckDep(DPkg.CurrentVer().VerStr(), R) == true))
	       {
		  Remove = false;
		  break;
	       }
	    }

	    if (Remove == false)
	       continue;

	    // Also check every virtual package provided by this
	    // dependency is required by packages not being removed,
	    // or being installed.
	    for (pkgCache::PrvIterator P = DPkg.CurrentVer().ProvidesList();
		 P.end() == false; P++)
	    {
	       pkgCache::PkgIterator PPkg = P.ParentPkg();
	       for (pkgCache::DepIterator R = PPkg.RevDependsList();
		    R.end() == false; R++)
	       {
		  pkgCache::PkgIterator RPkg = R.ParentPkg();

		  if (R->Type != pkgCache::Dep::Depends &&
		      R->Type != pkgCache::Dep::PreDepends)
		     continue;

		  if ((Cache[RPkg].Install() &&
		       (pkgCache::Version*)R.ParentVer() == Cache[RPkg].InstallVer &&
		       Cache.VS().CheckDep(P.ProvideVersion(), R) == true) ||
		      (RPkg->CurrentVer != 0 &&
		       Cache[RPkg].Install() == false &&
		       Cache[RPkg].Delete() == false &&
		       Cache.VS().CheckDep(P.ProvideVersion(), R) == true))
		  {
		     Remove = false;
		     break;
		  }
	       }
	    }

	    if (Remove == false)
	       continue;

	    if (Debug == true)
	       clog << "Marking " << DPkg.Name() << " as a removable dependency of " << Pkg.Name() << endl;

	    Cache.MarkDelete(DPkg);

	    // Do at least one more step, to ensure that packages which
	    // were being hold because of this one also get removed.
	    MoreSteps = true;
	 }
      }
   }
   return true;
}
									/*}}}*/
/* Like strcmp, but compares digit sections by number value.
 * E.g.: tar-1.10 > tar-1.9 > tar-1.1a
 * (while strcmp gives tar-1.10 < tar-1.9). */
static int nameCompare(const char* n1, const char* n2)
{
   while(*n1 && *n2) {
      if(isdigit(*n1) && isdigit(*n2)) {
	 unsigned long long i1, i2;
	 i1 = strtoull(n1, const_cast<char **>(&n1), 10);
	 i2 = strtoull(n2, const_cast<char **>(&n2), 10);
	 if(i1 != i2)
	    return (i1 > i2) ? 1 : -1;
      } else if(*n1 != *n2) {
	 return (*n1 > *n2) ? 1 : -1;
      } else {
	 n1++;
	 n2++;
      }
   }
   return 0;
}

// PrioSortList - Sort a list of versions by priority			/*{{{*/
// ---------------------------------------------------------------------
/* This is meant to be used in conjunction with AllTargets to get a list
   of versions ordered by preference. */
static pkgCache *PrioCache;
static int PrioComp(const void *A,const void *B)
{
   pkgCache::VerIterator L(*PrioCache,*(pkgCache::Version **)A);
   pkgCache::VerIterator R(*PrioCache,*(pkgCache::Version **)B);

   // CNC:2002-11-27
   if ((R.ParentPkg()->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential &&
       (L.ParentPkg()->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential)
   return 1;
   if ((R.ParentPkg()->Flags & pkgCache::Flag::Essential) != pkgCache::Flag::Essential &&
       (L.ParentPkg()->Flags & pkgCache::Flag::Essential) == pkgCache::Flag::Essential)
   return -1;

   if (L->Priority != R->Priority)
      return L->Priority - R->Priority;
   // PrioComp("gcc2","gcc3") == 1
   // PrioComp("gcc2", gcc10") == 1
   return nameCompare(R.ParentPkg().Name(),L.ParentPkg().Name());
}
void pkgPrioSortList(pkgCache &Cache,pkgCache::Version **List)
{
   unsigned long Count = 0;
   PrioCache = &Cache;
   for (pkgCache::Version **I = List; *I != 0; I++)
      Count++;
   qsort(List,Count,sizeof(*List),PrioComp);
}
									/*}}}*/
// vim:sts=3:sw=3
