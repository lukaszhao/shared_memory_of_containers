
http://www.boost.org/doc/libs/1_65_1/doc/html/interprocess/quick_guide.html

Quick Guide for the Impatient
Using shared memory as a pool of unnamed memory blocks
Creating named shared memory objects
Using an offset smart pointer for shared memory
Creating vectors in shared memory
Creating maps in shared memory
Using shared memory as a pool of unnamed memory blocks
You can just allocate a portion of a shared memory segment, copy the message to that buffer, send the offset of that portion of shared memory to another process, and you are done. Let's see the example:
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdlib> //std::system
#include <sstream>

int main (int argc, char *argv[])
{
   using namespace boost::interprocess;
   if(argc == 1){  //Parent process
      //Remove shared memory on construction and destruction
      struct shm_remove
      {
         shm_remove() {  shared_memory_object::remove("MySharedMemory"); }
         ~shm_remove(){  shared_memory_object::remove("MySharedMemory"); }
      } remover;

      //Create a managed shared memory segment
      managed_shared_memory segment(create_only, "MySharedMemory", 65536);

      //Allocate a portion of the segment (raw memory)
      managed_shared_memory::size_type free_memory = segment.get_free_memory();
      void * shptr = segment.allocate(1024/*bytes to allocate*/);

      //Check invariant
      if(free_memory <= segment.get_free_memory())
         return 1;

      //An handle from the base address can identify any byte of the shared
      //memory segment even if it is mapped in different base addresses
      managed_shared_memory::handle_t handle = segment.get_handle_from_address(shptr);
      std::stringstream s;
      s << argv[0] << " " << handle;
      s << std::ends;
      //Launch child process
      if(0 != std::system(s.str().c_str()))
         return 1;
      //Check memory has been freed
      if(free_memory != segment.get_free_memory())
         return 1;
   }
   else{
      //Open managed segment
      managed_shared_memory segment(open_only, "MySharedMemory");

      //An handle from the base address can identify any byte of the shared
      //memory segment even if it is mapped in different base addresses
      managed_shared_memory::handle_t handle = 0;

      //Obtain handle value
      std::stringstream s; s << argv[1]; s >> handle;

      //Get buffer local address from handle
      void *msg = segment.get_address_from_handle(handle);

      //Deallocate previously allocated memory
      segment.deallocate(msg);
   }
   return 0;
}
Creating named shared memory objects
You want to create objects in a shared memory segment, giving a string name to them so that any other process can find, use and delete them from the segment when the objects are not needed anymore. Example:
#include <boost/interprocess/managed_shared_memory.hpp>
#include <cstdlib> //std::system
#include <cstddef>
#include <cassert>
#include <utility>

