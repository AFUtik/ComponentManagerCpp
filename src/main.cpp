#include <iostream>

#include "components/BasicComponent.hpp"
#include "collections/SparseSet.hpp"

using ListenerId = u64;


int main(int, char**)
{
    // Example //
    AllocatedSerialSparseSet<u32, u32, 256> sparse_set;
    sparse_set.push(0);
    sparse_set.push(3);
    sparse_set.push(5);
    sparse_set.push(8);
    sparse_set.push(12);

    sparse_set.erase(2);

    for(u32 v : sparse_set) {std::cout << v << std::endl;}

    return 0;
}