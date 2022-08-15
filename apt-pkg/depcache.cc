// Description								/*{{{*/
// $Id: depcache.cc,v 1.1 2002/07/23 17:54:50 niemeyer Exp $
/* ######################################################################

   Dependency Cache - Caches Dependency information.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/depcache.h>
#include <apt-pkg/version.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sptr.h>
#include <apt-pkg/algorithms.h>

// for Debug::pkgMarkInstall
#include <apt-pkg/configuration.h>

// CNC:2002-07-05
#include <apt-pkg/pkgsystem.h>

// CNC:2003-03-17
#include <apt-pkg/luaiface.h>

#include <apti18n.h>

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

#include <fstream>

									/*}}}*/

// DepCache::pkgDepCache - Constructors					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDepCache::pkgDepCache(pkgCache * const pCache,Policy * const Plcy) :
                Cache(pCache), PkgState(0), DepState(0)
{
   delLocalPolicy = 0;
   LocalPolicy = Plcy;
   if (LocalPolicy == 0)
      delLocalPolicy = LocalPolicy = new Policy;
}
									/*}}}*/
// DepCache::~pkgDepCache - Destructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgDepCache::~pkgDepCache()
{
   delete [] PkgState;
   delete [] DepState;
   delete delLocalPolicy;
}
									/*}}}*/
// DepCache::Init - Generate the initial extra structures.		/*{{{*/
// ---------------------------------------------------------------------
/* This allocats the extension buffers and initializes them. */
bool pkgDepCache::Init(OpProgress * const Prog)
{
   delete [] PkgState;
   delete [] DepState;
   // allocate and zero memory (for a struct with
   // no user-provided constructors and only non-class members)
   PkgState = new StateCache[Head().PackageCount]();
   // allocate and zero memory
   DepState = new unsigned char[Head().DependsCount]();

   if (Prog != 0)
   {
      Prog->OverallProgress(0,2*Head().PackageCount,Head().PackageCount,
			    _("Building Dependency Tree"));
      Prog->SubProgress(Head().PackageCount,_("Candidate Versions"));
   }

   /* Set the current state of everything. In this state all of the
      packages are kept exactly as is. See AllUpgrade */
   int Done = 0;
   for (PkgIterator I = PkgBegin(); I.end() != true; I++,Done++)
   {
      if (Prog != 0)
	 Prog->Progress(Done);

      // Find the proper cache slot
      StateCache &State = PkgState[I->ID];
      State.iFlags = 0;

      State.Flags = 0;

      if (I->Flags & pkgCache::Flag::Auto)
      {
          State.Flags |= Flag::Auto;
      }

      // Figure out the install version
      State.CandidateVer = GetCandidateVer(I).operator pkgCache::Version *();
      State.InstallVer = I.CurrentVer().operator pkgCache::Version *();
      State.Mode = ModeKeep;

      State.Update(I,*this);
   }

   if (Prog != 0)
   {

      Prog->OverallProgress(Head().PackageCount,2*Head().PackageCount,
			    Head().PackageCount,
			    _("Building Dependency Tree"));
      Prog->SubProgress(Head().PackageCount,_("Dependency Generation"));
   }

   Update(Prog);

// CNC:2003-03-17
#ifdef WITH_LUA
   _lua->SetDepCache(this);
   _lua->RunScripts("Scripts::Cache::Init", true);
   _lua->ResetCaches();
#endif

   return true;
}
									/*}}}*/

// DepCache::CheckDep - Checks a single dependency			/*{{{*/
// ---------------------------------------------------------------------
/* This first checks the dependency against the main target package and
   then walks along the package provides list and checks if each provides
   will be installed then checks the provides against the dep. Res will be
   set to the package which was used to satisfy the dep. */