int main(int argc, char *argv[])
{
   using namespace boost::interprocess;
   typedef std::pair<double, int> MyType;

   if(argc == 1){  //Parent process
      //Remove shared memory on construction and destruction
      struct shm_remove
      {
         shm_remove() { shared_memory_object::remove("MySharedMemory"); }
         ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
      } remover;

      //Construct managed shared memory
      managed_shared_memory segment(create_only, "MySharedMemory", 65536);

      //Create an object of MyType initialized to {0.0, 0}
      MyType *instance = segment.construct<MyType>
         ("MyType instance")  //name of the object
         (0.0, 0);            //ctor first argument

      //Create an array of 10 elements of MyType initialized to {0.0, 0}
      MyType *array = segment.construct<MyType>
         ("MyType array")     //name of the object
         [10]                 //number of elements
         (0.0, 0);            //Same two ctor arguments for all objects

      //Create an array of 3 elements of MyType initializing each one
      //to a different value {0.0, 0}, {1.0, 1}, {2.0, 2}...
      float float_initializer[3] = { 0.0, 1.0, 2.0 };
      int   int_initializer[3]   = { 0, 1, 2 };

      MyType *array_it = segment.construct_it<MyType>
         ("MyType array from it")   //name of the object
         [3]                        //number of elements
         ( &float_initializer[0]    //Iterator for the 1st ctor argument
         , &int_initializer[0]);    //Iterator for the 2nd ctor argument

      //Launch child process
      std::string s(argv[0]); s += " child ";
      if(0 != std::system(s.c_str()))
         return 1;


      //Check child has destroyed all objects
      if(segment.find<MyType>("MyType array").first ||
         segment.find<MyType>("MyType instance").first ||
         segment.find<MyType>("MyType array from it").first)
         return 1;
   }
   else{
      //Open managed shared memory
      managed_shared_memory segment(open_only, "MySharedMemory");

      std::pair<MyType*, managed_shared_memory::size_type> res;

      //Find the array
      res = segment.find<MyType> ("MyType array");
      //Length should be 10
      if(res.second != 10) return 1;

      //Find the object
      res = segment.find<MyType> ("MyType instance");
      //Length should be 1
      if(res.second != 1) return 1;

      //Find the array constructed from iterators
      res = segment.find<MyType> ("MyType array from it");
      //Length should be 3
      if(res.second != 3) return 1;

      //We're done, delete all the objects
      segment.destroy<MyType>("MyType array");
      segment.destroy<MyType>("MyType instance");
      segment.destroy<MyType>("MyType array from it");
   }
   return 0;
}
Using an offset smart pointer for shared memory
Boost.Interprocess?offers offset_ptr smart pointer family as an offset pointer that stores the distance between the address of the offset pointer itself and the address of the pointed object. When offset_ptr is placed in a shared memory segment, it can point safely objects stored in the same shared memory segment, even if the segment is mapped in different base addresses in different processes.
This allows placing objects with pointer members in shared memory. For example, if we want to create a linked list in shared memory:
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/offset_ptr.hpp>

using namespace boost::interprocess;

//Shared memory linked list node
struct list_node
{
   offset_ptr<list_node> next;
   int                   value;
};

int main ()
{
   //Remove shared memory on construction and destruction
   struct shm_remove
   {
      shm_remove() { shared_memory_object::remove("MySharedMemory"); }
      ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
   } remover;

   //Create shared memory
   managed_shared_memory segment(create_only,
                                 "MySharedMemory",  //segment name
                                 65536);

   //Create linked list with 10 nodes in shared memory
   offset_ptr<list_node> prev = 0, current, first;

   int i;
   for(i = 0; i < 10; ++i, prev = current){
      current = static_cast<list_node*>(segment.allocate(sizeof(list_node)));
      current->value = i;
      current->next  = 0;

      if(!prev)
         first = current;
      else
         prev->next = current;
   }

   //Communicate list to other processes
   //. . .
   //When done, destroy list
   for(current = first; current; /**/){
      prev = current;
      current = current->next;
      segment.deallocate(prev.get());
   }
   return 0;
}
To help with basic data structures,?Boost.Interprocess?offers containers like vector, list, map, so you can avoid these manual data structures just like with standard containers.
Creating vectors in shared memory
Boost.Interprocess?allows creating complex objects in shared memory and memory mapped files. For example, we can construct STL-like containers in shared memory. To do this, we just need to create a special (managed) shared memory segment, declare a?Boost.Interprocess?allocator and construct the vector in shared memory just if it was any other object.
The class that allows this complex structures in shared memory is called?boost::interprocess::managed_shared_memory?and it's easy to use. Just execute this example without arguments:
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <string>
#include <cstdlib> //std::system

using namespace boost::interprocess;

//Define an STL compatible allocator of ints that allocates from the managed_shared_memory.
//This allocator will allow placing containers in the segment
typedef allocator<int, managed_shared_memory::segment_manager>  ShmemAllocator;

//Alias a vector that uses the previous STL-like allocator so that allocates
//its values from the segment
typedef vector<int, ShmemAllocator> MyVector;

