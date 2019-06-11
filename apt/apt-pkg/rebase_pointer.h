#ifndef LIBAPT_REBASE_POINTER_H
#define LIBAPT_REBASE_POINTER_H

template <typename T>
static inline T* RebasePointer(T *ptr,
                               void *old_base, void *new_base)
{
   return reinterpret_cast<T*>(reinterpret_cast<char*>(new_base)
      + (reinterpret_cast<char*>(ptr)
         - reinterpret_cast<char*>(old_base)));
}

template <typename T>
static inline const T* RebasePointer(const T *ptr,
                                     void *old_base, void *new_base)
{
   return reinterpret_cast<const T*>(reinterpret_cast<char*>(new_base)
      + (reinterpret_cast<const char*>(ptr)
         - reinterpret_cast<char*>(old_base)));
}

#endif
