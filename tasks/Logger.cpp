#include "Logger.hpp"
#include <typelib/registry.hh>
#include <rtt/rtt-config.h>
#include <utilmm/configfile/pkgconfig.hh>
#include <utilmm/stringtools.hh>
#include <typelib/pluginmanager.hh>
#include <rtt/PortInterface.hpp>

#include "Logfile.hpp"
#include <fstream>

using namespace logger;
using namespace std;
using namespace Logging;
using DFKI::Time;
using RTT::TypeInfo;
using RTT::log;
using RTT::endlog;
using RTT::Error;
using RTT::Info;

Logger::Logger(std::string const& name, TaskCore::TaskState initial_state)
    : LoggerBase(name, initial_state)
    , m_file(0)
{
    loadRegistry();

    // Register a fake protocol object to be able to marshal samples
    // "the typelib way"
}

Logger::~Logger()
{
    stop();
}

bool Logger::startHook()
{
    if (_file.value().empty())
        return false;

    // The registry has been loaded on construction
    // Now, create the output file
    auto_ptr<ofstream> io(new ofstream(_file.value().c_str()));
    auto_ptr<Logfile>  file(new Logfile(*io));

    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
    {
        it->read_port->clear();

        RTT::TypeInfo const* type_info = it->read_port->getTypeInfo();
        it->logger = new Logging::StreamLogger(
                it->name, type_info->getTypeName(), m_registry, *file);
    }

    m_io   = io.release();
    m_file = file.release();
    return true;
}

void Logger::updateHook(std::vector<RTT::PortInterface*> const& updated_ports)
{
    // Execute all copies in one shot to do it as fast as possible
    Time stamp = Time::now();
    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
        it->read_command->execute();

    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
    {
        if (find(updated_ports.begin(), updated_ports.end(), it->read_port) != updated_ports.end())
        {
            TypeInfo const* type_info = it->read_port->getTypeInfo();
            RTT::detail::TypeTransporter* converter = type_info->getProtocol(ORO_UNTYPED_PROTOCOL_ID);
            vector<uint8_t>* buffer = reinterpret_cast< vector<uint8_t>* >( converter->createBlob(it->read_source) );
            it->logger->update(stamp, &(*buffer)[0], buffer->size());
            delete buffer;
        }
    }
}

void Logger::stopHook()
{
    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
    {
        delete it->logger;
        it->logger = 0;
    }
    delete m_io;
    m_io = 0;
    delete m_file;
    m_file = 0;
}

bool Logger::reportComponent( const std::string& component ) { 
    // Users may add own data sources, so avoid duplicates
    //std::vector<std::string> sources                = comp->data()->getNames();
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
    {
        log(Error) << "no such component " << component << endlog();
        return false;
    }

    Ports ports   = comp->ports()->getPorts();
    for (Ports::iterator it = ports.begin(); it != ports.end() ; ++it)
        this->reportPort( component, (*it)->getName() );
    return true;
}


bool Logger::unreportComponent( const std::string& component ) {
    TaskContext* comp = this->getPeer(component);
    if (!comp)
    {
        log(Error) << "no such component " << component << endlog();
        return false;
    }

    Ports ports   = comp->ports()->getPorts();
    for (Ports::iterator it = ports.begin(); it != ports.end() ; ++it) {
        this->unreportPort(component, (*it)->getName());
    }
    return true;
}

// report a specific connection.
bool Logger::reportPort(const std::string& component, const std::string& port ) {
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
    {
        log(Error) << "no such component " << component << endlog();
        return false;
    }

    RTT::OutputPortInterface* writer = dynamic_cast<RTT::OutputPortInterface*>(comp->ports()->getPort(port));
    if ( !writer )
    {
        log(Error) << "component " << component << " does not have a port named " << port << ", or it is a read port" << endlog();
        return false;
    }

    PortMap::iterator it = port_map.find(writer);
    if (it != port_map.end()) // we are already reporting this port
    {
        log(Info) << "port " << port << " of component " << component << " is already logged" << endlog();
        return true;
    }

    // Create the corresponding read port
    RTT::InputPortInterface* reader = static_cast<RTT::InputPortInterface*>(writer->antiClone());
    reader->setName(component + "." + port);

    writer->createBufferConnection(*reader, 5);
    ports()->addEventPort(reader);
    it = port_map.insert( make_pair(writer, reader) ).first;

    log(Info) << "triggering updates when data is available on " << writer->getName() << endlog();

    RTT::DataSourceBase::shared_ptr orig = reader->getDataSource();
    if (! orig->getTypeInfo()->getProtocol(ORO_UNTYPED_PROTOCOL_ID))
    {
        log(Error) << "cannot report port " << port << " from component " << component << " as its toolkit has not been generated by Orogen" << endlog();
        return false;
    }

    // creates a copy of the data and an update command to
    // update the copy from the original.
    RTT::DataSourceBase::shared_ptr clone = orig->getTypeInfo()->buildValue();

    try {
        boost::shared_ptr<RTT::CommandInterface> comm( clone->updateCommand( orig.get() ) );
        assert( comm );

        ReportDescription report;
        report.name         = reader->getName();
        report.read_source  = clone;
        report.read_command = comm;
        report.read_port    = reader;
        report.logger       = NULL;
        report.write_port   = writer;
        root.push_back(report);
    } catch ( RTT::bad_assignment& ba ) {
        return false;
    }
    return true;
}

bool Logger::unreportPort(const std::string& component, const std::string& port )
{
    std::string name = component + "." + port;
    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
    {
        if ( it->name == name )
        {
            ports()->removePort(name);
            delete it->read_port;
            port_map.erase(it->write_port);
            root.erase(it);
            return true;
        }
    }
    return false;
}

void Logger::snapshot()
{
    // execute the copy commands (fast).
    for(Reports::iterator it = root.begin(); it != root.end(); ++it )
        it->read_command->execute();
    if( this->engine()->getActivity() )
        this->engine()->getActivity()->trigger();
}

#define TASK_LIBRARY_NAME_PATTERN_aux0(target) #target
#define TASK_LIBRARY_NAME_PATTERN_aux(target) "-toolkit-" TASK_LIBRARY_NAME_PATTERN_aux0(target)
#define TASK_LIBRARY_NAME_PATTERN TASK_LIBRARY_NAME_PATTERN_aux(OROCOS_TARGET)
void Logger::loadRegistry()
{
    string const pattern = TASK_LIBRARY_NAME_PATTERN;

    // List all known toolkits and load the ones that have a .tlb file defined.
    list<string> packages = utilmm::pkgconfig::packages();
    for (list<string>::const_iterator it = packages.begin(); it != packages.end(); ++it)
    {
        if (it->size() > pattern.size() && string(*it, it->size() - pattern.size()) == pattern)
        {
            utilmm::pkgconfig pkg(*it);
            string tlb = pkg.get("type_registry");
            if (!tlb.empty())
            {
                try {
                    auto_ptr<Typelib::Registry> registry( Typelib::PluginManager::load("tlb", tlb) );
                    m_registry.merge(*registry.get());
                    log(Info) << "loaded " << tlb << " in the data logger registry" << endlog();
                }
                catch(...)
                {
                    log(Error) << "cannot load registry file " << tlb << endlog();
                }
            }
        }
    }
}

