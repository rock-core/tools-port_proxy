#include "Task.hpp"
#include <rtt/rtt-config.h>
#include <rtt/base/PortInterface.hpp>
#include <rtt/types/Types.hpp>
#include <rtt/base/InputPortInterface.hpp>
#include <rtt/plugin/PluginLoader.hpp>

using namespace std;
using base::Time;
using RTT::types::TypeInfo;
using RTT::log;
using RTT::endlog;
using RTT::Error;
using RTT::Info;

namespace  port_proxy
{

struct Task::ConnectionDescription
{
    std::string name;

    RTT::base::DataSourceBase::shared_ptr sample;

    // The port that should be proxied from
    RTT::base::InputPortInterface* read_port;
    // The port that should be written to
    RTT::base::OutputPortInterface* write_port;

    // Periodicity of retrieving the info
    // set from ((TaskContent*) this)->getActivity()->getPeriod()
    unsigned int period_max_count;

    // Allow activation of the reader based on subsampling rate
    // from the main periodicty
    unsigned int period_counter;
};

Task::Task(std::string const& name, TaskCore::TaskState initial_state)
    : TaskBase(name, initial_state)
{
}

Task::Task(std::string const& name, RTT::ExecutionEngine* engine, TaskCore::TaskState initial_state)
    : TaskBase(name, engine, initial_state)
{
}



Task::~Task()
{
}

bool Task::startHook()
{
    return true;
}

void Task::updateHook()
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        // Check on periodicity 
        if(it->period_counter == -1 || it->period_counter++ >= it->period_max_count)
        {
            RTT::FlowStatus status = it->read_port->read(it->sample, (it->period_counter == -1));
            if(status == RTT::NoData)
                continue;

            if(it->period_counter == -1 || status == RTT::NewData)
            {
                it->write_port->write(it->sample);
                it->period_counter = 0;
            }
        }
    }
}

void Task::stopHook()
{
}

bool Task::loadTypekit(std::string const& name)
{
    return RTT::plugin::PluginLoader::Instance()->loadLibrary(name);
}

// report a specific connection.
bool Task::createProxyConnection(const std::string& name, const std::string& type_name, double periodicity, bool keep_last_value) {

    std::string input_port_name("in_" + name);
    std::string output_port_name("out_" + name);

    RTT::base::PortInterface *pi = ports()->getPort(input_port_name);
    if(pi) // we are already having a connection of the given name
    {
        log(Info) << "connection " << name << " is already registered" << endlog();
        // Since the connection already exists, everything is good?! 
        // so could be true here as well
        return false;
    }

    RTT::types::TypeInfoRepository::shared_ptr ti = RTT::types::TypeInfoRepository::Instance();
    RTT::types::TypeInfo* type = ti->type(type_name);
    if (! type)
    {
	cerr << "cannot find " << type_name << " in the type info repository" << endl;
	return false;
    }
    
    RTT::base::InputPortInterface *in_port = type->inputPort(input_port_name);
    RTT::base::OutputPortInterface *out_port = type->outputPort(output_port_name); 
    return addProxyConnection(in_port, out_port, name, periodicity, keep_last_value);
}

bool Task::addProxyConnection(RTT::base::InputPortInterface* in_port, RTT::base::OutputPortInterface* out_port, std::string const& name, double periodicity, bool keep_last_value)
{
    TypeInfo const* type = in_port->getTypeInfo();

    // Add ports to the current task
    ports()->addPort(in_port->getName(), *in_port);
    ports()->addPort(out_port->getName(), *out_port);
    out_port->keepLastWrittenValue(keep_last_value);

    try {
        ConnectionDescription connection;
        connection.name         = name;
        connection.read_port    = in_port;
        connection.write_port   = out_port; 

        connection.sample = type->buildValue();
        
        double taskPeriodicity = this->getActivity()->getPeriod();
        if(periodicity < taskPeriodicity)
        {
            log(Error) << "requested connection periodicity is lower than the core component's one. Either adapt the periodicity of the core component or lower your requirements. " << endlog();
        }
           
        // Currently assuming the core periodicity will be low enough 
        // to support a resolution of 0.1 s
        connection.period_max_count = periodicity / taskPeriodicity;
        connection.period_counter = -1;

        log(Info) << "adding connection period count for '" << name << "' to " << connection.period_max_count << " while having a base period of " << taskPeriodicity << "s" << endlog();

        root.push_back(connection);
    } catch ( RTT::internal::bad_assignment& ba ) {
        return false;
    }
    return true;
}

bool Task::closeProxyConnection(std::string const& name)
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        if ( it->read_port->getName() == name )
        {
            ports()->removePort(it->read_port->getName());
            ports()->removePort(it->write_port->getName());

            delete it->read_port;
            delete it->write_port;
            root.erase(it);
            return true;
        }
    }

    return false;
}

bool Task::closeAllProxyConnection()
{
    std::vector<std::string> names;
    std::vector<std::string>::iterator names_it;

    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        names.push_back(it->name);
    }
    
    for(names_it = names.begin(); names_it != names.end(); ++names_it)
    {
        closeProxyConnection(*names_it);
    }

    return true;
}
} // end of namespace

