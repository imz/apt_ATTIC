
/*
 ######################################################################

 RPM database and hdlist related handling

 ######################################################################
 */

#include <config.h>

#ifdef HAVE_RPM

#define ALT_RPM_API
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <utime.h>
#include <unistd.h>
#include <assert.h>
#include <sstream>
#include <algorithm>

#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/md5.h>
#include <apt-pkg/sha1.h>

#include "rpmhandler.h"
#include "rpmpackagedata.h"
#include "raptheader.h"

#include <apti18n.h>

#include <rpm/rpmts.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlib.h>

bool RPMHandler::HasFile(const char *File) const
{
   if (*File == '\0')
      return false;

   vector<string> Files;
   FileList(Files);
   vector<string>::const_iterator I = std::find(Files.begin(), Files.end(), File);
   return I != Files.end();
}

unsigned int RPMHandler::DepOp(raptDepFlags rpmflags) const
{
   unsigned int Op = 0;
   raptDepFlags flags = (raptDepFlags)(rpmflags & RPMSENSE_SENSEMASK);
   if (flags == RPMSENSE_ANY) {
      Op = pkgCache::Dep::NoOp;
   } else if (flags & RPMSENSE_LESS) {
      if (flags & RPMSENSE_EQUAL)
	 Op = pkgCache::Dep::LessEq;
      else
	 Op = pkgCache::Dep::Less;
   } else if (flags & RPMSENSE_GREATER) {
      if (flags & RPMSENSE_EQUAL)
	 Op = pkgCache::Dep::GreaterEq;
      else
	 Op = pkgCache::Dep::Greater;
   } else if (flags & RPMSENSE_EQUAL) {
      Op = pkgCache::Dep::Equals;
   } else {
      /* can't happen, right? */
      _error->Error(_("Impossible flags %d in %s"), rpmflags, Name().c_str());
   }

   return Op;
}

bool RPMHandler::InternalDep(const char *name, const char *ver, raptDepFlags flag) const
{
   static const char rpmlib_prefix[] = "rpmlib(";
   if (strncmp(name, rpmlib_prefix, sizeof(rpmlib_prefix) - 1) != 0)
      return false;

   /* 4.13.0 (ALT specific) */
   return rpmCheckRpmlibProvides(name, ver, flag);
}

bool RPMHandler::PutDep(const char *name, const char *ver, raptDepFlags flags,
			unsigned int Type, bool checkInternalDep,
			vector<Dependency*> &Deps) const
{
   if (checkInternalDep && InternalDep(name, ver, flags))
      return true;

   if (Type == pkgCache::Dep::Depends) {
      if (flags & RPMSENSE_PREREQ)
         Type = pkgCache::Dep::PreDepends;
      else if (flags & RPMSENSE_MISSINGOK)
         Type = pkgCache::Dep::Suggests;
      else
         Type = pkgCache::Dep::Depends;
   }

   Dependency *Dep = new Dependency;
   Dep->Name = name;
   Dep->Version = ver;
   Dep->Op = DepOp(flags);
   Dep->Type = Type;
   Deps.push_back(Dep);
   return true;
}

// follow-up on LORG:2006-03-16
Cksum RPMHandler::Checksums() const
{
   const string ChkType = ChecksumType();
   if (ChkType == "SHA1-Hash")
      return Cksum(FileSize(), ChkType, SHA1Sum());
   else if (ChkType == "MD5-Hash")
      return Cksum(FileSize(), ChkType, MD5Sum());
   else
      return Cksum(FileSize(), "", ""); // FIXME: fail on unknown type
}

off_t RPMHdrHandler::GetITag(raptTag Tag) const
{
   raptInt val = 0;
   raptHeader h(HeaderP);

   h.getTag(Tag, val);
   return val;
}

string RPMHdrHandler::GetSTag(raptTag Tag) const
{
   string str = "";
   raptHeader h(HeaderP);

   h.getTag(Tag, str);
   return str;
}