bool pkgDepCache::CheckDep(DepIterator Dep,int const Type,PkgIterator &Res)
{
// CNC:2003-02-17 - This function is the main path when checking
//                  dependencies, so we must avoid as much overhead
//                  as possible. It has several changes to improve
//                  performance.
#if 0
   Res = Dep.TargetPkg();

   /* Check simple depends. A depends -should- never self match but
      we allow it anyhow because dpkg does. Technically it is a packaging
      bug. Conflicts may never self match */
   if (Dep.TargetPkg() != Dep.ParentPkg() ||
       (Dep->Type != Dep::Conflicts && Dep->Type != Dep::Obsoletes))
   {
      PkgIterator const Pkg = Dep.TargetPkg();
      // Check the base package
      if (Type == NowVersion && Pkg->CurrentVer != 0)
	 // CNC:2002-07-10 - RPM must check the dependency type to properly
	 //                  define if it would be satisfied or not.
	 if (VS().CheckDep(Pkg.CurrentVer().VerStr(),Dep) == true)
	    return true;

      if (Type == InstallVersion && PkgState[Pkg->ID].InstallVer != 0)
	 if (VS().CheckDep(PkgState[Pkg->ID].InstVerIter(*this).VerStr(),
				 Dep) == true)
	    return true;

      if (Type == CandidateVersion && PkgState[Pkg->ID].CandidateVer != 0)
	 if (VS().CheckDep(PkgState[Pkg->ID].CandidateVerIter(*this).VerStr(),
				 Dep) == true)
	    return true;
   }

   if (Dep->Type == Dep::Obsoletes)
      return false;

   // Check the providing packages
   PrvIterator P = Dep.TargetPkg().ProvidesList();
   PkgIterator const Pkg = Dep.ParentPkg();
   for (; P.end() != true; P++)
   {
      /* Provides may never be applied against the same package if it is
         a conflicts. See the comment above. */
      if (P.OwnerPkg() == Pkg && Dep->Type == Dep::Conflicts)
	 continue;

      // Check if the provides is a hit
      if (Type == NowVersion)
      {
	 if (P.OwnerPkg().CurrentVer() != P.OwnerVer())
	    continue;
      }

      if (Type == InstallVersion)
      {
	 StateCache &State = PkgState[P.OwnerPkg()->ID];
	 if (State.InstallVer != (Version *)P.OwnerVer())
	    continue;
      }

      if (Type == CandidateVersion)
      {
	 StateCache &State = PkgState[P.OwnerPkg()->ID];
	 if (State.CandidateVer != (Version *)P.OwnerVer())
	    continue;
      }

      // Compare the versions.
      if (VS().CheckDep(P.ProvideVersion(),Dep) == true) // CNC:2002-07-10
      {
	 Res = P.OwnerPkg();
	 return true;
      }
   }

   // CNC:2002-07-05
   if (_system->IgnoreDep(VS(),Dep) == true)
   {
      Res = Dep.TargetPkg();
      return true;
   }

#else

   Res = Dep.TargetPkg();

   // CNC:2003-02-17 - Res is currently not changed. If it happens to
   //		       be changed in this function (besides when
   //		       returning anyway), assign a new Dep.TargetPkg()
   //		       to Dep_TargetPkg instead of using Res.
   //
   const PkgIterator &Dep_TargetPkg = Res;
   PkgIterator const Dep_ParentPkg = Dep.ParentPkg();
   pkgVersioningSystem &VS = this->VS();

   static const char *lastV;
   static const pkgCache::Dependency *lastD;
   static bool lastRet;
#define VS_CheckDep(V, D) \
   ({ \
      if (!(lastV == V && lastD == D)) { \
	 lastV = V; lastD = D; \
	 lastRet = VS.CheckDep(V, D); \
      } \
      lastRet; \
   })

   /* Check simple depends. A depends -should- never self match but
      we allow it anyhow because dpkg does. Technically it is a packaging
      bug. Conflicts may never self match */
   if (Dep_TargetPkg != Dep_ParentPkg ||
       (Dep->Type != Dep::Conflicts && Dep->Type != Dep::Obsoletes))
   {
      const PkgIterator &Pkg = Dep_TargetPkg;

      // Check the base package
      switch (Type)
      {
	 case NowVersion:
	    if (Pkg->CurrentVer != 0)
	       // CNC:2002-07-10 - RPM must check the dependency type to
	       //		   properly define if it would be satisfied
	       //		   or not.
	       if (VS_CheckDep(Pkg.CurrentVer().VerStr(),Dep) == true)
		  return true;
	    break;

	 case InstallVersion:
	    if (PkgState[Pkg->ID].InstallVer != 0)
	       if (VS_CheckDep(PkgState[Pkg->ID].InstVerIter(*this).VerStr(),
				       Dep) == true)
		  return true;
	    break;

	 case CandidateVersion:
	    if (PkgState[Pkg->ID].CandidateVer != 0)
	       if (VS_CheckDep(PkgState[Pkg->ID].CandidateVerIter(*this).VerStr(),
				       Dep) == true)
		  return true;
	    break;
      }


   }

   if (Dep->Type == Dep::Obsoletes)
      return false;

   // Check the providing packages
   PrvIterator P = Dep_TargetPkg.ProvidesList();
   const PkgIterator &Pkg = Dep_ParentPkg;
   for (; P.end() != true; P++)
   {
      PkgIterator const P_OwnerPkg = P.OwnerPkg();

      /* Provides may never be applied against the same package if it is
         a conflicts. See the comment above. */
      if (P_OwnerPkg == Pkg && Dep->Type == Dep::Conflicts)
	 continue;

      // Check if the provides is a hit
      switch (Type)
      {
	 case NowVersion:
	    {
	       if (P_OwnerPkg.CurrentVer() != P.OwnerVer())
		  continue;
	       break;
	    }
	 case InstallVersion:
	    {
	       StateCache &State = PkgState[P_OwnerPkg->ID];
	       if (State.InstallVer != (Version *)P.OwnerVer())
		  continue;
	       break;
	    }
	 case CandidateVersion:
	    {
	       const StateCache &State = PkgState[P_OwnerPkg->ID];
	       if (State.CandidateVer != (Version *)P.OwnerVer())
		  continue;
	       break;
	    }
      }

      // Compare the versions.
      if (VS_CheckDep(P.ProvideVersion(),Dep) == true) // CNC:2002-07-10
      {
	 Res = P_OwnerPkg;
	 return true;
      }
   }

   // CNC:2002-07-05
   if (_system->IgnoreDep(VS,Dep) == true)
   {
      Res = Dep_TargetPkg;
      return true;
   }
#endif

   return false;
}
									/*}}}*/
