// Description								/*{{{*/// $Id: rsh.h,v 1.2 2003/01/29 18:43:48 niemeyer Exp $
// $Id: rsh.h,v 1.2 2003/01/29 18:43:48 niemeyer Exp $
/* ######################################################################

   RSH method - Transfer files via rsh compatible program

   ##################################################################### */
									/*}}}*/
#ifndef APT_RSH_H
#define APT_RSH_H

#include <string>
#include <apt-pkg/strutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/fileutl.h>

class RSHConn
{
   char Buffer[1024*10];
   unsigned long Len;
   int WriteFd;
   int ReadFd;
   URI ServerName;

   // Private helper functions
   bool ReadLine(string &Text);

   public:

   int Process;

   // Raw connection IO
   bool WriteMsg(string &Text,bool Sync,const char *Fmt,...);
   bool Connect(string Host, string User);
   bool Comp(URI Other) {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};

   // Connection control
   bool Open();
   void Close();

   // Query
   bool Size(const char *Path,unsigned long &Size);
   bool ModTime(const char *Path, time_t &Time);
   bool Get(const char *Path,FileFd &To,unsigned long Resume,
            Hashes &Hash,bool &Missing, unsigned long Size);

   RSHConn(URI Srv);
   ~RSHConn();
};

class RSHMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm) override;
   virtual bool Configuration(string Message) override;

   RSHConn *Server;

   static string FailFile;
   static int FailFd;
   static time_t FailTime;
   static void SigTerm(int);

   public:

   RSHMethod();
};

#endif
