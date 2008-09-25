#include "LoggerTest.hpp"

using namespace logger;

LoggerTest::LoggerTest(std::string const& name)
    : LoggerTestBase(name)
{
    range.min = 0;
    range.resolution = 0;
    range.speed = 0;
}


void LoggerTest::updateHook()
{
    DFKI::Time t = DFKI::Time::now();
    _time.Set(t);

    range.stamp = t;
    range.min += 1;
    if (range.min % 5 == 0)
    {
        range.resolution += 2;
        range.speed += 3;
        range.ranges.resize(range.min);
        for (int i = 0; i < range.min; ++i)
            range.ranges[i] = range.min + i;
        _scans.Set(range);
    }
        
}