// DepCache::AddSizes - Add the packages sizes to the counters		/*{{{*/
// ---------------------------------------------------------------------
/* Call with Mult = -1 to preform the inverse opration */
void pkgDepCache::AddSizes(const PkgIterator &Pkg,signed long const Mult)
{
   StateCache &P = PkgState[Pkg->ID];

   if (Pkg->VersionList == 0)
      return;

   if (Pkg.State() == pkgCache::PkgIterator::NeedsConfigure &&
       P.Keep() == true)
      return;

   // Compute the size data
   if (P.NewInstall() == true)
   {
      iUsrSize += (signed long long)(Mult*P.InstVerIter(*this)->InstalledSize);
      iDownloadSize += (signed long long)(Mult*P.InstVerIter(*this)->Size);
      return;
   }

   // Upgrading
   if (Pkg->CurrentVer != 0 &&
       (P.InstallVer != (Version *)Pkg.CurrentVer() ||
	(P.iFlags & ReInstall) == ReInstall) && P.InstallVer != 0)
   {
      iUsrSize += (signed long long)(Mult*((signed long long)P.InstVerIter(*this)->InstalledSize -
			(signed long long)Pkg.CurrentVer()->InstalledSize));
      iDownloadSize += (signed long long)(Mult*P.InstVerIter(*this)->Size);
      return;
   }

   // Reinstall
   if (Pkg.State() == pkgCache::PkgIterator::NeedsUnpack &&
       P.Delete() == false)
   {
      iDownloadSize += (signed long long)(Mult*P.InstVerIter(*this)->Size);
      return;
   }

   // Removing
   if (Pkg->CurrentVer != 0 && P.InstallVer == 0)
   {
      iUsrSize -= (signed long long)(Mult*Pkg.CurrentVer()->InstalledSize);
      return;
   }
}
									/*}}}*/
// DepCache::AddStates - Add the package to the state counter		/*{{{*/
// ---------------------------------------------------------------------
/* This routine is tricky to use, you must make sure that it is never
   called twice for the same package. This means the Remove/Add section
   should be as short as possible and not encompass any code that will
   calld Remove/Add itself. Remember, dependencies can be circular so
   while processing a dep for Pkg it is possible that Add/Remove
   will be called on Pkg */
void pkgDepCache::AddStates(const PkgIterator &Pkg,int const Add)
{
   const StateCache &State = PkgState[Pkg->ID];

   // The Package is broken
   if ((State.DepState & DepInstMin) != DepInstMin)
      iBrokenCount += Add;

   // Bad state
   if (Pkg.State() != PkgIterator::NeedsNothing)
      iBadCount += Add;

   // Not installed
   if (Pkg->CurrentVer == 0)
   {
      if (State.Mode == ModeDelete &&
	  (State.iFlags | Purge) == Purge && Pkg.Purge() == false)
	 iDelCount += Add;

      if (State.Mode == ModeInstall)
	 iInstCount += Add;
      return;
   }

   // Installed, no upgrade
   if (State.Status == 0)
   {
      if (State.Mode == ModeDelete)
	 iDelCount += Add;
      else
	 if ((State.iFlags & ReInstall) == ReInstall)
	    iInstCount += Add;

      return;
   }

   // Alll 3 are possible
   if (State.Mode == ModeDelete)
      iDelCount += Add;
   if (State.Mode == ModeKeep)
      iKeepCount += Add;
   if (State.Mode == ModeInstall)
      iInstCount += Add;
}
									/*}}}*/
// DepCache::BuildGroupOrs - Generate the Or group dep data		/*{{{*/
// ---------------------------------------------------------------------
/* The or group results are stored in the last item of the or group. This
   allows easy detection of the state of a whole or'd group. */
void pkgDepCache::BuildGroupOrs(VerIterator const &V)
{
   unsigned char Group = 0;

   for (DepIterator D = V.DependsList(); D.end() != true; D++)
   {
      // Build the dependency state.
      unsigned char &State = DepState[D->ID];

      /* Invert for Conflicts. We have to do this twice to get the
         right sense for a conflicts group */
      if (D->Type == Dep::Conflicts || D->Type == Dep::Obsoletes)
	 State = ~State;

      // Add to the group if we are within an or..
      State &= 0x7;
      Group |= State;
      State |= Group << 3;
      if ((D->CompareOp & Dep::Or) != Dep::Or)
	 Group = 0;

      // Invert for Conflicts
      if (D->Type == Dep::Conflicts || D->Type == Dep::Obsoletes)
	 State = ~State;
   }
}
									/*}}}*/
// DepCache::VersionState - Perform a pass over a dependency list	/*{{{*/
// ---------------------------------------------------------------------
/* This is used to run over a dependency list and determine the dep
   state of the list, filtering it through both a Min check and a Policy
   check. The return result will have SetMin/SetPolicy low if a check
   fails. It uses the DepState cache for it's computations. */
