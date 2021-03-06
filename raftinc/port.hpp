/**
 * port.hpp -
 * @author: Jonathan Beard
 * @version: Thu Aug 28 09:55:47 2014
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
#ifndef RAFTPORT_HPP
#define RAFTPORT_HPP  1

#include <map>
#include <vector>
#include <string>
#include <utility>
#include <typeinfo>
#include <typeindex>
#include <functional>
#include <utility>

#include "portbase.hpp"
#include "graphtools.hpp"
#include "ringbuffertypes.hpp"
#include "fifo.hpp"
#include "port_info.hpp"
#include "ringbuffer.tcc"
#include "port_info_types.hpp"
#include "portmap_t.hpp"
#include "portiterator.hpp"
#include "portexception.hpp"
#include "defs.hpp"

#ifndef NOVL
#include "fifo_vl.tcc"
#endif

/** needed for friending below **/
class MapBase;
class roundrobin;
class basic_parallel;
/** need to pre-declare this **/
namespace raft
{
   class map;
   class kernel;
   class parallel_k;
   template < class T, class method > class join;
   template < class T, class method > class split;
}


/** some helper structs for recursive port adding **/
template < class T, class PORT, class... PORTNAMES >
struct port_helper{};

/** stop recursion **/
template < class T, class PORT >
struct port_helper< T, PORT >
{
    static void add_port( PORT &port ) 
    {
        UNUSED( port );
        return;
    }

};

/** continue recursion **/
template < class T,
           class PORT,
           class PORTNAME,
           class... PORTNAMES >
struct port_helper< T, PORT, PORTNAME, PORTNAMES... >
{
    static void add_port( PORT &port,
                          PORTNAME &&portname,
                          PORTNAMES&&... portnames )
    {
        port.template add_port< T >( portname );
        port_helper< T, PORT, PORTNAMES... >::add_port( port,
                                                        std::forward< PORTNAMES >( portnames )... );
        return;
    }
};

/** kicks off recursion for adding ports **/
template< class T,
          class PORT,
          class... PORTNAMES >
static void
kick_port_helper( PORT &port, PORTNAMES&&... ports )
{
    port_helper< T, PORT, PORTNAMES... >::add_port( port,
        std::forward< PORTNAMES >( ports )... );
    return;
}


class Port : public PortBase
{
public:
   /**
    * Port - constructor used to construct a standard port
    * object, needs a reference to the parent kernel for
    * the port_info struct
    * @param   k  - raft::kernel*
    */
   Port( raft::kernel * const k );

   /**
    * Port - constructor used to construct a port with
    * pre-allocated memory, useful for things like
    * array distribution and reduction
    * @param   k - raft::kernel*
    * @param   ptr - void*
    * @param   nbytes - const std::size_t length in bytes
    */
   Port( raft::kernel * const k,
         void * const ptr,
         const std::size_t nbytes );

   /**
    * ~Port - destructor, deletes the FIFO that was given
    * when the object was initalized.
    */
   virtual ~Port() = default;


   /**
    * addPort - adds and initializes a port for the name
    * given.  Function returns true if added, false if not.
    * Main reason for returning false would be that the
    * port already exists.
    * @param   port_name - const std::string
    * @return  bool
    */
   template < class T,
              class... PORTNAMES >
   void addPort(  PORTNAMES&&... ports )
   {
       kick_port_helper< T, Port, PORTNAMES... >(
        (*this),
        std::forward< PORTNAMES >( ports )... );
   }



   /**
    * addPorts - add ports for an existing buffer, basically
    * allocate buffers in place.  These also won't be able
    * to be resized.
    * @param n_ports - const std::size_t
    */
   template < class T >
   bool addInPlacePorts( const std::size_t n_ports )
   {
      T *existing_buff_t( reinterpret_cast< T* >( alloc_ptr ) );
      std::size_t length( alloc_ptr_length / sizeof( T ) );
      const std::size_t inc( length / n_ports );
      const std::size_t adder( length % n_ports );

      using index_type = std::remove_const_t<decltype(n_ports)>;
      for( index_type index( 0 ); index < n_ports; index++ )
      {
         const std::size_t start_index( index * inc );
         PortInfo pi( typeid( T ),
                      (void*)&( existing_buff_t[ start_index ] ) /** pointer **/,
                      inc + ( index == (n_ports - 1) ? adder : 0 ),
                      start_index );
         pi.my_kernel = kernel;
         const std::string name( std::to_string( index ) );
         pi.my_name   = name;
         /** gotta initialize the maps to copy stuff to/from **/
         (this)->initializeConstMap< T >( pi );
         (this)->initializeSplit<    T >( pi );
         (this)->initializeJoin<     T >( pi );
         portmap.map.insert( std::make_pair( name, pi ) );
      }
      return( true );
   }

   /**
    * getPortType - input the port name, and get the hash
    * for the type of the port.  This function is useful
    * for checking the streaming graph to make sure all the
    * ports that are "dynamically" created do in fact have
    * compatible types.
    * @param port_name - const std::string
    * @return  const type_index&
    * @throws PortNotFoundException
    */
   const std::type_index& getPortType( const std::string &&port_name );


   /**
    * operator[] - input the port name and get a port
    * if it exists.
    */
   virtual FIFO& operator[]( const std::string &&port_name  );
   virtual FIFO& operator[]( const std::string &port_name ); 

   /**
    * hasPorts - returns true if any ports exists, false
    * otherwise.
    * @return   bool
    */
   virtual bool hasPorts();

