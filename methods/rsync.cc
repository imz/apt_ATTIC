// Description								/*{{{*/
// $Id$
/* ######################################################################

RSYNC Aquire Method - This is the RSYNC aquire method for APT.

##################################################################### */
/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/error.h>
#include <apt-pkg/hashes.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/wait.h>
#include <iostream>

// Internet stuff
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "apti18n.h"
#include "rfc2553emu.h"
#include "connect.h"
#include "rsync-method.h"
/*}}}*/

RsyncMethod::RsyncConn *RsyncMethod::server = 0;
RsyncMethod::ConnType RsyncMethod::connType = RsyncMethod::ConnTypeExec;
bool RsyncMethod::Debug = false;
unsigned int RsyncMethod::Timeout = 0;

/* Argv implementation */
Argv::Argv(int msize): max_size(msize), size(0)
{
   // allocate and zero memory
   args = new char*[max_size]();
}

Argv::~Argv()
{
   for (int i=0; i<size; i++)
	  delete args[i];
   delete [] args;
}

bool Argv::add(const char *arg)
{
   if (arg==0)
	  return false;
   if ( size+1 >= max_size && !resize() ) {
	  cerr << "Failed to resize" << endl;
	  return false;
   }
   int len = strlen(arg);
   args[size] = new char[len+1];
   strncpy(args[size], arg, len+1);
   ++size;
   return true;
}

bool Argv::resize()
{
   static const int increment = 5;
   char **new_args = new char *[max_size+increment];
   memcpy(new_args,args,size*sizeof(char*));
   memset(new_args+size,0, (max_size+increment-size) * sizeof(char*));
   args = new_args;
   max_size += increment;
   return true;
}

Argv::operator string()
{
   string res;
   for (char **p=args; *p; p++)
	  res += *p, res += " ";
   return res;
}

/* RsyncConn implementation */

/** Static buffer for RSYNC_PROXY variable */
char RsyncMethod::RsyncConn::proxy_value[1024];

bool RsyncMethod::RsyncConn::initProxy()
{
   if ( proxy.empty() )
	  return true;
   if ( proxy == "none" ) {
	  unsetenv("RSYNC_PROXY");
	  return true;
   }
   bool res = true;
   string var("RSYNC_PROXY=");
   var += proxy;
   if ( var.size() < sizeof(proxy_value) ) {
	  strcpy(proxy_value, var.c_str());
	  if ( putenv(proxy_value)!=0 ) {
		 res = false;
		 _error->Error("Failed to set %s", proxy_value);
	  }
   } else {
	  res = false;
	  _error->Error("Failed to set RSYNC_PROXY: not enough space in buffer");
   }
   return res;
}

/* RsyncConnExec implementation */

RsyncMethod::RsyncConnExec::RsyncConnExec(URI u, const string &_proxy, const string &prog)
   : RsyncConn(u,_proxy), ChildPid(-1), ChildFd(-1)
{
   program = prog.empty() ? RSYNC_PROGRAM : prog;
}

RsyncMethod::RsyncConnExec::~RsyncConnExec()
{
   if ( ChildPid>0 ) {
	  kill(ChildPid, SIGTERM);
	  waitpid(ChildPid, 0, 0);
	  ChildPid = -1;
   }
   if ( ChildFd>=0 )
	  close(ChildFd);
}