unsigned char pkgDepCache::VersionState(DepIterator D,
                                        unsigned char const Check,
                                        unsigned char const SetMin,
                                        unsigned char const SetPolicy)
{
   unsigned char Dep = 0xFF;

   while (D.end() != true)
   {
      // Compute a single dependency element (glob or)
      DepIterator const Start = D;
      unsigned char State = 0;
      for (bool LastOR = true; D.end() == false && LastOR == true; D++)
      {
	 State |= DepState[D->ID];
	 LastOR = (D->CompareOp & Dep::Or) == Dep::Or;
      }

      // CNC:2003-02-17 - IsImportantDep() currently calls IsCritical(), so
      //		  these two are currently doing the same thing. Check
      //		  comments in IsImportantDep() definition.
#if 0
      // Minimum deps that must be satisfied to have a working package
      if (Start.IsCritical() == true)
	 if ((State & Check) != Check)
	    Dep &= ~SetMin;

      // Policy deps that must be satisfied to install the package
      if (IsImportantDep(Start) == true &&
	  (State & Check) != Check)
	 Dep &= ~SetPolicy;
#else
      if (Start.IsCritical() == true)
	 if ((State & Check) != Check) {
	    Dep &= ~SetMin;
	    Dep &= ~SetPolicy;
	 }
#endif
   }

   return Dep;
}
									/*}}}*/
// DepCache::DependencyState - Compute the 3 results for a dep		/*{{{*/
// ---------------------------------------------------------------------
/* This is the main dependency computation bit. It computes the 3 main
   results for a dependencys, Now, Install and Candidate. Callers must
   invert the result if dealing with conflicts. */
unsigned char pkgDepCache::DependencyState(DepIterator &D)
{
   unsigned char State = 0;

   if (CheckDep(D,NowVersion) == true)
      State |= DepNow;
   if (CheckDep(D,InstallVersion) == true)
      State |= DepInstall;
   if (CheckDep(D,CandidateVersion) == true)
      State |= DepCVer;

   return State;
}
									/*}}}*/
// DepCache::UpdateVerState - Compute the Dep member of the state	/*{{{*/
// ---------------------------------------------------------------------
/* This determines the combined dependency representation of a package
   for its two states now and install. This is done by using the pre-generated
   dependency information. */
void pkgDepCache::UpdateVerState(PkgIterator const Pkg)
{
   // Empty deps are always true
   StateCache &State = PkgState[Pkg->ID];
   State.DepState = 0xFF;

   // Check the Current state
   if (Pkg->CurrentVer != 0)
   {
      DepIterator const D = Pkg.CurrentVer().DependsList();
      State.DepState &= VersionState(D,DepNow,DepNowMin,DepNowPolicy);
   }

   /* Check the candidate state. We do not compare against the whole as
      a candidate state but check the candidate version against the
      install states */
   if (State.CandidateVer != 0)
   {
      DepIterator const D = State.CandidateVerIter(*this).DependsList();
      State.DepState &= VersionState(D,DepInstall,DepCandMin,DepCandPolicy);
   }

   // Check target state which can only be current or installed
   if (State.InstallVer != 0)
   {
      DepIterator const D = State.InstVerIter(*this).DependsList();
      State.DepState &= VersionState(D,DepInstall,DepInstMin,DepInstPolicy);
   }
}
									/*}}}*/
// DepCache::Update - Figure out all the state information		/*{{{*/
// ---------------------------------------------------------------------
/* This will figure out the state of all the packages and all the
   dependencies based on the current policy. */
void pkgDepCache::Update(OpProgress * const Prog)
{
   iUsrSize = 0;
   iDownloadSize = 0;
   iDelCount = 0;
   iInstCount = 0;
   iKeepCount = 0;
   iBrokenCount = 0;
   iBadCount = 0;

   // Perform the depends pass
   int Done = 0;
   for (PkgIterator I = PkgBegin(); I.end() != true; I++,Done++)
   {
      if (Prog != 0 && Done%20 == 0)
	 Prog->Progress(Done);
      for (VerIterator V = I.VersionList(); V.end() != true; V++)
      {
	 unsigned char Group = 0;

	 for (DepIterator D = V.DependsList(); D.end() != true; D++)
	 {
	    // Build the dependency state.
	    unsigned char &State = DepState[D->ID];
	    State = DependencyState(D);

	    // Add to the group if we are within an or..
	    Group |= State;
	    State |= Group << 3;
	    if ((D->CompareOp & Dep::Or) != Dep::Or)
	       Group = 0;

	    // Invert for Conflicts
	    if (D->Type == Dep::Conflicts || D->Type == Dep::Obsoletes)
	       State = ~State;
	 }
      }

      // Compute the pacakge dependency state and size additions
      AddSizes(I);
      UpdateVerState(I);
      AddStates(I);
   }

   if (Prog != 0)
      Prog->Progress(Done);
}
									/*}}}*/
// DepCache::Update - Update the deps list of a package	   		/*{{{*/
// ---------------------------------------------------------------------
/* This is a helper for update that only does the dep portion of the scan.
   It is mainly ment to scan reverse dependencies. */
void pkgDepCache::Update(DepIterator D)
{
   // Update the reverse deps
   for (;D.end() != true; D++)
   {
      unsigned char &State = DepState[D->ID];
      State = DependencyState(D);

      // Invert for Conflicts
      if (D->Type == Dep::Conflicts || D->Type == Dep::Obsoletes)
	 State = ~State;

      RemoveStates(D.ParentPkg());
      BuildGroupOrs(D.ParentVer());
      UpdateVerState(D.ParentPkg());
      AddStates(D.ParentPkg());
   }
}
									/*}}}*/
