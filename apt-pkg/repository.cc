// CNC:2002-07-03

// Description								/*{{{*/
// $Id: repository.cc,v 1.4 2002/07/29 18:13:52 niemeyer Exp $
/* ######################################################################

   Repository abstraction for 1 or more unique URI+Dist

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/repository.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/acquire-item.h>

#include <apti18n.h>

#include <fstream>
									/*}}}*/
using namespace std;

// Repository::ParseRelease - Parse Release file for checksums		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgRepository::ParseRelease(string File)
{
   // Open the stream for reading
   FileFd F(File, FileFd::ReadOnly);
   if (_error->PendingError())
      return _error->Error(_("could not open Release file '%s'"),File.c_str());

   pkgTagFile Tags(&F);
   pkgTagSection Section;

   if (!Tags.Step(Section))
       return false;

   GotRelease = true;

   string Files = Section.FindS("MD5Sum");
   // Lack of checksum is only fatal if authentication is on
   if (Files.empty())
   {
      if (IsAuthenticated())
	 return _error->Error(_("No MD5Sum data in Release file '%s'"),
			      Files.c_str());
      else
	 return true;
   }

   // Iterate over the entire list grabbing each triplet
   const char *C = Files.c_str();
   while (*C != 0)
   {
      string Hash = "";
      string Size = "";
      string Path = "";

      if (ParseQuoteWord(C,Hash) == false || Hash.empty() == true ||
	  ParseQuoteWord(C,Size) == false || atoi(Size.c_str()) < 0 ||
	  ParseQuoteWord(C,Path) == false || Path.empty() == true)
	 return _error->Error(_("Error parsing MD5Sum hash record on Release file '%s'"),
			      File.c_str());

      // Parse the size and append the directory
      IndexChecksums[Path].Size = atoi(Size.c_str());
      IndexChecksums[Path].MD5 = Hash;
   }

   return true;
}
									/*}}}*/
// Repository::FindChecksums - Get checksum info for file		/*{{{*/
// ---------------------------------------------------------------------
/* */
std::optional<Cksum> pkgRepository::FindChecksums(string URI)
{
   string Path = string(URI,RootURI.size());
   if (IndexChecksums.find(Path) == IndexChecksums.end())
      return std::nullopt;
   return Cksum(IndexChecksums[Path].Size,
                GetCheckMethod(),
                IndexChecksums[Path].MD5);
}
									/*}}}*/
// vim:sts=3:sw=3