string RPMHdrHandler::EVRDB() const
{
   string str;
   raptInt val;
   ostringstream res;
   raptHeader h(HeaderP);

   if (h.getTag(RPMTAG_EPOCH, val))
      res << val << ":";

   res << Version() << '-';

   if (h.getTag(RPMTAG_RELEASE, str))
      res << str;

   if (h.getTag(RPMTAG_DISTTAG, str))
      res << ":" << str;

   if (h.getTag(RPMTAG_BUILDTIME, val))
      res << "@" << val;

   return res.str();
}

string RPMHdrHandler::Maintainer() const
{
   string str;
   raptHeader h(HeaderP);

   if (h.getTag(RPMTAG_PACKAGER, str))
      return str;
   if (h.getTag(RPMTAG_VENDOR, str))
      return str;

   return string("");
}

string RPMHdrHandler::Changelog() const
{
   string str;
   vector<string> names, texts;
   vector<raptInt> times;
   raptHeader h(HeaderP);

   if (h.getTag(RPMTAG_CHANGELOGTIME, times) &&
       h.getTag(RPMTAG_CHANGELOGNAME, names) &&
       h.getTag(RPMTAG_CHANGELOGTEXT, texts)) {

      vector<raptInt>::const_iterator timeI = times.begin();
      vector<string>::const_iterator nameI = names.begin();
      vector<string>::const_iterator textI = texts.begin();
      for (; timeI != times.end() &&
	     nameI != names.end() &&
	     textI != texts.end()
	   ; ++timeI, ++nameI, ++textI) {
	 time_t t = *timeI;
	 tm tmb;
	 char buf[256];
	 if (!localtime_r(&t, &tmb) ||
	     !strftime(buf, sizeof(buf), "%a %b %d %Y", &tmb)) {
	    continue;
	 }
	 str += string("* ") + buf + ' ' + *nameI + '\n' + *textI + '\n';
      }
   }

   return str;
}

bool RPMHdrHandler::FileList(vector<string> &FileList) const
{
   raptHeader h(HeaderP);
   if (!h.getTag(RPMTAG_OLDFILENAMES, FileList))
      h.getTag(RPMTAG_FILENAMES, FileList);
   // it's ok for a package not to have any files
   return true;
}

bool RPMHdrHandler::DepsList(unsigned int Type, vector<Dependency*> &Deps,
			     bool checkInternalDep) const
{
   rpmTag deptype;
   switch (Type) {
      case pkgCache::Dep::Depends:
         deptype = RPMTAG_REQUIRENAME;
         break;
      case pkgCache::Dep::Obsoletes:
         deptype = RPMTAG_OBSOLETENAME;
         break;
      case pkgCache::Dep::Conflicts:
         deptype = RPMTAG_CONFLICTNAME;
         break;
      case pkgCache::Dep::Provides:
         deptype = RPMTAG_PROVIDENAME;
         break;
      case pkgCache::Dep::Suggests:
         deptype = RPMTAG_SUGGESTNAME;
         break;
      default:
         /* can't happen... right? */
         return false;
   }
   rpmds ds = rpmdsNew(HeaderP, deptype, 0);
   if (ds) {
      while (rpmdsNext(ds) >= 0)
         PutDep(rpmdsN(ds), rpmdsEVR(ds), rpmdsFlags(ds), Type,
		checkInternalDep, Deps);
      rpmdsFree(ds);
   }
   return true;
}

RPMFileHandler::RPMFileHandler(string File)
{
   ID = File;
   FD = Fopen(File.c_str(), "r");
   if (FD == NULL)
   {
      /*
      _error->Error(_("could not open RPM package list file %s: %s"),
		    File.c_str(), rpmErrorString());
      */
      return;
   }
   iSize = fdSize(FD);
}

RPMFileHandler::RPMFileHandler(FileFd *File)
{
   FD = fdDup(File->Fd());
   if (FD == NULL)
   {
      /*
      _error->Error(_("could not create RPM file descriptor: %s"),
		    rpmErrorString());
      */
      return;
   }
   iSize = fdSize(FD);
}