// DepCache::Update - Update the related deps of a package		/*{{{*/
// ---------------------------------------------------------------------
/* This is called whenever the state of a package changes. It updates
   all cached dependencies related to this package. */
void pkgDepCache::Update(PkgIterator const &Pkg)
{
   // Recompute the dep of the package
   RemoveStates(Pkg);
   UpdateVerState(Pkg);
   AddStates(Pkg);

   // Update the reverse deps
   Update(Pkg.RevDependsList());

   // Update the provides map for the current ver
   if (Pkg->CurrentVer != 0)
      for (PrvIterator P = Pkg.CurrentVer().ProvidesList();
	   P.end() != true; P++)
	 Update(P.ParentPkg().RevDependsList());

   // Update the provides map for the candidate ver
   if (PkgState[Pkg->ID].CandidateVer != 0)
      for (PrvIterator P = PkgState[Pkg->ID].CandidateVerIter(*this).ProvidesList();
	   P.end() != true; P++)
	 Update(P.ParentPkg().RevDependsList());
}

									/*}}}*/

// DepCache::MarkKeep - Put the package in the keep state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkKeep(const PkgIterator &Pkg,bool const Soft)
{
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;

   /* Reject an attempt to keep a non-source broken installed package, those
      must be upgraded */
   if (Pkg.State() == PkgIterator::NeedsUnpack &&
       Pkg.CurrentVer().Downloadable() == false)
      return;

   /* We changed the soft state all the time so the UI is a bit nicer
      to use */
   StateCache &P = PkgState[Pkg->ID];
   if (Soft == true)
      P.iFlags |= AutoKept;
   else
      P.iFlags &= ~AutoKept;

   // Check that it is not already kept
   if (P.Mode == ModeKeep)
      return;

   // We dont even try to keep virtual packages..
   if (Pkg->VersionList == 0)
      return;

   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   P.Mode = ModeKeep;
   if (Pkg->CurrentVer == 0)
      P.InstallVer = 0;
   else
      P.InstallVer = Pkg.CurrentVer().operator pkgCache::Version *();

   MarkAuto(Pkg, getMarkAuto(Pkg));

   AddStates(Pkg);

   Update(Pkg);

   AddSizes(Pkg);
}
									/*}}}*/
// DepCache::MarkDelete - Put the package in the delete state		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::MarkDelete(const PkgIterator &Pkg, bool const rPurge)
{
   // Simplifies other routines.
   if (Pkg.end() == true)
      return;

   // Check that it is not already marked for delete
   StateCache &P = PkgState[Pkg->ID];
   P.iFlags &= ~(AutoKept | Purge);
   if (rPurge == true)
      P.iFlags |= Purge;

   if ((P.Mode == ModeDelete || P.InstallVer == 0) &&
       (Pkg.Purge() == true || rPurge == false))
      return;

   // We dont even try to delete virtual packages..
   if (Pkg->VersionList == 0)
      return;

   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   if (Pkg->CurrentVer == 0 && (Pkg.Purge() == true || rPurge == false))
      P.Mode = ModeKeep;
   else
      P.Mode = ModeDelete;
   P.InstallVer = 0;
   P.Flags &= Flag::Auto;
   MarkAuto(Pkg, getMarkAuto(Pkg));

   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);
}

void pkgDepCache::MarkAuto(const PkgIterator &Pkg,
                           pkgDepCache::AutoMarkFlag const AutoFlag)
{
   StateCache &state = PkgState[Pkg->ID];

   switch (AutoFlag)
   {
   case AutoMarkFlag::Auto:
      state.Flags |= Flag::Auto;
      break;

   case AutoMarkFlag::Manual:
      state.Flags &= ~Flag::Auto;
      break;

   case AutoMarkFlag::DontChange:
   default:
      break;
   }
}

pkgDepCache::AutoMarkFlag pkgDepCache::getMarkAuto(const PkgIterator &Pkg,
                                                   bool const installing_behaves_as_installed,
                                                   pkgDepCache::AutoMarkFlag const value_if_package_not_installed)
   const
{
   if ((!installing_behaves_as_installed) || (PkgState[Pkg->ID].Mode != pkgDepCache::ModeInstall))
   {
      if (Pkg->CurrentState != pkgCache::State::Installed)
      {
         return value_if_package_not_installed;
      }
   }

   return (((PkgState[Pkg->ID].Flags & Flag::Auto) == Flag::Auto) ? AutoMarkFlag::Auto : AutoMarkFlag::Manual);
}

									/*}}}*/

// For Mark*() functions
#define DEBUG_MI(n, fmt, ...) if (DebugStr) \
   fprintf(stderr, "%s:%*s " fmt "\n", DebugStr, Depth*2+n, "", __VA_ARGS__)
#define DEBUG_THIS(fmt, ...) DEBUG_MI(0, fmt, __VA_ARGS__)
#define DEBUG_NEXT(fmt, ...) DEBUG_MI(1, fmt, __VA_ARGS__)
#define DEBUG_NEXT2(fmt, ...) DEBUG_MI(2, fmt, __VA_ARGS__)