   /**
    * begin - get the beginning port.
    * @return PortIterator
    */
   virtual PortIterator begin();

   /**
    * end - get the end port
    * @return PortIterator
    */
   virtual PortIterator end();

   /**
    * count - get the total number of fifos within this port container
    * @return std::size_t
    */
   std::size_t count();

//TODO, get this guy into the private area
   /**
    * add_port - adds and initializes a port for the name
    * given.  Function returns true if added, false if not.
    * Main reason for returning false would be that the
    * port already exists.
    * @param   port_name - const std::string
    * @return  bool
    */
   template < class T >
   void add_port( const std::string &port_name )
   {
      /**
       * we'll have to make a port info object first and pass it by copy
       * to the portmap.  Perhaps re-work later with pointers, but for
       * right now this will work and it doesn't necessarily have to
       * be performant since its only executed once.
       */
      PortInfo pi( typeid( T ) );
      pi.my_kernel = kernel;
      pi.my_name   = port_name;
      (this)->initializeConstMap<T>( pi );
      (this)->initializeSplit< T >( pi );
      (this)->initializeJoin< T >( pi );
      const auto ret_val(
                  portmap.map.insert( std::make_pair( port_name,
                                                      pi ) ) );

      if( ! ret_val.second )
      {
         throw PortAlreadyExists( "FATAL ERROR: port \"" + port_name + "\" already exists!" );
      }
      return;
   }


protected:
   /**
    * initializeConstMap - hack to get around the inability to otherwise
    * initialize a template function where later we don't have the
    * template parameter.  NOTE:  this is a biggy, if we have more
    * FIFO types in the future (i.e., sub-classes of FIFO) then we
    * must create an entry here otherwise bad things will happen.
    * @param   pi - PortInfo&
    */
   template < class T > void initializeConstMap( PortInfo &pi )
   {
      pi.const_map.insert(
         std::make_pair( Type::Heap , std::make_shared< instr_map_t >() ) );

      pi.const_map[ Type::Heap ]->insert(
         std::make_pair( false /** no instrumentation **/,
                         RingBuffer< T, Type::Heap, false >::make_new_fifo ) );
      pi.const_map[ Type::Heap ]->insert(
         std::make_pair( true /** yes instrumentation **/,
                         RingBuffer< T, Type::Heap, true >::make_new_fifo ) );
#ifndef NOVL
      pi.const_map.insert(
         std::make_pair( Type::VirtualLink , std::make_shared< instr_map_t >() ) );

      pi.const_map[ Type::VirtualLink ]->insert(
         std::make_pair( false /** no instrumentation **/,
                         VLBuffer< T, Type::VirtualLink >::make_new_fifo ) );
      pi.const_map[ Type::VirtualLink ]->insert(
         std::make_pair( true /** yes instrumentation **/,
                         VLBuffer< T, Type::VirtualLink >::make_new_fifo ) );
#endif
      //pi.const_map.insert( std::make_pair( Type::SharedMemory, new instr_map_t() ) );
      //pi.const_map[ Type::SharedMemory ]->insert(
      //   std::make_pair( false /** no instrumentation **/,
      //                   RingBuffer< T, Type::SharedMemory >::make_new_fifo ) );
      /**
       * NOTE: If you define more port resource types, they have
       * to be defined here...otherwise the allocator won't be
       * able to allocate the correct type, size, etc. for the
       * port..and well, it'll be sad.
       */
      return;
   }

   /**
    * initializeSplit - pre-allocate split kernels...saves
    * allocation time later, then all that is needed is to
    * hook them up, and allocate memory for the ports.
    */
   template < class T > void initializeSplit( PortInfo &pi )
   {
      pi.split_func =
         []() -> raft::kernel*
         {
            return(  new raft::split< T, roundrobin >()  );
         };
      return;
   }



   /**
    * initializeJoin - pre-allocate join kernels...saves
    * allocation time later, takes up minimal space and
    * all that is needed when these are actually used
    * is to allocate memory for the ports which is done
    * by the
    */
   template < class T > void initializeJoin( PortInfo &pi )
   {
      pi.join_func =
         []() -> raft::kernel*
         {
            return(  new raft::join< T, roundrobin >() );
         };
      return;
   }

   /**
    * getPortInfo - returns the PortInfo struct for a kernel if we
    * expect it to have a single port.  If there's more than one port
    * this function throws an exception.
    * @return  std::pair< std::string, PortInfo& >
    */
   PortInfo& getPortInfo();

   /**
    * getPortInfoFor - gets port information for the param port
    * throws an exception if the port doesn't exist.
    * @param   port_name - const std::string
    * @return  PortInfo&
    */
   PortInfo& getPortInfoFor( const std::string port_name );

   /**
    * portmap - container struct with all ports.  The
    * mutex should be locked before accessing this structure
    */
   portmap_t   portmap;

   /**
    * parent kernel that owns this port
    */
   raft::kernel *    kernel            = nullptr;

   /**
    * ptr used for in-place allocations, will
    * not be deleted by the map, also should not
    * be modified by the map either.
    */
   void * const      alloc_ptr         = nullptr;

   /**
    * alloc_ptr_length - length of alloc_ptr in
    * bytes.
    */
   const std::size_t alloc_ptr_length  = 0;

   /** we need some friends **/
   friend class MapBase;
   friend class raft::kernel;
   friend class raft::map;
   friend class GraphTools;
   friend class basic_parallel;
   friend class raft::parallel_k;
};



#endif /* END RAFTPORT_HPP */