RPMFileHandler::~RPMFileHandler()
{
   if (HeaderP != NULL)
      headerFree(HeaderP);
   if (FD != NULL)
      Fclose(FD);
}

bool RPMFileHandler::Skip()
{
   if (FD == NULL)
      return false;
   iOffset = lseek(Fileno(FD),0,SEEK_CUR);
   if (HeaderP != NULL)
       headerFree(HeaderP);
   HeaderP = headerRead(FD, HEADER_MAGIC_YES);
   return (HeaderP != NULL);
}

bool RPMFileHandler::Jump(off_t Offset)
{
   if (FD == NULL)
      return false;
   if (lseek(Fileno(FD),Offset,SEEK_SET) != Offset)
      return false;
   return Skip();
}

void RPMFileHandler::Rewind()
{
   if (FD == NULL)
      return;
   iOffset = lseek(Fileno(FD),0,SEEK_SET);
   if (iOffset != 0)
      _error->Error(_("could not rewind RPMFileHandler"));
}

bool RPMSingleFileHandler::Skip()
{
   if (FD == NULL)
      return false;
   if (HeaderP != NULL) {
      headerFree(HeaderP);
      HeaderP = NULL;
      return false;
   }
   rpmts TS = rpmtsCreate();
   rpmtsSetVSFlags(TS, (rpmVSFlags_e)-1);
   int rc = rpmReadPackageFile(TS, FD, sFilePath.c_str(), &HeaderP);
   if (rc != RPMRC_OK && rc != RPMRC_NOTTRUSTED && rc != RPMRC_NOKEY) {
      _error->Error(_("Failed reading file %s"), sFilePath.c_str());
      HeaderP = NULL;
   }
   rpmtsFree(TS);
   return (HeaderP != NULL);
}

bool RPMSingleFileHandler::Jump(off_t Offset)
{
   assert(Offset == 0);
   Rewind();
   return RPMFileHandler::Jump(Offset);
}

void RPMSingleFileHandler::Rewind()
{
   if (FD == NULL)
      return;
   if (HeaderP != NULL) {
      HeaderP = NULL;
      headerFree(HeaderP);
   }
   lseek(Fileno(FD),0,SEEK_SET);
}

off_t RPMSingleFileHandler::FileSize() const
{
   struct stat S;
   if (stat(sFilePath.c_str(),&S) != 0)
      return 0;
   return S.st_size;
}

string RPMSingleFileHandler::MD5Sum() const
{
   MD5Summation MD5;
   FileFd File(sFilePath, FileFd::ReadOnly);
   MD5.AddFD(File.Fd(), File.Size());
   File.Close();
   return MD5.Result().Value();
}

string RPMSingleFileHandler::SHA1Sum() const
{
   SHA1Summation SHA1;
   FileFd File(sFilePath, FileFd::ReadOnly);
   SHA1.AddFD(File.Fd(), File.Size());
   File.Close();
   return SHA1.Result().Value();
}

RPMDirHandler::RPMDirHandler(string DirName)
   : sDirName(DirName)
{
   ID = DirName;
   TS = NULL;
   Dir = opendir(sDirName.c_str());
   if (Dir == NULL)
      return;
   iSize = 0;
   while (nextFileName() != NULL)
      iSize += 1;
   rewinddir(Dir);
   TS = rpmtsCreate();
   rpmtsSetVSFlags(TS, (rpmVSFlags_e)-1);
}

const char *RPMDirHandler::nextFileName()
{
   for (struct dirent *Ent = readdir(Dir); Ent != 0; Ent = readdir(Dir))
   {
      const char *name = Ent->d_name;

      if (name[0] == '.')
	 continue;

      if (flExtension(name) != "rpm")
	 continue;

      // Make sure it is a file and not something else
      sFilePath = flCombine(sDirName,name);
      struct stat St;
      if (stat(sFilePath.c_str(),&St) != 0 || S_ISREG(St.st_mode) == 0)
	 continue;

      sFileName = name;

      return name;
   }
   return NULL;
}

