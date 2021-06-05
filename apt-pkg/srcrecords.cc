// Description								/*{{{*/
// $Id: srcrecords.cc,v 1.2 2003/01/29 18:43:48 niemeyer Exp $
/* ######################################################################

   Source Package Records - Allows access to source package records

   Parses and allows access to the list of source records and searching by
   source name on that list.

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/srcrecords.h>
#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>

#include <apti18n.h>
									/*}}}*/

// SrcRecords::pkgSrcRecords - Constructor				/*{{{*/
// ---------------------------------------------------------------------
/* Open all the source index files */
pkgSrcRecords::pkgSrcRecords(pkgSourceList &List) : Files(0), Current(0)
{
   // allocate and zero memory
   Files = new Parser *[List.end() - List.begin() + 1]();

   unsigned int Count = 0;
   pkgSourceList::const_iterator I = List.begin();
   for (; I != List.end(); I++)
   {
      Files[Count] = (*I)->CreateSrcParser();
      if (_error->PendingError() == true)
	 return;
      if (Files[Count] != 0)
	 Count++;
   }
   Files[Count] = 0;

   // Doesn't work without any source index files
   if (Count == 0)
   {
      _error->Error(_("You must put some 'source' URIs"
		    " in your sources.list"));
      return;
   }

   Restart();
}
									/*}}}*/
// SrcRecords::~pkgSrcRecords - Destructor				/*{{{*/
// ---------------------------------------------------------------------
/* */
pkgSrcRecords::~pkgSrcRecords()
{
   if (Files == 0)
      return;

   // Blow away all the parser objects
   for (unsigned int Count = 0; Files[Count] != 0; Count++)
      delete Files[Count];
   delete [] Files;
}
									/*}}}*/
// SrcRecords::Restart - Restart the search				/*{{{*/
// ---------------------------------------------------------------------
/* Return all of the parsers to their starting position */
bool pkgSrcRecords::Restart()
{
   Current = Files;
   for (Parser **I = Files; *I != 0; I++)
      (*I)->Restart();

   return true;
}
									/*}}}*/
// SrcRecords::Find - Find the first source package with the given name	/*{{{*/
// ---------------------------------------------------------------------
/* This searches on both source package filenames and output binary names and
   returns the first found. A 'cursor' like system is used to allow this
   function to be called multiple times to get successive entries */
pkgSrcRecords::Parser *pkgSrcRecords::Find(const char *Package,bool SrcOnly)
{
   if (*Current == 0)
      return 0;

   while (true)
   {
      // DEBUG:
      //std::cerr << "start loop" << std::endl;
      // Step to the next record, possibly switching files
      while ((*Current)->Step() == false)
      {
	 if (_error->PendingError() == true)
	    return 0;
	 Current++;
	 if (*Current == 0)
	    return 0;
      }

      // IO error somehow
      if (_error->PendingError() == true)
	 return 0;

      // Source name hit
      if ((*Current)->FileName() == Package)
	 return *Current;


      // CNC:2003-11-21
      // Check for a files hit
      vector<pkgSrcRecords::File> Files;
      if ((*Current)->Files(Files) == true) {
         vector<pkgSrcRecords::File>::const_iterator I = Files.begin();
	 for (; I != Files.end(); I++) {
            if (flNotDir(I->Path) == flNotDir(Package))
	       return *Current;
	 }
      }

      if (SrcOnly == true)
	 continue;

      // Check for a binary hit
      const char **I = (*Current)->Binaries();
      for (; I != 0 && *I != 0; I++)
	 if (strcmp(Package,*I) == 0)
	    return *Current;
   }
}
									/*}}}*/
// Parser::BuildDepType - Convert a build dep to a string		/*{{{*/
// ---------------------------------------------------------------------
/* */
const char *pkgSrcRecords::Parser::BuildDepType(unsigned char Type)
{
   const char *fields[] = {"Build-Depends",
                           "Build-Depends-Indep",
			   "Build-Conflicts",
			   "Build-Conflicts-Indep"};
   if (Type < 4)
      return fields[Type];
   else
      return "";
}
									/*}}}*/
