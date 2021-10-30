// Description								/*{{{*/
// $Id: hashes.cc,v 1.1 2002/07/23 17:54:51 niemeyer Exp $
/* ######################################################################

   Hashes - Simple wrapper around the hash functions

   This is just used to make building the methods simpler, this is the
   only interface required..

   ##################################################################### */
									/*}}}*/
// Include Files							/*{{{*/
#include <config.h>

#include <apt-pkg/hashes.h>

#include <unistd.h>
#include <system.h>
									/*}}}*/
bool Hashes::Add(const unsigned char *Data,unsigned long Size)
{
   HashContainer::iterator I;
   for (I = HashSet.begin(); I != HashSet.end(); I++) {
      if (I->Add(Data,Size) == false)
         break;
   }
   return (I == HashSet.end());
}

// Hashes::AddFD - Add the contents of the FD				/*{{{*/
// ---------------------------------------------------------------------
/* */
bool Hashes::AddFD(int Fd,unsigned long Size)
{
   unsigned char Buf[64*64];
   int Res = 0;
   while (Size != 0)
   {
      Res = read(Fd,Buf,MIN(Size,sizeof(Buf)));
      if (Res < 0 || (unsigned)Res != MIN(Size,sizeof(Buf)))
	 return false;
      Size -= Res;
      if (! Add(Buf,Res))
         return false;
   }
   return true;
}
									/*}}}*/
Hashes::Hashes()
{
   const char *htypes[] = { "MD5-Hash", "SHA1-Hash", "SHA256-Hash", NULL };

   for (const char **name = htypes; *name != NULL; name++)
       HashSet.push_back(raptHash(*name));
}
