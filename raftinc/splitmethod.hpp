/**
 * splitmethod.hpp -
 * @author: Jonathan Beard
 * @version: Tue Oct 28 12:56:43 2014
 *
 * Copyright 2014 Jonathan Beard
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef _SPLITMETHOD_HPP_
#define _SPLITMETHOD_HPP_  1

#include <type_traits>
#include <functional>

#include "autoreleasebase.hpp"
#include "signalvars.hpp"
#include "port.hpp"
#include "fifo.hpp"


class autoreleasebase;

class splitmethod
{
public:
   splitmethod()          = default;
   virtual ~splitmethod() = default;

   /**
    * send - this version sends one item ever time to the selected
    * output port (selected by the specific implementation.
    * @param item - item to be send
    * @param signal - sigal to be sent
    * @param outputs - output ports to be multiplexed over
    * @return std::size_t, for this one will either be one or zero items
    */
   template < class T /* item */,
              typename std::enable_if<
                        std::is_fundamental< T >::value >::type* = nullptr >
      std::size_t send( T &item, const raft::signal signal, Port &outputs )
   {
      auto * const fifo( select_fifo( outputs, sendtype ) );
      if( fifo != nullptr )
      {
         fifo->push( item, signal );
         return( static_cast< std::size_t >( 1 ) );
      }
      else
      {
         return( static_cast< std::size_t >( 0 ) );
      }
   }

   /**
    * send - this version is intended for the peekrange object from
    * autorelease.tcc in the fifo dir.  I'll add some code to enable
    * only on the autorelease object shortly, but for now this will
    * get it working.
    * @param   range - T&, autorelease object
    * @param   outputs - output port list
    * @return   number of items sent
    */
   template < class T   /* peek range obj,  */,
              typename std::enable_if<
                       ! std::is_base_of< autoreleasebase,
                                        T >::value >::type* = nullptr >
      std::size_t send( T &range, Port &outputs )
   {
      auto * const fifo( select_fifo( outputs, sendtype ) );
      if( fifo != nullptr )
      {
         const auto space_avail(
            std::min( fifo->space_avail(), range.size() ) );

         using index_type = std::remove_const_t<decltype(space_avail)>;
         for( index_type i( 0 ); i < space_avail; i++ )
         {
            fifo->push( range[ i ].ele, range[ i ].sig );
         }
         return( static_cast< std::size_t >( space_avail ) );
      }
      else
      {
         return( static_cast< std::size_t >( 0 ) );
      }
   }

   template < class T /* item */ >
      bool get( T &item, raft::signal &signal, Port &inputs )
   {
      auto * const fifo( select_fifo( inputs, gettype ) );
      if( fifo != nullptr )
      {
         fifo->pop< T >( item, &signal );
         return( true );
      }
      else
      {
         return( false );
      }
   }


protected:
   enum functype { sendtype, gettype };
   virtual FIFO*  select_fifo( Port &port_list, const functype type ) = 0;
};
#endif /* END _SPLITMETHOD_HPP_ */
