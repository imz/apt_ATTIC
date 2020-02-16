// Description								/*{{{*/// $Id: ftp.h,v 1.1 2002/07/23 17:54:53 niemeyer Exp $
// $Id: ftp.h,v 1.1 2002/07/23 17:54:53 niemeyer Exp $
/* ######################################################################

   FTP Aquire Method - This is the FTP aquire method for APT.

   ##################################################################### */
									/*}}}*/
#ifndef APT_FTP_H
#define APT_FTP_H

#include "connect.h"

class FTPConn
{
   char Buffer[1024*10];
   unsigned long long Len;
   std::unique_ptr<MethodFd> ServerFd;
   int DataFd;
   int DataListenFd;
   URI ServerName;
   bool ForceExtended;
   bool TryPassive;
   bool Debug;

   struct addrinfo *PasvAddr;
   
   // Generic Peer Address
   struct sockaddr_storage PeerAddr;
   socklen_t PeerAddrLen;
   
   // Generic Server Address (us)
   struct sockaddr_storage ServerAddr;
   socklen_t ServerAddrLen;
   
   // Private helper functions
   bool ReadLine(string &Text);      
   bool Login();
   bool CreateDataFd();
   bool Finalize();
   
   public:

   bool Comp(URI Other) {return Other.Host == ServerName.Host && Other.Port == ServerName.Port;};
   
   // Raw connection IO
   bool ReadResp(unsigned int &Ret,string &Text);
   bool WriteMsg(unsigned int &Ret,string &Text,const char *Fmt,...);
   
   // Connection control
   bool Open(pkgAcqMethod *Owner);
   void Close();   
   bool GoPasv();
   bool ExtGoPasv();
   
   // Query
   bool Size(const char *Path,unsigned long long &Size);
   bool ModTime(const char *Path, time_t &Time);
   bool Get(const char *Path,FileFd &To,unsigned long long Resume,
	    Hashes &MD5,bool &Missing);
   
   FTPConn(URI Srv);
   ~FTPConn();
};

class FtpMethod : public pkgAcqMethod
{
   virtual bool Fetch(FetchItem *Itm) override;
   virtual bool Configuration(const string &Message) override;
   
   FTPConn *Server;
   
   static string FailFile;
   static int FailFd;
   static time_t FailTime;
   static void SigTerm(int);
   
   public:
   
   FtpMethod();
};

#endif