bool RsyncMethod::RsyncConnExec::WaitChild(pkgAcqMethod *Owner, FetchResult &FRes, const char *To)
{
   static const int buflen = 1024;
   static char buf[buflen+1];
   int saved = 0;
   int status = 0, res = 0;
   fd_set readfd;
   struct timeval tv;
   if ( RsyncMethod::Debug )
	  cerr << "RSYNC: WaitChild: fd=" << ChildFd << endl;

   while (1) {
	  FD_ZERO(&readfd);
	  FD_SET(ChildFd,&readfd);
	  FD_SET(0,&readfd);

	  tv.tv_sec = 1;
	  tv.tv_usec = 0;
	  res = select(ChildFd+1, &readfd, 0, 0, &tv);

	  if (res>0) {
		 if ( FD_ISSET(ChildFd,&readfd) ) {
			int len = read(ChildFd,buf+saved, buflen-saved);
			if (len>0) {
			   // Split buffer into single-line strings
			   // and pass each string to ParseOutput.
			   // Strings, that are not terminated with '\n' will
			   // be stored in buffer for later completion.
			   buf[saved+len] = 0;
			   int start = 0;
			   for (int off=saved; off<len; off++) {
			      if ( buf[off]=='\n' ) {
					 buf[off] = 0;
					 ParseOutput(Owner,FRes,buf+start);
					 start = off+1;
			      }
			   }
			   saved = saved+len-start;
			   if ( saved==buflen ) {
			      // Parse process output even it was not terminated with '\n'
			      // in case of full buffer (we can't read anything if there is
			      // no free space in buffer).
			      ParseOutput(Owner,FRes,buf);
			      saved = 0;
			   } else if ( saved>0 ) {
			      if ( RsyncMethod::Debug )
					 cerr << "RSYNC: Saved " << saved << " byted in buffer:"
						  << endl << start << endl;
			      // Move saved data to the beginning of the buffer
			      // including trailing zero
			      memmove(buf,buf+start,saved+1);
			   }
			}
		 }
	  }
	  res = waitpid(ChildPid, &status, WNOHANG);
	  if ((res>0 && WIFEXITED(status)) || res<0) {
		 ChildPid = -1;
		 if ( RsyncMethod::Debug )
			cerr << endl << "RSYNC: Closing ChildFd: " << ChildFd << endl;
		 close(ChildFd);
		 ChildFd = -1;
		 // Parse end of process output if it was not terminated with '\n'
		 if (saved>0)
			ParseOutput(Owner,FRes,buf);
		 if (res < 0) {
		    if ( RsyncMethod::Debug )
		       cerr << endl << "RSYNC: Unknown status of child process " << ChildFd << endl;
		    if (State == Done)
		       return true;
		    return false;
		 }
		 switch (WEXITSTATUS(status)) {
			case 0:
			   return true;
			   break;
			default:
			   if ( State != Failed ) {
				  State = Failed;
				  _error->Error("rsync process terminated with exit code %d", WEXITSTATUS(status));
			   }
			   return false;
			   break;
		 }
	  }
   }
   return false;
}

/* Parse rysnc output
 * Need to parse lines like:
 *
 * ^opening tcp connection to rsync.altlinux.ru port 873
 * ^[11948] i=0 <NULL> FILENAME mode=0100664 len=171935
 * ^FILENAME
 * ^renaming .FILENAME.bWM2bW to FILENAME
 * ^set modtime of FILENAME to (1048860997) Fri Mar 28 17:16:37 2003
 * ^recv_files finished
 *
 */
void RsyncMethod::RsyncConnExec::ParseOutput(pkgAcqMethod *Owner, FetchResult &FRes, const char *buf)
{
   static const char * TMPFN = "Tmp-Filename: ";
   static const char * SIZE  = "Size: ";
   static const char * START = "Start: ";
   static const char * DONE  = "recv_files finished";
   static const char * FAILED= "Failed: ";
   const char * ptr;

   if ( RsyncMethod::Debug )
     cerr << "ParseOutput: " << buf << endl;

   ptr = strstr(buf,"opening tcp connection");
   if (ptr) {
	 if ( RsyncMethod::Debug )
	   cerr << endl << "Status: Connecting" << endl;
	 State = Connecting;
	 Owner->Status(_("Connecting"));
	 return;
   }

   // [PID] i=0 DIRNAME FILENAME mode=XXXXXXX len=YYYYYY
   if (buf[0]=='[') {
	 ptr = strstr(buf,"i=0 ");
	 if (ptr == 0) return;
	 ptr += 4; // skip "i=0 "
	 ptr = strchr(ptr,' '); // skip DIRNAME
   }

   ptr = strstr(buf,TMPFN);
   if (ptr) {
	  ptr += strlen(TMPFN);
	  const char *ptr2 = ptr;
	  while (*ptr2!=0 && !isspace(*ptr2))
		 ++ptr2;
	  if (ptr!=ptr2) {
		 char *tmpfn = new char[ptr2-ptr+1];
		 bzero(tmpfn, ptr2-ptr+1);
		 strncpy(tmpfn, ptr, ptr2-ptr);
		 if (RsyncMethod::Debug)
			cerr << endl << "RSYNC: " << TMPFN << tmpfn << endl;
		 FRes.TmpFilename = string(tmpfn);
		 delete tmpfn;
	  }
   }

   ptr = strstr(buf,SIZE);
   if (ptr) {
	  ptr += strlen(SIZE);
	  unsigned long size = atol(ptr);
	  if (RsyncMethod::Debug)
		 cerr << "RSYNC: " << SIZE << size << endl;
	  FRes.Size = size;
   }

   ptr = strstr(buf,START);
   if (ptr) {
	  State = Fetching;
	  dynamic_cast<RsyncMethod*>(Owner)->Start(FRes);
   }

   ptr = strstr(buf,DONE);
   if (ptr)
	  State = Done;

   ptr = strstr(buf,FAILED);
   if (ptr) {
	  State = Failed;
	  ptr += strlen(FAILED);
	  const char *ptr2 = ptr;
	  while (*ptr2!=0 && *ptr2!='\n')
		 ++ptr2;
	  if (ptr!=ptr2) {
		 char *tmp = new char[ptr2-ptr+1];
		 bzero(tmp, ptr2-ptr+1);
		 strncpy(tmp, ptr, ptr2-ptr);
		 _error->Error("%s",tmp);
		 if (RsyncMethod::Debug)
			cerr << endl << FAILED << tmp << endl;
		 delete tmp;
	  } else {
		 _error->Error("Child process failed (no description)");
	  }
   }
}

