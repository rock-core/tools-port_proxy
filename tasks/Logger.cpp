#include "Logger.hpp"
#include <typelib/registry.hh>
#include <rtt/rtt-config.h>
#include <utilmm/configfile/pkgconfig.hh>
#include <utilmm/stringtools.hh>
#include <typelib/pluginmanager.hh>
#include <rtt/EventDrivenActivity.hpp>

#include "Logfile.hpp"
#include <fstream>

using namespace logger;
using namespace std;
using DFKI::Time;
using RTT::TypeInfo;
using RTT::log;
using RTT::endlog;
using RTT::Error;
using RTT::Info;

Logger::Logger(std::string const& name)
    : LoggerBase(name)
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

RTT::EventDrivenActivity* Logger::getEventDrivenActivity() const
{
    return dynamic_cast<RTT::EventDrivenActivity*>(engine()->getActivity());
}

bool Logger::startHook()
{
    if (_file.value().empty())
        return false;

    // The registry has been loaded on construction
    // Now, create the output file
    auto_ptr<ofstream> out_file(new ofstream(_file.value().c_str()));
    Logging::writePrologue(*out_file.get());

    RTT::EventDrivenActivity* activity = getEventDrivenActivity();
    
    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
    {
        RTT::TypeInfo const* type_info = it->source->getTypeInfo();
        std::cerr << type_info->getTypeName() << std::endl;
        it->logger = new Logging::StreamLogger(
                it->name, type_info->getTypeName(), m_registry, *out_file);
    }

    m_file = out_file.release();
    return true;
}

void Logger::updateHook()
{
    // Execute all copies in one shot to do it as fast as possible
    Time stamp = Time::now();
    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
        it->reading_command->execute();

    for (Reports::iterator it = root.begin(); it != root.end(); ++it)
    {
        TypeInfo const* type_info = it->source->getTypeInfo();
        RTT::detail::TypeTransporter* converter = type_info->getProtocol(ORO_UNTYPED_PROTOCOL_ID);
        uint8_t* buffer = reinterpret_cast<uint8_t*>( converter->createBlob(it->dest) );
        it->logger->update(stamp, buffer);
        delete[] buffer;
    }
}

void Logger::stopHook()
{
    delete m_file;
    m_file = 0;
}

bool Logger::reportComponent( const std::string& component, bool peek ) { 
    // Users may add own data sources, so avoid duplicates
    //std::vector<std::string> sources                = comp->data()->getNames();
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
    {
        log(Error) << "Could not report Component " << component <<" : no such peer."<<endlog();
        return false;
    }

    Ports ports   = comp->ports()->getPorts();
    for (Ports::iterator it = ports.begin(); it != ports.end() ; ++it)
        this->reportPort( component, (*it)->getName(), peek );
    return true;
}


bool Logger::unreportComponent( const std::string& component ) {
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
        return false;

    Ports ports   = comp->ports()->getPorts();
    for (Ports::iterator it = ports.begin(); it != ports.end() ; ++it) {
        this->unreportDataSource( component + "." + (*it)->getName() );
        if ( this->ports()->getPort( (*it)->getName() ) ) {
        }
    }
    return true;
}

// report a specific connection.
bool Logger::reportPort(const std::string& component, const std::string& port, bool peek ) {
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
        return false;

    RTT::PortInterface* porti   = comp->ports()->getPort(port);
    if ( !porti )
        return false;

    if ( porti->connected() ) {
        this->reportDataSource( component + "." + port, "Port", porti->connection()->getDataSource(), peek );
    } else {
        // create new port temporarily 
        // this port is only created with the purpose of
        // creating a connection object.
        RTT::PortInterface* ourport = porti->antiClone();
        assert(ourport);

        if ( porti->connectTo( ourport ) == false ) {
            delete ourport;
            return false;
        }

        RTT::EventDrivenActivity* activity = getEventDrivenActivity();
        if (activity)
        {
            log(Info) << "triggering updates when data is available on " << porti->getName() << endlog();
            activity->addEvent( porti->getNewDataEvent() );
        }

        delete ourport;
        this->reportDataSource( component + "." + porti->getName(), "Port", porti->connection()->getDataSource(), peek );
    }
    return true;
}

bool Logger::unreportPort(const std::string& component, const std::string& port ) {
    return this->unreportDataSource( component + "." + port );
}

// report a specific datasource, property,...
bool Logger::reportData(const std::string& component,const std::string& dataname, bool peek) 
{ 
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
        return false;

    // Is it an attribute ?
    if ( comp->attributes()->getValue( dataname ) )
        return this->reportDataSource( component + "." + dataname, "Data",
                comp->attributes()->getValue( dataname )->getDataSource(), peek );
    // Is it a property ?
    if ( comp->properties() && comp->properties()->find( dataname ) )
        return this->reportDataSource( component + "." + dataname, "Data",
                comp->properties()->find( dataname )->getDataSource(), peek );
    return false; 
}

bool Logger::unreportData(const std::string& component,const std::string& datasource) { 
    return this->unreportDataSource( component +"." + datasource); 
}

void Logger::snapshot() {
    // execute the copy commands (fast).
    for(Reports::iterator it = root.begin(); it != root.end(); ++it )
        it->reading_command->execute();
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
        list<string> fields = utilmm::split(*it, " ");
        string name = fields.front();
        if (name.size() > pattern.size() && string(name, name.size() - pattern.size()) == pattern)
        {
            utilmm::pkgconfig pkg(name);
            string tlb = pkg.get("type_registry");
            if (!tlb.empty())
            {
                try {
                    auto_ptr<Typelib::Registry> registry( Typelib::PluginManager::load("tlb", tlb) );
                    m_registry.merge(*registry.get());
                }
                catch(...)
                {
                    std::cerr << "cannot load registry file " << tlb << std::endl;
                }
            }
        }
    }
}

bool Logger::reportDataSource(std::string tag, std::string type, RTT::DataSourceBase::shared_ptr orig, bool peek)
{
    orig->setPeeking(peek);

    if (! orig->getTypeInfo()->getProtocol(ORO_UNTYPED_PROTOCOL_ID))
    {
        std::cerr << "cannot report " << tag << " as its toolkit has not been generated by Orogen" << std::endl;
        return false;
    }

    // creates a copy of the data and an update command to
    // update the copy from the original.
    RTT::DataSourceBase::shared_ptr clone = orig->getTypeInfo()->buildValue();
    if ( !clone )
        return false;
    try {
        boost::shared_ptr<RTT::CommandInterface> comm( clone->updateCommand( orig.get() ) );
        assert( comm );

        ReportDescription report;
        report.name   = tag;
        report.source = orig;
        report.reading_command = comm;
        report.dest   = clone;
        report.kind   = type;
        report.logger = NULL;
        root.push_back(report);
    } catch ( RTT::bad_assignment& ba ) {
        return false;
    }
    return true;
}

bool Logger::unreportDataSource(std::string tag)
{
    for (Reports::iterator it = root.begin();
            it != root.end(); ++it)
        if ( it->name == tag ) {
            root.erase(it);
            return true;
        }
    return false;
}