// DepCache::MarkInstall - Put the package in the install state		/*{{{*/
// ---------------------------------------------------------------------
/* */
int pkgDepCache::MarkInstall0(PkgIterator const &Pkg,
                              int const Depth, const char * const DebugStr)
{
   DEBUG_THIS("to mark for install (shallow): %s", ToDbgStr(Pkg).c_str());

   if (Pkg.end() == true)
      return -1;

   /* Check that it is not already marked for install and that it can be
      installed */
   StateCache &P = PkgState[Pkg->ID];
   P.iFlags &= ~AutoKept;
   if (P.InstBroken() == false && (P.Mode == ModeInstall ||
	P.CandidateVer == Pkg.CurrentVer().operator const pkgCache::Version *()))
   {
      if (P.CandidateVer == Pkg.CurrentVer().operator const pkgCache::Version *() && P.InstallVer == 0)
	 MarkKeep(Pkg);
      return 0;
   }

   // See if there is even any possible instalation candidate
   if (P.CandidateVer == 0)
      return -1;

   // We dont even try to install virtual packages..
   if (Pkg->VersionList == 0)
      return -1;

   /* Target the candidate version and remove the autoflag. We reset the
      autoflag below if this was called recursively. Otherwise the user
      should have the ability to de-auto a package by changing its state */
   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   MarkAuto(Pkg, getMarkAuto(Pkg));
   P.Mode = ModeInstall;
   P.InstallVer = P.CandidateVer;
   if (P.CandidateVer == Pkg.CurrentVer().operator const pkgCache::Version *())
      P.Mode = ModeKeep;

   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);

   return 1;
}

void pkgDepCache::MarkInstallRec(const PkgIterator &Pkg,
      bool const Restricted, std::set<PkgIterator> &MarkAgain,
      int const Depth, const char * const DebugStr)
{
   DEBUG_THIS("to mark for install (recursively): %s", ToDbgStr(Pkg).c_str());

   if (Depth > 100)
   {
      DEBUG_THIS("Too deep (%d)!", Depth);
      return;
   }
   if (MarkInstall0(Pkg, Depth+1, DebugStr) <= 0)
      return;

   DEBUG_THIS("marked for install (shallow): %s", ToDbgStr(Pkg).c_str());

   bool AddMarkAgain = false;
   for (DepIterator Dep = PkgState[Pkg->ID].InstVerIter(*this).DependsList();
        Dep.end() != true;)
   {
      // Grok or groups
      DepIterator Start = Dep;
      bool Result = true;
      unsigned Ors = 0;
      for (bool LastOR = true; Dep.end() == false && LastOR == true; Dep++,Ors++)
      {
	 LastOR = (Dep->CompareOp & Dep::Or) == Dep::Or;

	 if ((DepState[Dep->ID] & DepInstall) == DepInstall)
	    Result = false;
      }

      // Dep is satisfied okay.
      if (Result == false)
	 continue;

      // CNC:2003-02-17 - IsImportantDep() currently calls IsCritical(), so
      //		  these two are currently doing the same thing. Check
      //		  comments in IsImportantDep() definition.
#if 0
      /* Check if this dep should be consider for install. If it is a user
         defined important dep and we are installed a new package then
	 it will be installed. Otherwise we only worry about critical deps */
      if (IsImportantDep(Start) == false)
	 continue;
      if (Pkg->CurrentVer != 0 && Start.IsCritical() == false)
	 continue;
#else
      if (Start.IsCritical() == false)
	 continue;
#endif

      /* If we are in an or group locate the first or that can
         succeed. We have already cached this.. */
      for (; Ors > 1 && (DepState[Start->ID] & DepCVer) != DepCVer; Ors--)
	 Start++;

      DEBUG_NEXT("satisfying %s", ToDbgStr(Start).c_str());

      /* This bit is for processing the possibilty of an install/upgrade
         fixing the problem */
      if ((DepState[Start->ID] & DepCVer) == DepCVer)
      {
         const SPtrArray<Version *> List(Start.AllTargets());
	 // Right, find the best version to install..
	 Version **Cur = List.get();

	 // See if there are direct matches (at the start of the list)
	 for (; Start.IsTargetDirect(Cur); Cur++)
	 {
	    PkgIterator const TrgPkg = parentPkg(**Cur);
	    if (PkgState[TrgPkg->ID].CandidateVer == *Cur)
               break;
	 }

         Version * TargetCandidateVer = nullptr;
	 if (Start.IsTargetDirect(Cur))
         {
            VerIterator const TrgVer(*Cache,*Cur);
            DEBUG_NEXT2("found a direct target: %s", ToDbgStr(TrgVer).c_str());
            TargetCandidateVer = *Cur;
         }
         else
	 { // Select the highest priority providing package
	    int CanSelect = 0;
	    pkgPrioSortList(*Cache,Cur);
	    for (; *Cur != 0; Cur++)
	    {
	       PkgIterator const TrgPkg = parentPkg(**Cur);
	       if (PkgState[TrgPkg->ID].CandidateVer == *Cur)
               {
                  VerIterator const TrgVer(*Cache,*Cur);
                  if (CanSelect++ == 0)
                  {
                     DEBUG_NEXT2("found a providing target: %s", ToDbgStr(TrgVer).c_str());
                     TargetCandidateVer = *Cur;
                  }
                  else
                  {
                     DEBUG_NEXT2("found another providing target: %s", ToDbgStr(TrgVer).c_str());
                     break;
                  }
               }
	    }
	    // In restricted mode, skip ambiguous dependencies.
	    if (Restricted && CanSelect > 1) {
	       DEBUG_NEXT("target %s", "AMBI");
	       AddMarkAgain = true;
	       continue;
	    }
	 }

	 if (TargetCandidateVer == nullptr)
	 {
            DEBUG_NEXT("target %s", "NONE");
            continue;
	 }

         VerIterator const InstVer(*Cache,TargetCandidateVer);
	 DEBUG_NEXT("target SELECTED: %s", ToDbgStr(InstVer).c_str());
         // Recursion is always restricted
         MarkInstallRec(InstVer.ParentPkg(),/*Restricted*/true,MarkAgain,Depth+1,DebugStr);
      }

      /* For conflicts we just de-install the package and mark as auto,
         Conflicts may not have or groups */
      else if (Start->Type == Dep::Conflicts || Start->Type == Dep::Obsoletes)
      {
         const SPtrArray<Version * const> List(Start.AllTargets());
	 for (Version * const *I = List.get();
              Start->Type == Dep::Obsoletes ? Start.IsTargetDirect(I) : *I != nullptr;
              I++)
	 {
	    VerIterator const TrgVer(*Cache,*I);
            DEBUG_NEXT2("target to delete: %s", ToDbgStr(TrgVer).c_str());
	    PkgIterator const TrgPkg = TrgVer.ParentPkg();
	    MarkDelete(TrgPkg);
	    MarkAuto(TrgPkg, getMarkAuto(TrgPkg));
	 }
      }
   }

   if (AddMarkAgain)
      MarkAgain.insert(Pkg);
}