//Main function. For parent process argc == 1, for child process argc == 2
int main(int argc, char *argv[])
{
   if(argc == 1){ //Parent process
      //Remove shared memory on construction and destruction
      struct shm_remove
      {
         shm_remove() { shared_memory_object::remove("MySharedMemory"); }
         ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
      } remover;

      //Create a new segment with given name and size
      managed_shared_memory segment(create_only, "MySharedMemory", 65536);

      //Initialize shared memory STL-compatible allocator
      const ShmemAllocator alloc_inst (segment.get_segment_manager());

      //Construct a vector named "MyVector" in shared memory with argument alloc_inst
      MyVector *myvector = segment.construct<MyVector>("MyVector")(alloc_inst);

      for(int i = 0; i < 100; ++i)  //Insert data in the vector
         myvector->push_back(i);

      //Launch child process
      std::string s(argv[0]); s += " child ";
      if(0 != std::system(s.c_str()))
         return 1;

      //Check child has destroyed the vector
      if(segment.find<MyVector>("MyVector").first)
         return 1;
   }
   else{ //Child process
      //Open the managed segment
      managed_shared_memory segment(open_only, "MySharedMemory");

      //Find the vector using the c-string name
      MyVector *myvector = segment.find<MyVector>("MyVector").first;

      //Use vector in reverse order
      std::sort(myvector->rbegin(), myvector->rend());

      //When done, destroy the vector from the segment
      segment.destroy<MyVector>("MyVector");
   }

   return 0;
}
The parent process will create an special shared memory class that allows easy construction of many complex data structures associated with a name. The parent process executes the same program with an additional argument so the child process opens the shared memory and uses the vector and erases it.
Creating maps in shared memory
Just like a vector,?Boost.Interprocess?allows creating maps in shared memory and memory mapped files. The only difference is that like standard associative containers,?Boost.Interprocess's map needs also the comparison functor when an allocator is passed in the constructor:
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <functional>
#include <utility>

int main ()
{
   using namespace boost::interprocess;

   //Remove shared memory on construction and destruction
   struct shm_remove
   {
      shm_remove() { shared_memory_object::remove("MySharedMemory"); }
      ~shm_remove(){ shared_memory_object::remove("MySharedMemory"); }
   } remover;

   //Shared memory front-end that is able to construct objects
   //associated with a c-string. Erase previous shared memory with the name
   //to be used and create the memory segment at the specified address and initialize resources
   managed_shared_memory segment
      (create_only
      ,"MySharedMemory" //segment name
      ,65536);          //segment size in bytes

   //Note that map<Key, MappedType>'s value_type is std::pair<const Key, MappedType>,
   //so the allocator must allocate that pair.
   typedef int    KeyType;
   typedef float  MappedType;
   typedef std::pair<const int, float> ValueType;

   //Alias an STL compatible allocator of for the map.
   //This allocator will allow to place containers
   //in managed shared memory segments
   typedef allocator<ValueType, managed_shared_memory::segment_manager>
      ShmemAllocator;

   //Alias a map of ints that uses the previous STL-like allocator.
   //Note that the third parameter argument is the ordering function
   //of the map, just like with std::map, used to compare the keys.
   typedef map<KeyType, MappedType, std::less<KeyType>, ShmemAllocator> MyMap;

   //Initialize the shared memory STL-compatible allocator
   ShmemAllocator alloc_inst (segment.get_segment_manager());

   //Construct a shared memory map.
   //Note that the first parameter is the comparison function,
   //and the second one the allocator.
   //This the same signature as std::map's constructor taking an allocator
   MyMap *mymap =
      segment.construct<MyMap>("MyMap")      //object name
                                 (std::less<int>() //first  ctor parameter
                                 ,alloc_inst);     //second ctor parameter

   //Insert data in the map
   for(int i = 0; i < 100; ++i){
      mymap->insert(std::pair<const int, float>(i, (float)i));
   }
   return 0;
}
For a more advanced example including containers of containers, see the section?Containers of containers.
Copyright ? 2005-2015 Ion Gaztanaga
Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at?http://www.boost.org/LICENSE_1_0.txt)