RPMDirHandler::~RPMDirHandler()
{
   if (HeaderP != NULL)
      headerFree(HeaderP);
   if (TS != NULL)
      rpmtsFree(TS);
   if (Dir != NULL)
      closedir(Dir);
}

bool RPMDirHandler::Skip()
{
   if (Dir == NULL)
      return false;
   if (HeaderP != NULL) {
      headerFree(HeaderP);
      HeaderP = NULL;
   }
   const char *fname = nextFileName();
   bool Res = false;
   for (; fname != NULL; fname = nextFileName()) {
      iOffset++;
      if (fname == NULL)
	 break;
      FD_t FD = Fopen(sFilePath.c_str(), "r");
      if (FD == NULL)
	 continue;
      int rc = rpmReadPackageFile(TS, FD, fname, &HeaderP);
      Fclose(FD);
      if (rc != RPMRC_OK
	  && rc != RPMRC_NOTTRUSTED
	  && rc != RPMRC_NOKEY)
	 continue;
      Res = true;
      break;
   }
   return Res;
}

bool RPMDirHandler::Jump(off_t Offset)
{
   if (Dir == NULL)
      return false;
   rewinddir(Dir);
   iOffset = 0;
   while (1) {
      if (iOffset+1 == Offset)
	 return Skip();
      if (nextFileName() == NULL)
	 break;
      iOffset++;
   }
   return false;
}

void RPMDirHandler::Rewind()
{
   rewinddir(Dir);
   iOffset = 0;
}

off_t RPMDirHandler::FileSize() const
{
   if (Dir == NULL)
      return 0;
   struct stat St;
   if (stat(sFilePath.c_str(),&St) != 0) {
      _error->Errno("stat",_("Unable to determine the file size"));
      return 0;
   }
   return St.st_size;
}

string RPMDirHandler::MD5Sum() const
{
   if (Dir == NULL)
      return "";
   MD5Summation MD5;
   FileFd File(sFilePath, FileFd::ReadOnly);
   MD5.AddFD(File.Fd(), File.Size());
   File.Close();
   return MD5.Result().Value();
}

string RPMDirHandler::SHA1Sum() const
{
   if (Dir == NULL)
      return "";
   SHA1Summation SHA1;
   FileFd File(sFilePath, FileFd::ReadOnly);
   SHA1.AddFD(File.Fd(), File.Size());
   File.Close();
   return SHA1.Result().Value();
}


RPMDBHandler::RPMDBHandler(bool WriteLock)
   : Handler(0), WriteLock(WriteLock)
{
   RpmIter = NULL;
   string Dir = _config->Find("RPM::RootDir");
   string DBDir = _config->Find("RPM::DBPath");

   rpmReadConfigFiles(NULL, NULL);
   ID = DataPath(false);

   RPMPackageData::Singleton()->InitMinArchScore();

   // Everytime we open a database for writing, it has its
   // mtime changed, and kills our cache validity. As we never
   // change any information in the database directly, we will
   // restore the mtime and save our cache.
   struct stat St;
   stat(DataPath(false).c_str(), &St);
   DbFileMtime = St.st_mtim.tv_sec;
   DbFileMnanotime = St.st_mtim.tv_nsec;

   Handler = rpmtsCreate();
   rpmtsSetVSFlags(Handler, (rpmVSFlags_e)-1);
   rpmtsSetRootDir(Handler, Dir.c_str());

   if (!DBDir.empty())
   {
      string dbpath_macro = string("_dbpath ") + DBDir;
      if (rpmDefineMacro(NULL, dbpath_macro.c_str(), 0) != 0)
      {
         _error->Error(_("Failed to set rpm dbpath"));
         return;
      }
   }

   if (rpmtsOpenDB(Handler, O_RDONLY) != 0)
   {
      _error->Error(_("could not open RPM database"));
      return;
   }
   RpmIter = raptInitIterator(Handler, RPMDBI_PACKAGES, NULL, 0);
   if (RpmIter == NULL) {
      _error->Error(_("could not create RPM database iterator"));
      return;
   }
   // iSize = rpmdbGetIteratorCount(RpmIter);
   // This doesn't seem to work right now. Code in rpm (4.0.4, at least)
   // returns a 0 from rpmdbGetIteratorCount() if raptInitIterator() is
   // called with RPMDBI_PACKAGES or with keyp == NULL. The algorithm
   // below will be used until there's support for it.
   iSize = 0;
   rpmdbMatchIterator countIt;
   countIt = raptInitIterator(Handler, RPMDBI_PACKAGES, NULL, 0);
   while (rpmdbNextIterator(countIt) != NULL)
      iSize++;
   rpmdbFreeIterator(countIt);

   // Restore just after opening the database, and just after closing.
   if (WriteLock) {
      struct utimbuf Ut;
      Ut.actime = DbFileMtime;
      Ut.modtime = DbFileMtime;
      utime(DataPath(false).c_str(), &Ut);
   }
}

