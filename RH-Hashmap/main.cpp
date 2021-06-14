#include <iostream>
#include "RHSet.h"
#include <random>
#include <stdint.h>

using namespace NRobinHoodSet;

int main() {
    //int32_t  Counter = 0;
    //RobinHoodSet<int32_t> HashSet;
    //HashSet.Set(1);
    //HashSet.Set(5);
    //HashSet.Set(6);
    //HashSet.Set(2);
    //HashSet.Set(3);
    //HashSet.Set(1);
    //HashSet.Set(4);

    //HashSet.Set(23);
    //HashSet.Set(31);
    //HashSet.Set(39);
    //HashSet.Set(48);
    //HashSet.Set(57);
    //HashSet.Set(64);

    
    uint32_t Seed = 0;
    RobinHoodSet<int32_t> RHHashSet;

    std::mt19937 Rng{ Seed };
    std::uniform_int_distribution<int32_t> Dist(0, 5000);

    for (int32_t i = 0; i < 300; ++i)
    {
        bool ShouldAdd = static_cast<float>(Rng()) / Rng.max();
        int32_t R = Dist(Rng);
        if (ShouldAdd)
        {
            RHHashSet.Set(R);
        }
        else
        {
            // Remove
        }
    }

    return 0;
}