bool RsyncMethod::RsyncConnExec::Get(pkgAcqMethod *Owner, FetchResult &FRes, const char *From, const char *To)
{
   int p[2];
   int res = 0;
   Argv argv(10);

   State = Starting;
   if ( RsyncMethod::Debug )
	  cerr << "RSYNC: Get: " << From << endl;

   argv.add(program.c_str());
   argv.add("-Lpt");
   argv.add("--partial");
   AddOptions(argv);
   if (RsyncMethod::Timeout>0) {
	  argv.add("--timeout");
	  char S[10];
	  sprintf(S,"%u",RsyncMethod::Timeout);
	  argv.add(S);
   }
   // Add optional user-defined options to command line
   Configuration::Item const *Itm = _config->Tree("Acquire::rsync::options");
   if (Itm != 0 && Itm->Child != 0) {
	  Itm = Itm->Child;
	  while (Itm != 0) {
		 if (Itm->Value.empty() == false)
			argv.add(Itm->Value.c_str());
		 Itm = Itm->Next;
	  }
   }

   char port[12];
   if (srv.Port!=0)
	  snprintf(port, sizeof(port), ":%u", srv.Port);
   else port[0] = 0;
   argv.add( "rsync://" + srv.Host + port + From);
   argv.add(To);

   if ( pipe(p) ) {
	  _error->Error("RSYNC: RsyncConnExec: Can't create pipe");
	  return false;
   }
   if ( RsyncMethod::Debug )
	  cerr << "RSYNC: Created pipe [" << p[0] << ',' << p[1] << ']' << endl;
   if ( RsyncMethod::Debug )
	  cerr << "RSYNC: Starting: " << string(argv) << endl;

   switch ( ChildPid = fork() ) {
	  case -1:
		 _error->Error("RsyncConnExec: Can't fork");
		 return false;
		 break;
	  case 0:
		 // Child process
		 initProxy();
		 //if ( RsyncMethod::Debug )
		 //   cerr << endl << "RSYNC_PROXY(" << srv.Host << "): " << getenv("RSYNC_PROXY") << endl;
		 close(p[0]);
		 res = dup2(p[1], STDOUT_FILENO);
		 if (res==-1) {
			cout << "Failed: " << "Can't dup2(p[1], STDOUT_FILENO)" << endl;
			exit(100);
		 }
		 res = dup2(p[1], STDERR_FILENO);
		 if (res==-1) {
			cout << "Failed: " << "Can't dup2(p[1], STDERR_FILENO)" << endl;
			exit(100);
		 }

		 close(p[1]);
		 execve(program.c_str(), argv, environ);
		 cout << "Failed: " << "Can not execute " << program << endl;
		 exit(100);
		 break;
	  default:
		 // Parent process
		 close(p[1]);
		 ChildFd = p[0];
		 return WaitChild(Owner,FRes,To);
   }
   return false;
}

