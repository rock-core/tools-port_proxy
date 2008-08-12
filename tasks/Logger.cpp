#include "Logger.hpp"
#include <typelib/registry.hh>
#include <rtt/rtt-config.h>
#include <utilmm/configfile/pkgconfig.hh>
#include <utilmm/stringtools.hh>
#include <typelib/pluginmanager.hh>

using namespace logger;
using namespace std;

Logger::Logger(std::string const& name)
    : LoggerBase(name)
{
    loadRegistry();
}

bool Logger::reportComponent( const std::string& component, bool peek ) { 
    // Users may add own data sources, so avoid duplicates
    //std::vector<std::string> sources                = comp->data()->getNames();
    TaskContext* comp = this->getPeer(component);
    if ( !comp )
        return false;

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
        (it->get<2>())->execute();
    if( this->engine()->getActivity() )
        this->engine()->getActivity()->trigger();
}

#define TASK_LIBRARY_NAME_PATTERN_aux0(target) #target
#define TASK_LIBRARY_NAME_PATTERN_aux(target) "-toolkit-" TASK_LIBRARY_NAME_PATTERN_aux0(target)
#define TASK_LIBRARY_NAME_PATTERN TASK_LIBRARY_NAME_PATTERN_aux(OROCOS_TARGET)
void Logger::loadRegistry()
{
    string const pattern = TASK_LIBRARY_NAME_PATTERN;

    std::cerr << "pattern: " << pattern << endl;
    // List all known toolkits and load the ones that have a .tlb file defined.
    list<string> packages = utilmm::pkgconfig::packages();
    for (list<string>::const_iterator it = packages.begin(); it != packages.end(); ++it)
    {
        list<string> fields = utilmm::split(*it, " ");
        string name = fields.front();
        if (name.size() > pattern.size() && string(name, name.size() - pattern.size()) == pattern)
        {
            utilmm::pkgconfig pkg(name);
            string tlb_file   = pkg.get("prefix") + "/share/orogen/" + name + ".tlb";
            auto_ptr<Typelib::Registry> registry( Typelib::PluginManager::load("tlb", tlb_file) );
            m_registry.merge(*registry.get());
        }
    }
}

bool Logger::reportDataSource(std::string tag, std::string type, RTT::DataSourceBase::shared_ptr orig, bool peek)
{
    orig->setPeeking(peek);

    // creates a copy of the data and an update command to
    // update the copy from the original.
    RTT::DataSourceBase::shared_ptr clone = orig->getTypeInfo()->buildValue();
    if ( !clone )
        return false;
    try {
        boost::shared_ptr<RTT::CommandInterface> comm( clone->updateCommand( orig.get() ) );
        assert( comm );
        root.push_back( boost::make_tuple( tag, orig, comm, clone, type, (logger::LogStream*)NULL ) );
    } catch ( RTT::bad_assignment& ba ) {
        return false;
    }
    return true;
}

bool Logger::unreportDataSource(std::string tag)
{
    for (Reports::iterator it = root.begin();
            it != root.end(); ++it)
        if ( it->get<0>() == tag ) {
            root.erase(it);
            return true;
        }
    return false;
}

