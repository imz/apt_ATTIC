// Description								/*{{{*/
// $Id: acquire-item.h,v 1.26 2003/02/02 03:13:13 doogie Exp $
/* ######################################################################

   Acquire Item - Item to acquire

   When an item is instantiated it will add it self to the local list in
   the Owner Acquire class. Derived classes will then call QueueURI to
   register all the URI's they wish to fetch at the initial moment.

   Two item classes are provided to provide functionality for downloading
   of Index files and downloading of Packages.

   A Archive class is provided for downloading .deb files. It does Md5
   checking and source location as well as a retry algorithm.

   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_ITEM_H
#define PKGLIB_ACQUIRE_ITEM_H

#include <apt-pkg/acquire.h>
#include <apt-pkg/indexfile.h>
#include <apt-pkg/pkgrecords.h>

// Item to acquire
class pkgAcquire::Item
{
   protected:

   // Some private helper methods for registering URIs
   pkgAcquire *Owner;
   inline void QueueURI(ItemDesc &Item)
                 {Owner->Enqueue(Item);}
   inline void Dequeue() {Owner->Dequeue(this);}

   // Safe rename function with timestamp preservation
   void Rename(const string &From,const string &To);

   public:

   // State of the item
   /* CNC:2002-11-22
    * Do not use anonyomus enums, as this breaks SWIG in some cases */
   enum StatusFlags {StatIdle, StatFetching, StatDone, StatError} Status;
   string ErrorText;

   /** \brief The size of the object to fetch. */
   unsigned long long FileSize;

   unsigned long PartialSize;
   const char *Mode;
   unsigned long ID;
   bool Complete;
   bool Local;

   // Number of queues we are inserted into
   unsigned int QueueCounter;

   // File to write the fetch into
   string DestFile;
   // Alternative temporary destination
   // Used if method (e.g. rsync) can't use directly DestFile
   string TmpFile;

   // Action members invoked by the worker
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf);
   virtual void Done(string Message,unsigned long Size,string /* Md5Hash unused in this class and almost obsolete */,
		     pkgAcquire::MethodConfig *Cnf);
   virtual void Start(string Message,unsigned long Size);
   virtual string Custom600Headers() {return string();}
   virtual string DescURI() = 0;
   virtual void Finished() {}

   // Inquire functions
   virtual string CheckType() const = 0;
   virtual string ExpectedHash() const = 0;
   pkgAcquire *GetOwner() {return Owner;}

   Item(pkgAcquire *Owner);
   virtual ~Item();
};

// CNC:2002-07-03
class pkgRepository;

// Item class for index files
class pkgAcqIndex : public pkgAcquire::Item
{
   protected:

   bool Decompression;
   bool Erase;
   pkgAcquire::ItemDesc Desc;
   string RealURI;

   // CNC:2002-07-03
   const pkgRepository *Repository;

   /* Not implemented; therefore hidden as protected
      to prohibit meaningless direct calls on objects of this type.
   */
   virtual string CheckType() const override {return std::string();}
   virtual string ExpectedHash() const override {return std::string();}

   public:

   // Specialized action members
   virtual void Done(string Message,unsigned long Size,string /* Md5Hash obsolete */,
		     pkgAcquire::MethodConfig *Cnf) override;
   virtual string Custom600Headers() override;
   virtual string DescURI() override {return RealURI;} // CNC:2003-02-14

   // CNC:2002-07-03
   pkgAcqIndex(pkgAcquire *Owner,const pkgRepository *Repository,const string &URI,
	       const string &URIDesc,const string &ShortDesct);
};

// Item class for index files
class pkgAcqIndexRel : public pkgAcquire::Item
{
   protected:

   pkgAcquire::ItemDesc Desc;
   string RealURI;

   // CNC:2002-07-03
   bool Authentication;
   bool Master;
   bool Erase;
   pkgRepository *Repository;

   /* Not implemented; therefore hidden as protected
      to prohibit meaningless direct calls on objects of this type.
   */
   virtual string CheckType() const override {return std::string();}
   virtual string ExpectedHash() const override {return std::string();}

   public:

   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf) override;
   virtual void Done(string Message,unsigned long Size,string /* Md5Hash obsolete */,
		     pkgAcquire::MethodConfig *Cnf) override;
   virtual string Custom600Headers() override;
   virtual string DescURI() override {return RealURI;}

   // CNC:2002-07-03
   pkgAcqIndexRel(pkgAcquire *Owner,pkgRepository *Repository,const string &URI,
		  const string &URIDesc, const string &ShortDesc,bool Master=false);
};

// Item class for archive files
class pkgAcqArchive : public pkgAcquire::Item
{
   protected:

   // State information for the retry mechanism
   pkgCache::VerIterator Version;
   pkgAcquire::ItemDesc Desc;
   const pkgSourceList *Sources;
   pkgRecords *Recs;
   string ExpectHash;
   string ChkType;
   string &StoreFilename;
   pkgCache::VerFileIterator Vf;
   unsigned int Retries;

   // Queue the next available file for download.
   bool QueueNext();

   public:

   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf) override;
   virtual void Done(string Message,unsigned long Size,string /* Md5Hash obsolete */,
		     pkgAcquire::MethodConfig *Cnf) override;
   virtual string CheckType() const override {return ChkType;}
   virtual string ExpectedHash() const override {return ExpectHash;}
   virtual string DescURI() override {return Desc.URI;}
   virtual void Finished() override;

   pkgAcqArchive(pkgAcquire *Owner,const pkgSourceList *Sources,
		 pkgRecords *Recs,pkgCache::VerIterator const &Version,
		 string &StoreFilename);
};

// Fetch a generic file to the current directory
class pkgAcqFile : public pkgAcquire::Item
{
   pkgAcquire::ItemDesc Desc;
   string ExpectMd5Hash;
   unsigned int Retries;

   public:

   // Specialized action members
   virtual void Failed(string Message,pkgAcquire::MethodConfig *Cnf) override;
   virtual void Done(string Message,unsigned long Size,string MD5,
		     pkgAcquire::MethodConfig *Cnf) override;
   virtual string CheckType() const override {return "MD5-Hash";}
   virtual string ExpectedHash() const override {return ExpectMd5Hash;}
   virtual string DescURI() override {return Desc.URI;}

   pkgAcqFile(pkgAcquire *Owner,const string &URI, const string &MD5,unsigned long Size,
		  const string &Desc, const string &ShortDesc);
};

#endif
