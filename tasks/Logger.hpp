#ifndef LOGGER_LOGGER_TASK_HPP
#define LOGGER_LOGGER_TASK_HPP

#include "logger/LoggerBase.hpp"
#include <typelib/registry.hh>

namespace Logging {
    class StreamLogger;
}

namespace logger {
    class Logger : public LoggerBase
    {
	friend class LoggerBase;
        static const int ORO_UNTYPED_PROTOCOL_ID = 42;

    protected:

        Typelib::Registry m_registry;
        std::ofstream*    m_file;
    
        bool startHook();
        void updateHook();
        void stopHook();

    public:
        Logger(std::string const& name = "logger::Logger");
        ~Logger();

        /**
         * Report all the data ports of a component.
         */
        bool reportComponent( const std::string& component, bool peek = true );
            
        /**
         * Unreport the data ports of a component.
         */
        bool unreportComponent( const std::string& component );

        /**
         * Report a specific data port of a component.
         */
        bool reportPort(const std::string& component, const std::string& port, bool peek = true );

        /**
         * Unreport a specific data port of a component.
         */
        bool unreportPort(const std::string& component, const std::string& port );

        /**
         * Report a specific data source of a component.
         */
        bool reportData(const std::string& component,const std::string& dataname, bool peek = true);

        /**
         * Unreport a specific data source of a component.
         */
        bool unreportData(const std::string& component,const std::string& datasource);

        /**
         * This real-time function makes copies of the data to be
         * reported.
         */
        void snapshot();

    private:
        typedef RTT::DataFlowInterface::Ports Ports;

        struct ReportDescription {
            std::string name;
            RTT::DataSourceBase::shared_ptr source;
            boost::shared_ptr<RTT::CommandInterface> reading_command;
            RTT::DataSourceBase::shared_ptr dest;
            std::string kind;
            Logging::StreamLogger* logger;
        };

        /**
         * Stores the 'datasource' of all reported items as properties.
         */
        typedef std::vector<ReportDescription> Reports;
        Reports root;

        void loadRegistry();

        bool reportDataSource(std::string tag, std::string type, RTT::DataSourceBase::shared_ptr orig, bool peek = true);

        bool unreportDataSource(std::string tag);
    };
}

#endif

