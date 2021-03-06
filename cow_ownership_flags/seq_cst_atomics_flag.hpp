/*  This file is part of copy_on_write_ptr.

    copy_on_write_ptr is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    copy_on_write_ptr is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with copy_on_write_ptr.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef SEQ_CST_ATOMICS_FLAG_H
#define SEQ_CST_ATOMICS_FLAG_H

#include <atomic>
#include <cstdint>

namespace cow_ownership_flags {

   // This implementation of the copy-on-write ownership flag uses sequentially consistent atomics
   // to achieve thread safety.
   class seq_cst_atomics_flag {
      public:
      
         // Ownership flags may be initialized to a certain value without synchronization, as at
         // construction time only one thread has access to the active ownership flag.
         seq_cst_atomics_flag(bool initially_owned) :
            m_ownership_status{to_ownership_status(initially_owned)}
         { }
         
         
         // When we move-construct from an ownership flag rvalue, we may assume that no other thread
         // has access to either that rvalue or the active flag, and avoid using synchronization.
         seq_cst_atomics_flag(seq_cst_atomics_flag && other) :
            m_ownership_status{other.unsynchronized_status()}
         { }
         
         
         // There's nothing special about deleting an ownership flag.
         ~seq_cst_atomics_flag() = default;
         
         
         // When we move-assign an ownership flag rvalue, no other thread has access to that rvalue,
         // so we can access it without read synchronization.
         // But the active flag may be shared with other threads, so we need write synchronization.
         seq_cst_atomics_flag & operator=(seq_cst_atomics_flag && other) {
            set_ownership_status(other.unsynchronized_status());
         }
         
         
         // Ownership flags are not copyable. Proper CoW semantics would require clearing them upon
         // copy, which is at odds with normal copy semantics. It's better to throw a compiler error
         // in this case, and let the user write more explicit code.
         seq_cst_atomics_flag(const seq_cst_atomics_flag &) = delete;
         seq_cst_atomics_flag & operator=(const seq_cst_atomics_flag &) = delete;
         
         
         // Authoritatively mark the active memory block as owned/not owned by the active thread.
         void set_ownership(bool owned) {
            set_ownership_status(to_ownership_status(owned));
         }
      
      
         // Acquire ownership of the active memory block, using the provided resource acquisition
         // routine, if that's not done already. Other threads should block during this process.
         template<typename Callable>
         void acquire_ownership_once(Callable && acquisition_routine) {
            // Try to switch the ownership status from NotOwner to AcquiringOwnership
            // and tell previous ownership status
            OwnershipStatusType previous_ownership = NotOwner;
            m_ownership_status.compare_exchange_strong(previous_ownership, AcquiringOwnership);
            
            // Act according to the previous ownership status
            switch(previous_ownership) {
               case NotOwner:  // Acquire resource ownership
                  acquisition_routine();
                  m_ownership_status.store(Owner);
                  break;
                  
               case AcquiringOwnership:  // Wait for ownership acquisition
                  while(m_ownership_status.load() != Owner);
                  break;
                  
               case Owner:  // Nothing to do, we already own the resource
                  break;
            }
         }
         
         
      private:
      
         using OwnershipStatusType = std::uint_fast8_t;
         enum OwnershipStatus : OwnershipStatusType { NotOwner, AcquiringOwnership, Owner };
         std::atomic<OwnershipStatusType> m_ownership_status;
         
         static OwnershipStatusType to_ownership_status(bool is_owned) {
            return (is_owned ? Owner : NotOwner);
         }
         
         OwnershipStatusType unsynchronized_status() {
            return m_ownership_status.load(std::memory_order_relaxed);
         }
         
         void set_ownership_status(const OwnershipStatusType desired_ownership) {
            OwnershipStatusType current_ownership = m_ownership_status.load();
            
            do {
               // Wait for any resource ownership acquisition operation to complete
               while(current_ownership == AcquiringOwnership) {
                  current_ownership = m_ownership_status.load();
               }
            
               // Once that is done, try to swap in the new resource ownership status
            } while(!m_ownership_status.compare_exchange_weak(current_ownership, desired_ownership));
         }
         
   };

}

#endif
