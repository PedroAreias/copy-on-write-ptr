#ifndef THREAD_UNSAFE_FLAG_H
#define THREAD_UNSAFE_FLAG_H

#include <mutex>

namespace cow_ownership_flags {

   // This implementation of the copy-on-write ownership flag makes no particular efforts to achieve thread safety
   class thread_unsafe_flag {
      public:
      
         // Construct our ownership flag from an initial value
         thread_unsafe_flag(bool initially_owned) : m_owned{initially_owned} { }
         
         // Move-construct the flag from a flag rvalue
         thread_unsafe_flag(thread_unsafe_flag && other) : m_owned{other.m_owned} { }
         
         // There's nothing special about deleting an ownership flag.
         ~thread_unsafe_flag() = default;
         
         // Move-assign the flag. If thread safety is not desired, this is equivalent to move-initialization
         thread_unsafe_flag & operator=(thread_unsafe_flag && other) {
            set_ownership(other.m_owned);
         }
         
         // Ownership flags are not copyable. Proper CoW semantics would require clearing them upon copy,
         // which is at odds with normal copy semantics. It's better to throw a compiler error in this case,
         // and let the user write more explicit code.
         thread_unsafe_flag(const thread_unsafe_flag &) = delete;
         thread_unsafe_flag & operator=(const thread_unsafe_flag &) = delete;
         
         // Authoritatively mark the active memory block as owned/not owned by the active thread
         void set_ownership(bool owned) {
            m_owned = owned;
         }
      
         // Acquire ownership of the active memory block once, using the provided resource acquisition routine,
         // if that's not done already. Disregard the possibility that other threads may be doing the same thing.
         template<typename Callable>
         void acquire_ownership_once(Callable && acquire) {
            if(!m_owned) {
               acquire();
               m_owned = true;
            }
         }
         
      private:
      
         bool m_owned;
   };

}

#endif
