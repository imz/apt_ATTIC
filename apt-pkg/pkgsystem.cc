// Description								/*{{{*/
// $Id: pkgsystem.cc,v 1.1 2002/07/23 17:54:50 niemeyer Exp $
/* ######################################################################

   System - Abstraction for running on different systems.

   Basic general structure..

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
#include <cstring>
									/*}}}*/

pkgSystem *_system = 0;
static pkgSystem *SysList[10];
pkgSystem **pkgSystem::GlobalList = SysList;
unsigned long pkgSystem::GlobalListLen = 0;

// System::pkgSystem - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* Add it to the global list.. */
pkgSystem::pkgSystem()
{
   SysList[GlobalListLen] = this;
   GlobalListLen++;
}
									/*}}}*/
// System::GetSystem - Get the named system				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSystem *pkgSystem::GetSystem(const char *Label)
{
   for (unsigned I = 0; I != GlobalListLen; I++)
      if (strcmp(SysList[I]->Label,Label) == 0)
	 return SysList[I];
   return 0;
}
									/*}}}*/
singleSystemLock::singleSystemLock():
   m_acquired(false)
{
}

bool singleSystemLock::Acquire()
{
   /* This class is designed to remember only a single acquisition of a lock,
      and at the end it can UnLock() only a single time. We can't allow
      a mismatch when Lock() is called more times than UnLock() would be.
   */
   if (!m_acquired)
   {
      // I hope this method is never invoked twice concurrently,
      // so never gets to this point twice at the same moment;
      // otherwise, it would invoke Lock() twice.
      m_acquired = _system->Lock();
   }
   return m_acquired;
}

bool singleSystemLock::Acquired()
{
   return m_acquired;
}

bool singleSystemLock::Drop(bool NoErrors)
{
   if (!m_acquired)
      return NoErrors; // dropping twice is an error (analogously to UnLock())

   // I hope this method is never invoked twice concurrently,
   // so never gets to this point twice at the same moment;
   // otherwise, it would invoke UnLock() twice.
   m_acquired = false;
   return _system->UnLock(NoErrors);
}

singleSystemLock::~singleSystemLock()
{
   Drop(true); // No Errors
}
