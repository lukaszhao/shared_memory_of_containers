#include <iostream>

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

void create_shm_of_vector()
{
	//Create a new segment with given name and size
	managed_shared_memory segment(create_only, "MySharedMemory", 65536);

	//Initialize shared memory STL-compatible allocator
	const ShmemAllocator alloc_inst(segment.get_segment_manager());

	//Construct a vector named "MyVector" in shared memory with argument alloc_inst
	MyVector *myvector = segment.construct<MyVector>("MyVector")(alloc_inst);

	for (int i = 0; i < 10; ++i) {  // insert data in the vector
		myvector->push_back(i);
	}
}

void read_shm_of_vector()
{
	//Open the managed segment
	managed_shared_memory segment(open_only, "MySharedMemory");

	//Find the vector using the c-string name
	MyVector *myvector = segment.find<MyVector>("MyVector").first;

	for (unsigned int i = 0; i < myvector->size(); ++i) {
		std::cout << "i = " << i << ", myvector[" << i << "] = " << (*myvector)[i] << "\n";
	}

	//Destroy the vector from the segment
	segment.destroy<MyVector>("MyVector");

	//Destroy managed_shared_memory using shared_memory_object::remove()
	shared_memory_object::remove("MySharedMemory");
}



int main()
{
	create_shm_of_vector();

	read_shm_of_vector();

	return 0;
}
