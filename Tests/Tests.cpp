#include "pch.h"
#include "CppUnitTest.h"

#include "RHSet.h"

#include <unordered_set>
#include <random>
#include <stdint.h>
#include <functional>

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

struct SMoveableInvokationCounter
{
    struct SInvokations
    {
        int32_t _Constructor = 0;
        int32_t _CopyConstructor = 0;
        int32_t _MoveConstructor = 0;
        int32_t _Destructor = 0;
    };

    SMoveableInvokationCounter(SInvokations& InvokationCounter, int32_t Hash)
        : _InvokationCounter(InvokationCounter)
    {
        _InvokationCounter._Constructor++;
        _Hash = Hash;
    }
    SMoveableInvokationCounter(SMoveableInvokationCounter& Other)
        : _InvokationCounter(Other._InvokationCounter)
    {
        _InvokationCounter._CopyConstructor++;
        _Hash = Other._Hash;
    }
    SMoveableInvokationCounter(SMoveableInvokationCounter&& Other)
        : _InvokationCounter(Other._InvokationCounter)
    {
        _InvokationCounter._MoveConstructor++;
        _Hash = Other._Hash;
    }
    ~SMoveableInvokationCounter()
    {
        _InvokationCounter._Destructor++;
    }

    bool operator==(const SMoveableInvokationCounter& Other)
    {
        return _Hash == Other._Hash;
    }

    int32_t _Hash;
    SInvokations& _InvokationCounter;
};

namespace std
{
    template <>
    struct hash<SMoveableInvokationCounter>
    {
        size_t operator()(const SMoveableInvokationCounter& Object) const
        {
            return Object._Hash;
        }
    };
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

        // Checks that the implementation properly calls destructor and uses move constructor when availible
        TEST_METHOD(Invokation)
        {
            uint32_t Seed = 0;
            std::mt19937 Rng{ Seed };
            
            
            auto GetHash = [](std::mt19937& Rng) -> int32_t
            {
                static std::unordered_set<int32_t> UsedHashes;
                std::uniform_int_distribution<int32_t> Dist(0, std::numeric_limits<int32_t>::max());
                while (UsedHashes.size() < 0.9 * std::numeric_limits<int32_t>::max())
                {
                    int32_t R = Dist(Rng);
                    auto Result = UsedHashes.insert(R);
                    bool InsertedValue = Result.second;
                    if (InsertedValue)
                    {
                        return R;
                    }
                    // If we did not insert value, it is already in use, and we try again with a new value
                }
                // This definitely should never happen
                Assert::IsTrue(false);
                return -1;
            };

            SMoveableInvokationCounter::SInvokations InvokationCounter;
            {
                RobinHoodSet<SMoveableInvokationCounter> RHHashSet;
                std::uniform_int_distribution<int32_t> Dist(0, 5000);
                for (int32_t i = 0; i < 5'000; ++i)
                {
                    bool ShouldRemove = static_cast<float>(Rng()) / Rng.max();
                    int32_t R = Dist(Rng);
                    RHHashSet.Set(SMoveableInvokationCounter{ InvokationCounter, R });
                    if (ShouldRemove)
                    {
                        RHHashSet.Remove(SMoveableInvokationCounter{ InvokationCounter, R });
                    }
                    int32_t ConstructionCount = InvokationCounter._Constructor + InvokationCounter._CopyConstructor + InvokationCounter._MoveConstructor;
                    // Ensure we do not leak memory
                    Assert::AreEqual(ConstructionCount, InvokationCounter._Destructor + RHHashSet.GetSize());
                    // Ensure we do not copy a move'able object
                    Assert::AreEqual(InvokationCounter._CopyConstructor, 0);
                }
            }
            int32_t ConstructionCount = InvokationCounter._Constructor + InvokationCounter._CopyConstructor + InvokationCounter._MoveConstructor;
            Assert::AreEqual(ConstructionCount, InvokationCounter._Destructor);
        }
	};
}

