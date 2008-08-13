#ifndef LOGGER_LOGGERTEST_TASK_HPP
#define LOGGER_LOGGERTEST_TASK_HPP

#include "logger/LoggerTestBase.hpp"

namespace logger {
    class LoggerTest : public LoggerTestBase
    {
	friend class LoggerTestBase;

    protected:
        void updateHook();

    public:
        LoggerTest(std::string const& name = "logger::LoggerTest");
    };
}

#endif

