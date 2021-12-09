#ifndef RPMPACKAGEDATA_H
#define RPMPACKAGEDATA_H

#include <apt-pkg/tagfile.h>
#include <apt-pkg/pkgcache.h>

#include <map>
#include <vector>
#include <regex.h>
#include <cstring>

#include <unordered_map>

using std::unordered_map;

using std::map;
using std::string;
using std::vector;

class RPMPackageData
{
   protected:

   unordered_map<string,pkgCache::State::VerPriority> Priorities;
   unordered_map<string,pkgCache::Flag::PkgFlags> Flags;
   unordered_map<string,vector<string>*> FakeProvides;
   unordered_map<string,int> IgnorePackages;
   unordered_map<string,int> DuplicatedPackages;
   unordered_map<string,vector<string> > CompatArch;
   typedef map<string,pkgCache::VerIterator> VerMapValueType;
   typedef unordered_map<unsigned long,VerMapValueType> VerMapType;
   typedef unordered_map<string,int> ArchScoresType;

   vector<regex_t*> HoldPackages;
   vector<regex_t*> DuplicatedPatterns;

   struct Translate {
	   regex_t Pattern;
	   string Template;
   };

   vector<Translate*> BinaryTranslations;
   vector<Translate*> SourceTranslations;
   vector<Translate*> IndexTranslations;

   VerMapType VerMap;

   void GenericTranslate(vector<Translate*> &TList, string &FullURI,
			 map<string,string> &Dict);

   int MinArchScore;

   ArchScoresType ArchScores;
   int RpmArchScore(const string &Arch);

   string BaseArch;
   bool MultilibSys;

   public:

   inline pkgCache::State::VerPriority VerPriority(const string &Package)
   {
      if (Priorities.find(Package) != Priorities.end())
	 return Priorities[Package];
      return pkgCache::State::Optional;
   }
   inline pkgCache::Flag::PkgFlags PkgFlags(const string &Package)
	{return Flags[Package];}

   bool HoldPackage(const char *name);
   bool IgnorePackage(const string &Name)
	{return IgnorePackages.find(Name) != IgnorePackages.end();}

   bool IgnoreDep(pkgVersioningSystem &VS,pkgCache::DepIterator &Dep);

   void TranslateBinary(string &FullURI, map<string,string> &Dict)
	{return GenericTranslate(BinaryTranslations, FullURI, Dict);}
   void TranslateSource(string &FullURI, map<string,string> &Dict)
	{return GenericTranslate(SourceTranslations, FullURI, Dict);}
   void TranslateIndex(string &FullURI, map<string,string> &Dict)
	{return GenericTranslate(IndexTranslations, FullURI, Dict);}

   bool HasBinaryTranslation()
	{return !BinaryTranslations.empty();}
   bool HasSourceTranslation()
	{return !SourceTranslations.empty();}
   bool HasIndexTranslation()
	{return !IndexTranslations.empty();}

   int ArchScore(const string &Arch)
   {
      ArchScoresType::const_iterator I = ArchScores.find(Arch);
      if (I != ArchScores.end())
	 return I->second;
      int Ret = RpmArchScore(Arch);
      ArchScores[Arch] = Ret;
      return Ret;
   }
   void InitMinArchScore();

   bool IsCompatArch(const string &Arch);
   bool IsMultilibSys() { return MultilibSys; }

   void SetDupPackage(const string &Name)
	{DuplicatedPackages[Name] = 1;}
   bool IsDupPackage(const string &Name);

   static RPMPackageData *Singleton();

   void SetVersion(const string &ID, unsigned long Offset,
		   const pkgCache::VerIterator &Version)
   {
      VerMap[Offset][ID] = Version;
   }
   const pkgCache::VerIterator *GetVersion(const string &ID,
					   unsigned long Offset)
   {
       VerMapType::const_iterator I1 = VerMap.find(Offset);
       if (I1 != VerMap.end()) {
	       VerMapValueType::const_iterator I2 =
		       I1->second.find(ID);
	       if (I2 != I1->second.end())
		       return &I2->second;
       }
       return NULL;
   }

   void CacheBuilt() {VerMap.clear();}

   RPMPackageData();
};


#endif

// vim:sts=3:sw=3