void pkgDepCache::MarkInstall2(PkgIterator const &Pkg)
{
   bool const Debug = _config->FindB("Debug::pkgMarkInstall", false);
   const char * const DebugA = Debug ? "MI2a" : NULL;
   const char * const DebugB = Debug ? "MI2b" : NULL;
   const char * const DebugC = Debug ? "MI2c" : NULL;
   std::set<PkgIterator> MA;
   MarkInstallRec(Pkg, true, MA, 0, DebugA);
   while (1) {
      std::set<PkgIterator> MAA;
      for (auto I = MA.cbegin(); I != MA.cend(); ++I)
	 MarkInstallRec(*I, /*Restricted*/true, MAA, 0, DebugB);
      for (auto I = MA.cbegin(); I != MA.cend(); ++I)
	 MarkInstallRec(*I, /*Restricted*/false, MAA, 0, DebugC);
      if (MA == MAA)
	 break;
      MA = MAA;
   }
}

void pkgDepCache::MarkInstall(const PkgIterator &Pkg,
                              pkgDepCache::AutoMarkFlag const AutoFlag,
                              bool const AutoInst)
{
   if (AutoInst == false)
      MarkInstall0(Pkg,
                   /* Debugging args */ 0, nullptr);
   else
      MarkInstall2(Pkg);

   if ((*this)[Pkg].Install())
   {
      MarkAuto(Pkg, AutoFlag);
   }
}
									/*}}}*/
// DepCache::SetReInstall - Set the reinstallation flag			/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::SetReInstall(const PkgIterator &Pkg,bool const To)
{
   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   StateCache &P = PkgState[Pkg->ID];
   if (To == true)
   {
      P.iFlags |= ReInstall;

      if (_config->FindB("APT::Get::ReInstall::MarkManual", true))
      {
         MarkAuto(Pkg, pkgDepCache::AutoMarkFlag::Manual);
      }
   }
   else
   {
      P.iFlags &= ~ReInstall;
   }

   AddStates(Pkg);
   AddSizes(Pkg);
}
									/*}}}*/
// DepCache::SetCandidateVersion - Change the candidate version		/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::SetCandidateVersion(VerIterator TargetVer)
{
   pkgCache::PkgIterator Pkg = TargetVer.ParentPkg();
   StateCache &P = PkgState[Pkg->ID];

   RemoveSizes(Pkg);
   RemoveStates(Pkg);

   if (P.CandidateVer == P.InstallVer)
      P.InstallVer = (Version *)TargetVer;
   P.CandidateVer = (Version *)TargetVer;
   P.Update(Pkg,*this);

   AddStates(Pkg);
   Update(Pkg);
   AddSizes(Pkg);
}
									/*}}}*/
// StateCache::Update - Compute the various static display things	/*{{{*/
// ---------------------------------------------------------------------
/* This is called whenever the Candidate version changes. */
void pkgDepCache::StateCache::Update(PkgIterator Pkg,pkgCache &Cache)
{
   // Some info
   VerIterator Ver = CandidateVerIter(Cache);

   // Use a null string or the version string
   if (Ver.end() == true)
      CandVersion = "";
   else
      CandVersion = Ver.VerStr();

   // Find the current version
   CurVersion = "";
   if (Pkg->CurrentVer != 0)
      CurVersion = Pkg.CurrentVer().VerStr();

   // Strip off the epochs for display
   CurVersion = StripEpoch(CurVersion);
   CandVersion = StripEpoch(CandVersion);

   // Figure out if its up or down or equal
   Status = Ver.CompareVer(Pkg.CurrentVer());
   if (Pkg->CurrentVer == 0 || Pkg->VersionList == 0 || CandidateVer == 0)
     Status = 2;
}
									/*}}}*/
