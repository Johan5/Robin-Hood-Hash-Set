#include "pch.h"
#include "CppUnitTest.h"

#include "RHSet.h"

#include <unordered_set>
#include <random>
#include <stdint.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using namespace NRobinHoodSet;

namespace
{
    bool HasEqualEntries(const std::unordered_set<int32_t>& SetA, const RobinHoodSet<int32_t>& SetB)
    {
        for (int32_t i : SetA)
        {
            if (!SetB.Contains(i))
            {
                return false;
            }
        }

        for (int32_t i : SetB)
        {
            if (SetA.find(i) == SetA.end())
            {
                return false;
            }
        }

        return true;
    }
}

namespace Tests
{
	TEST_CLASS(RobinHoodSetTest)
	{
	public:
		TEST_METHOD(AddRandom)
		{
            uint32_t Seed = 0;
            RobinHoodSet<int32_t> RHHashSet;
            std::unordered_set<int32_t> StlSet;

            std::mt19937 Rng{ Seed };
            std::uniform_int_distribution<int32_t> Dist(0, 5000);

            for (int32_t i = 0; i < 5'000; ++i)
            {
                int32_t R = Dist(Rng);
                RHHashSet.Set(R);
                StlSet.insert(R);
                
                Assert::AreEqual(RHHashSet.GetSize(), static_cast<int32_t>( StlSet.size() ));
                if (i % 1000 == 0)
                {
                    Assert::IsTrue(HasEqualEntries(StlSet, RHHashSet));
                }
            }
            Assert::IsTrue(HasEqualEntries(StlSet, RHHashSet));
		}

        TEST_METHOD(Remove)
        {
            uint32_t Seed = 0;
            RobinHoodSet<int32_t> RHHashSet;
            std::unordered_set<int32_t> StlSet;

            std::mt19937 Rng{ Seed };
            std::uniform_int_distribution<int32_t> Dist(0, 250);

            for (int32_t i = 0; i < 5'000; ++i)
            {
                bool ShouldAdd = static_cast<float>(Rng()) / Rng.max();
                int32_t R = Dist(Rng);
                if (ShouldAdd)
                {
                    RHHashSet.Set(R);
                    StlSet.insert(R);
                }
                else
                {
                    int32_t NumRemovedElements = StlSet.erase(R);
                    bool StlRemoved = (NumRemovedElements > 0);
                    bool RhRemoved = RHHashSet.Remove(R);
                    Assert::AreEqual(StlRemoved, RhRemoved);
                }
                Assert::AreEqual(RHHashSet.GetSize(), static_cast<int32_t>(StlSet.size()));
                if (i % 1000 == 0)
                {
                    Assert::IsTrue(HasEqualEntries(StlSet, RHHashSet));
                }
            }
            Assert::IsTrue(HasEqualEntries(StlSet, RHHashSet));
        }
	};
}

