// Description								/*{{{*/
// $Id: acquire-worker.h,v 1.1 2002/07/23 17:54:50 niemeyer Exp $
/* ######################################################################

   Acquire Worker - Worker process manager
   
   Each worker class is associated with exaclty one subprocess.
   
   ##################################################################### */
									/*}}}*/
#ifndef PKGLIB_ACQUIRE_WORKER_H
#define PKGLIB_ACQUIRE_WORKER_H

#include <apt-pkg/acquire.h>

#ifdef __GNUG__
#pragma interface "apt-pkg/acquire-worker.h"
#endif 

// Interfacing to the method process
class pkgAcquire::Worker
{
   friend class pkgAcquire;
   
   protected:
   friend class Queue;

   /* Linked list starting at a Queue and a linked list starting
      at Acquire */
   Worker *NextQueue;
   Worker *NextAcquire;
   
   // The access association
   Queue *OwnerQ;
   pkgAcquireStatus *Log;
   MethodConfig *Config;
   string Access;

   // This is the subprocess IPC setup
   pid_t Process;
   int InFd;
   int OutFd;
   bool InReady;
   bool OutReady;
   
   // Various internal things
   bool Debug;
   vector<string> MessageQueue;
   string OutQueue;
   
   // Private constructor helper
   void Construct();
   
   // Message handling things
   bool ReadMessages();
   bool RunMessages();
   bool InFdReady();
   bool OutFdReady();
   
   // The message handlers
   bool Capabilities(const string &Message);
   bool SendConfiguration();
   bool MediaChange(const string &Message);
   bool Authenticate(const string &Message);
   
   bool MethodFailure();
   void ItemDone();
   
   public:
   
   // The curent method state
   pkgAcquire::Queue::QItem *CurrentItem;
   string Status;
   unsigned long long CurrentSize;
   unsigned long long TotalSize;
   unsigned long long ResumePoint;
   
   // Load the method and do the startup 
   bool QueueItem(pkgAcquire::Queue::QItem *Item);
   bool Start();
   void Pulse();
   inline const MethodConfig *GetConf() const {return Config;};
   
   Worker(Queue *OwnerQ,MethodConfig *Config,pkgAcquireStatus *Log);
   Worker(MethodConfig *Config);
   ~Worker();
};

#endif

// vim:sts=3:sw=3