// StateCache::StripEpoch - Remove the epoch specifier from the version	/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgDepCache::StateCache::StripEpoch(const char *Ver)
{
   if (Ver == 0)
      return 0;

   // Strip any epoch
   for (const char *I = Ver; *I != 0; I++)
      if (*I == ':')
	 return I + 1;
   return Ver;
}
									/*}}}*/

// Policy::GetCandidateVer - Returns the Candidate install version	/*{{{*/
// ---------------------------------------------------------------------
/* The default just returns the highest available version that is not
   a source and automatic. */
pkgCache::VerIterator pkgDepCache::Policy::GetCandidateVer(PkgIterator Pkg)
{
   /* Not source/not automatic versions cannot be a candidate version
      unless they are already installed */
   VerIterator Last(*(pkgCache *)this,0);

   for (VerIterator I = Pkg.VersionList(); I.end() == false; I++)
   {
      if (Pkg.CurrentVer() == I)
	 return I;

      for (VerFileIterator J = I.FileList(); J.end() == false; J++)
      {
	 if ((J.File()->Flags & Flag::NotSource) != 0)
	    continue;

	 /* Stash the highest version of a not-automatic source, we use it
	    if there is nothing better */
	 if ((J.File()->Flags & Flag::NotAutomatic) != 0)
	 {
	    if (Last.end() == true)
	       Last = I;
	    continue;
	 }

	 return I;
      }
   }

   return Last;
}
									/*}}}*/
// Policy::IsImportantDep - True if the dependency is important		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgDepCache::Policy::IsImportantDep(DepIterator Dep)
{
   // CNC:2002-03-17 - Every place that uses this function seems to
   //		       currently check for IsCritical() as well. Since
   //		       this is a virtual (heavy) function, we'll try
   //		       not to use it while not necessary.
   return Dep.IsCritical();
}
									/*}}}*/

// CNC:2003-02-24
// pkgDepCache::State::* - Routines to work on the state of a DepCache.	/*{{{*/
// ---------------------------------------------------------------------
/* */
void pkgDepCache::State::Copy(pkgDepCache::State const &Other)
{
   memcpy(this, &Other, sizeof(*this));
   int Size = Dep->Head().PackageCount;
   int DepSize = Dep->Head().DependsCount;
   PkgState = new StateCache[Size];
   PkgIgnore = new bool[Size];
   DepState = new unsigned char[DepSize];
   memcpy(PkgState, Other.PkgState, Size*sizeof(*PkgState));
   memcpy(PkgIgnore, Other.PkgIgnore, Size*sizeof(*PkgIgnore));
   memcpy(DepState, Other.DepState, DepSize*sizeof(*DepState));
}

void pkgDepCache::State::Save(pkgDepCache *dep)
{
   Dep = dep;
   delete[] PkgState;
   delete[] DepState;
   delete[] PkgIgnore;
   int Size = Dep->Head().PackageCount;
   int DepSize = Dep->Head().DependsCount;
   PkgState = new StateCache[Size];
   // allocate and zero memory
   PkgIgnore = new bool[Size]();
   DepState = new unsigned char[DepSize];
   memcpy(PkgState, Dep->PkgState, Size*sizeof(*PkgState));
   memcpy(DepState, Dep->DepState, DepSize*sizeof(*DepState));
   iUsrSize = Dep->iUsrSize;
   iDownloadSize= Dep->iDownloadSize;
   iInstCount = Dep->iInstCount;
   iDelCount = Dep->iDelCount;
   iKeepCount = Dep->iKeepCount;
   iBrokenCount = Dep->iBrokenCount;
   iBadCount = Dep->iBadCount;
}

void pkgDepCache::State::Restore()
{
   memcpy(Dep->PkgState, PkgState, Dep->Head().PackageCount*sizeof(*PkgState));
   memcpy(Dep->DepState, DepState, Dep->Head().DependsCount*sizeof(*DepState));
   Dep->iUsrSize = iUsrSize;
   Dep->iDownloadSize= iDownloadSize;
   Dep->iInstCount = iInstCount;
   Dep->iDelCount = iDelCount;
   Dep->iKeepCount = iKeepCount;
   Dep->iBrokenCount = iBrokenCount;
   Dep->iBadCount = iBadCount;
}

bool pkgDepCache::State::Changed()
{
   int Size = Dep->Head().PackageCount;
   StateCache *NewPkgState = Dep->PkgState;
   for (int i = 0; i != Size; i++) {
      if (PkgIgnore[i] == false &&
          ((PkgState[i].Status != NewPkgState[i].Status) ||
          (PkgState[i].Mode != NewPkgState[i].Mode)))
         return true;
   }
   return false;
}

void pkgDepCache::State::UnIgnoreAll()
{
   memset(PkgIgnore, 0, Dep->Head().PackageCount*sizeof(*PkgIgnore));
}

									/*}}}*/

// vim:sts=3:sw=3
