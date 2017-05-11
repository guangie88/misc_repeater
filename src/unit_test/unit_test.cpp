#include "misc/repeater.h"

#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <thread>

// misc/repeater.h
using misc::repeater;

// std
using std::atomic_size_t;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

TEST(Repeater, InterruptDoesNothing)
{
    size_t count = 0;

    {
        repeater r(
            milliseconds(1000),
            false,
            [&count] { ++count; });

        // 2x activations
        sleep_for(milliseconds(2500));
        EXPECT_EQ(2, count);

        // this refreshes the repeater interval
        r.interrupt();
        EXPECT_EQ(2, count);

        // 2x activations
        sleep_for(milliseconds(2500));
        EXPECT_EQ(4, count);

        // repeater is destructed here
    }

    EXPECT_EQ(4, count);
}

TEST(Repeater, InterruptDoesSomething)
{
    atomic_size_t count(0);

    {
        repeater r(
            milliseconds(500),
            true,
            [&count] { ++count; });

        // 5x activations
        sleep_for(milliseconds(2700));
        EXPECT_EQ(5, count);

        // 1x forced activation
        // each interrupt requires a small amount of time before the count gets updated
        // though it is not completely foolproof
        r.interrupt();
        sleep_for(milliseconds(50));
        EXPECT_EQ(6, count);

        // 1x forced activation
        sleep_for(milliseconds(100));
        EXPECT_EQ(6, count);

        r.interrupt();
        sleep_for(milliseconds(50));
        EXPECT_EQ(7, count);

        // 2x normal + 1x forced activations
        sleep_for(milliseconds(1200));
        EXPECT_EQ(9, count);

        r.interrupt();
        sleep_for(milliseconds(50));
        EXPECT_EQ(10, count);

        // repeater is destructed here
    }

    EXPECT_EQ(10, count);
}

int main(int argc, char * argv[])
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