// Parse output of rsync process with --apt-support option
void RsyncMethod::RsyncConnExecExt::ParseOutput(pkgAcqMethod *Owner, FetchResult &FRes, const char *buf)
{
   static const char * TMPFN = "Tmp-Filename: ";
   static const char * SIZE  = "Size: ";
   static const char * START = "Start: ";
   static const char * DONE  = "Done: ";
   static const char * FAILED= "Failed: ";
   const char * ptr;

   //if ( RsyncMethod::Debug )
   //  cerr << "ParseOutput: " << buf << endl;

   ptr = strstr(buf,TMPFN);
   if (ptr) {
	  ptr += strlen(TMPFN);
	  const char *ptr2 = ptr;
	  while (*ptr2!=0 && !isspace(*ptr2))
		 ++ptr2;
	  if (ptr!=ptr2) {
		 char *tmpfn = new char[ptr2-ptr+1];
		 bzero(tmpfn, ptr2-ptr+1);
		 strncpy(tmpfn, ptr, ptr2-ptr);
		 if (RsyncMethod::Debug)
			cerr << endl << "RSYNC: " << TMPFN << tmpfn << endl;
		 FRes.TmpFilename = string(tmpfn);
		 delete tmpfn;
	  }
   }

   ptr = strstr(buf,SIZE);
   if (ptr) {
	  ptr += strlen(SIZE);
	  unsigned long size = atol(ptr);
	  if (RsyncMethod::Debug)
		 cerr << "RSYNC: " << SIZE << size << endl;
	  FRes.Size = size;
   }

   ptr = strstr(buf,START);
   if (ptr) {
	  State = Fetching;
	  dynamic_cast<RsyncMethod*>(Owner)->Start(FRes);
   }

   ptr = strstr(buf,DONE);
   if (ptr)
	  State = Done;

   ptr = strstr(buf,FAILED);
   if (ptr) {
	  State = Failed;
	  ptr += strlen(FAILED);
	  const char *ptr2 = ptr;
	  while (*ptr2!=0 && *ptr2!='\n')
		 ++ptr2;
	  if (ptr!=ptr2) {
		 char *tmp = new char[ptr2-ptr+1];
		 bzero(tmp, ptr2-ptr+1);
		 strncpy(tmp, ptr, ptr2-ptr);
		 _error->Error("%s",tmp);
		 if (RsyncMethod::Debug)
			cerr << endl << FAILED << tmp << endl;
		 delete tmp;
	  } else {
		 _error->Error("Child process failed (no description)");
	  }
   }
}

// RsyncMethod::RsyncMethod - Constructor					/*{{{*/
// ---------------------------------------------------------------------
/* */
RsyncMethod::RsyncMethod() : pkgAcqMethod("1.0",SendConfig),
							 RsyncProg(RSYNC_PROGRAM)
{
   signal(SIGTERM,SigTerm);
   signal(SIGINT,SigTerm);
}
/*}}}*/
// RsyncMethod::SigTerm - Handle a fatal signal				/*{{{*/
// ---------------------------------------------------------------------
/* Delete existing server connection */
void RsyncMethod::SigTerm(int)
{
   delete server;
   _exit(100);
}
/*}}}*/
// RsyncMethod::Configuration - Handle a configuration message		/*{{{*/
// ---------------------------------------------------------------------
/* We stash the desired pipeline depth */
bool RsyncMethod::Configuration(string Message)
{
   if (pkgAcqMethod::Configuration(Message) == false)
	  return false;

   Debug = _config->FindB("Debug::rsync",false);
   Timeout = _config->FindI("Acquire::rsync::Timeout",0);
   RsyncProg = _config->Find("Acquire::rsync::program",RSYNC_PROGRAM);
   return true;
}
/*}}}*/
// RsyncMethod::Fetch - Fetch a file					/*{{{*/
// ---------------------------------------------------------------------
/* Fetch a single file, called by the base class..  */
bool RsyncMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   const char *File = Get.Path.c_str();
   FetchResult Res;
   Res.Filename = Itm->DestFile;
   Res.IMSHit = false;

   struct stat st;
   if ( stat(Itm->DestFile.c_str(), &st)==0 ) {
	  Res.ResumePoint = st.st_size;
   }

   string proxy = _config->Find(string("Acquire::rsync::proxy::")+Get.Host);
   if ( proxy.empty() )
	  proxy = _config->Find("Acquire::rsync::proxy");

   if (Debug)
	  cerr << endl << "RSYNC: Proxy(" << Get.Host << "): " << proxy << endl;

   // Don't compare now for the same server uri
   delete server;
   if ( _config->FindB("Acquire::rsync::apt-support",true) )
	 server = new RsyncConnExecExt(Get,proxy,RsyncProg);
   else
	 server = new RsyncConnExec(Get,proxy,RsyncProg);

   if ( server->Get(this,Res,File,Itm->DestFile.c_str()) ) {
	  if ( stat(Itm->DestFile.c_str(), &st)==0 ) {
		 Res.Size = st.st_size;
		 // Calculating checksum
		 //
		 int fd = open(Itm->DestFile.c_str(), O_RDONLY);
		 if (fd>=0) {
			Hashes Hash;
			Hash.AddFD(fd,st.st_size);
			Res.TakeHashes(Hash);
			close(fd);
		 }
	  }
	  URIDone(Res);
	  return true;
   }
   Fail(true);
   return false;
}
/*}}}*/

int main(int argc,const char *argv[])
{
   RsyncMethod Mth;

   return Mth.Run();
}
