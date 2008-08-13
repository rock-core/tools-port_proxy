#include "LoggerTest.hpp"

using namespace logger;

LoggerTest::LoggerTest(std::string const& name)
    : LoggerTestBase(name) {}


void LoggerTest::updateHook()
{
    DFKI::Time t = DFKI::Time::now();
    _time.Set(t);
}