RPMDBHandler::~RPMDBHandler()
{
   if (RpmIter != NULL)
      rpmdbFreeIterator(RpmIter);

   if (Handler != NULL) {
      rpmtsFree(Handler);
   }

   // Restore just after opening the database, and just after closing.
   if (WriteLock) {
      struct utimbuf Ut;
      Ut.actime = DbFileMtime;
      Ut.modtime = DbFileMtime;
      utime(DataPath(false).c_str(), &Ut);
   }
}

string RPMDBHandler::DataPath(bool DirectoryOnly)
{
   string File = "Packages";
   string DBPath = _config->Find("RPM::DBPath");
   if (DBPath.empty()) {
      char *tmp = (char *) rpmExpand("%{_dbpath}", NULL);
      DBPath = _config->Find("RPM::RootDir") + tmp;
      free(tmp);
   }

   if (DirectoryOnly == true)
       return DBPath;
   else
       return DBPath+"/"+File;
}

bool RPMDBHandler::Skip()
{
   if (RpmIter == NULL)
       return false;
   HeaderP = rpmdbNextIterator(RpmIter);
   iOffset = rpmdbGetIteratorOffset(RpmIter);
   if (HeaderP == NULL)
      return false;
   return true;
}

bool RPMDBHandler::Jump(off_t Offset)
{
   iOffset = Offset;
   if (RpmIter == NULL)
      return false;
   rpmdbFreeIterator(RpmIter);
   if (iOffset == 0)
      RpmIter = raptInitIterator(Handler, RPMDBI_PACKAGES, NULL, 0);
   else {
      // rpmdb indexes are hardcoded uint32_t, the size must match here
      raptDbOffset rpmOffset = iOffset;
      RpmIter = raptInitIterator(Handler, RPMDBI_PACKAGES,
				 &rpmOffset, sizeof(rpmOffset));
      iOffset = rpmOffset;
   }
   HeaderP = rpmdbNextIterator(RpmIter);
   return true;
}

bool RPMDBHandler::JumpByName(const string &PkgName, bool Provides)
{
   raptTag tag = Provides ? (raptTag) RPMTAG_PROVIDES : (raptTag) RPMDBI_LABEL;
   if (RpmIter == NULL) return false;
   rpmdbFreeIterator(RpmIter);
   RpmIter = raptInitIterator(Handler, tag, PkgName.c_str(), 0);
   HeaderP = rpmdbNextIterator(RpmIter);
   return HeaderP != NULL;
}

void RPMDBHandler::Rewind()
{
   if (RpmIter == NULL)
      return;
   rpmdbFreeIterator(RpmIter);
   RpmIter = raptInitIterator(Handler, RPMDBI_PACKAGES, NULL, 0);
   iOffset = 0;
}
#endif

// vim:sts=3:sw=3
