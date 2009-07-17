#ifndef LOGGER_LOGGER_TASK_HPP
#define LOGGER_LOGGER_TASK_HPP

#include "logger/LoggerBase.hpp"
#include <typelib/registry.hh>

namespace Logging {
    class StreamLogger;
    class Logfile;
}
namespace RTT {
    class InputPortInterface;
    class OutputPortInterface;
}

namespace logger {
    class Logger : public LoggerBase
    {
	friend class LoggerBase;
        static const int ORO_UNTYPED_PROTOCOL_ID = 42;

        typedef std::map<RTT::OutputPortInterface*, RTT::InputPortInterface*> PortMap;
        PortMap port_map;

    protected:

        Typelib::Registry m_registry;
        std::ofstream*    m_io;
        Logging::Logfile* m_file;
    
        bool startHook();
        void updateHook(std::vector<RTT::PortInterface*> const& updated_ports);
        void stopHook();

    public:
        Logger(std::string const& name = "logger::Logger", TaskCore::TaskState initial_state = Stopped);
        ~Logger();

        /**
         * Report all the data ports of a component.
         */
        bool reportComponent( const std::string& component );
            
        /**
         * Unreport the data ports of a component.
         */
        bool unreportComponent( const std::string& component );

        /**
         * Report a specific data port of a component.
         */
        bool reportPort(const std::string& component, const std::string& port );

        /**
         * Unreport a specific data port of a component.
         */
        bool unreportPort(const std::string& component, const std::string& port );

        /**
         * This real-time function makes copies of the data to be
         * reported.
         */
        void snapshot();

    private:
        typedef RTT::DataFlowInterface::Ports Ports;

        struct ReportDescription {
            std::string name;
            RTT::DataSourceBase::shared_ptr read_source;
            boost::shared_ptr<RTT::CommandInterface> read_command;
            RTT::InputPortInterface* read_port;
            Logging::StreamLogger* logger;
            RTT::OutputPortInterface* write_port;
            std::vector<uint8_t> buffer;
        };

        /**
         * Stores the 'datasource' of all reported items as properties.
         */
        typedef std::vector<ReportDescription> Reports;
        Reports root;

        void loadRegistry();
    };
}

#endif

