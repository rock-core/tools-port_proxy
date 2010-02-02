#include "LoggerTest.hpp"

using namespace logger;

LoggerTest::LoggerTest(std::string const& name, TaskCore::TaskState initial_state)
    : LoggerTestBase(name, initial_state)
{
    range.start_angle = 0;
    range.angular_resolution = 0;
    range.speed = 0;
}


void LoggerTest::updateHook()
{
    base::Time t = base::Time::now();
    _time.write(t);

    //generate a fake reading every 10 ms
    if (range.time - t > base::Time(0, 10000))
    {
	//juste some garbadge scan
	range.start_angle += 0.1;
	range.time = t;
        range.angular_resolution +=0.1;
        range.speed += 0.2;
	int scan_count = t.seconds % 1000;
        range.ranges.resize(scan_count);
        for (int i = 0; i < scan_count; ++i)
            range.ranges[i] = scan_count + i;
        _scans.write(range);
    }
        
}





