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
#include <apt-pkg/validate.h>
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

   string Files = Section.FindS("SHA1");
   // Lack of checksum is only fatal if authentication is on
   if (Files.empty())
   {
      if (IsAuthenticated())
	 return _error->Error(_("No SHA1 data in Release file '%s'"),
			      Files.c_str());
      else
	 return true;
   }

   // Iterate over the entire list grabbing each triplet
   const char *C = Files.c_str();
   while (*C != 0)
   {
      const auto Hash =
         mbind(NonEmpty,
               ParseQuoteWord_(C));
      // Parse the size and append the directory
      const auto Size =
         Hash // otherwise (if already failed) skip
         ? mbind(from_chars_<decltype(Checksum::Size)>,
                 ParseQuoteWord_(C))
         : std::nullopt;
      const auto Path =
         Size // otherwise (if already failed) skip
         ? mbind(NonEmpty,
                 ParseQuoteWord_(C))
         : std::nullopt;
      if (! Path)
         return _error->Error(_("Error parsing SHA1 hash record on Release file '%s'"),
                              File.c_str());
      IndexChecksums[*Path].Size = *Size;
      IndexChecksums[*Path].MD5 = *Hash;
   }

   return true;
}
									/*}}}*/
// Repository::FindChecksums - Get checksum info for file		/*{{{*/
// ---------------------------------------------------------------------
/* */
bool pkgRepository::FindChecksums(const string URI,
                                  decltype(Checksum::Size) &Size, string &MD5) const
{
   const string Path = string(URI,RootURI.size());
   const auto Found = IndexChecksums.find(Path);
   if (Found == IndexChecksums.end())
      return false;
   Size = Found->second.Size;
   MD5 = Found->second.MD5;
   return true;
}

// vim:sts=3:sw=3
