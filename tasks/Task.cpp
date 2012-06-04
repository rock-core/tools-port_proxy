#include "Task.hpp"
#include <rtt/rtt-config.h>
#include <rtt/base/PortInterface.hpp>
#include <rtt/types/Types.hpp>
#include <rtt/base/InputPortInterface.hpp>
#include <rtt/plugin/PluginLoader.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>

using namespace std;
using base::Time;
using RTT::types::TypeInfo;
using RTT::log;
using RTT::endlog;
using RTT::Error;
using RTT::Info;
using RTT::Debug;


namespace  port_proxy
{

struct Task::ConnectionDescription
{
    std::string task_name;
    std::string port_name;
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

    // Allow checking the connection of the reader based on subsampling rate
    // from reader activity
    unsigned int check_period_max_count;
    unsigned int check_period_counter;

    // Allow checking the connection to the consumer on subsampling rate
    // from the main periodicty
    // if delete_period_max_count is reached and no one is connected to the write_port
    // the proxy connection will be closed
    unsigned int delete_period_max_count;
    unsigned int delete_period_counter;

    bool was_connected;
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

bool Task::isProxingPort(std::string const& task_name,std::string const& port_name)
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        if (it->task_name == task_name && it->port_name == port_name)
            return true;
    }
    return false;
}

std::string Task::getInputPortName(std::string const& task_name,std::string const& port_name)
{
    std::string input_port_name("in_" + task_name + '_' + port_name);
    return input_port_name;
}

std::string Task::getOutputPortName(std::string const& task_name,std::string const& port_name)
{
    std::string output_port_name("out_" + task_name + '_' + port_name);
    return output_port_name;
}

bool Task::isConnected(::std::string const & task_name, ::std::string const & port_name)
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        if (it->task_name == task_name && it->port_name == port_name)
            return it->read_port->connected();
    }
    return false;
}

bool Task::checkProxyConnection(std::string const & task_name, std::string const & port_name)
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        if ( it->task_name == task_name && it->port_name == port_name )
            return checkProxyConnection(*it);
    }
    return false;
}

bool Task::checkProxyConnection(ConnectionDescription &connection)
{
    log(Debug) << "checking connection to " << connection.task_name << "." << connection.port_name <<  endlog();
    if(connection.read_port->connected())
    {
        connection.was_connected = true;
        return true;
    }

    if(connection.was_connected == true)
    {
        connection.was_connected = false;
        connection.write_port->disconnect();
    }

    //get remote task
    bool result = false;
    try 
    {
        RTT::TaskContext* remote_task = RTT::corba::TaskContextProxy::Create(connection.task_name);
        if(!remote_task)
        {
            log(Debug) << "no task named " << connection.task_name << " could be found. Keep on trying ..." <<  endlog();
            return false;
        }
        log(Debug) << "got remote task " << connection.task_name << endlog();

        //get remote port
        RTT::base::PortInterface *remote_port = remote_task->getPort(connection.port_name);
        if(!remote_port)
        {
            log(Debug) << "remote task " << connection.task_name << " has no port called. Dynamic port? " << connection.port_name << endlog();
            delete remote_task;
            return false;
        }

        //set up connection
        RTT::ConnPolicy policy = RTT::ConnPolicy::data(RTT::ConnPolicy::LOCK_FREE,true,true);
        result = remote_port->connectTo(connection.read_port,policy);
        if(result)
        {
            log(Info) << "set up connection between " << connection.task_name << "." << connection.port_name;
            log(Info) <<  " and " << connection.read_port->getName() << endlog();
        }
        else
        {
            log(Error) << "setting up connection between " << connection.task_name << "." << connection.port_name;
            log(Error) <<  " and " << connection.read_port->getName() << " failed" << endlog();
        }
    }
    catch(...)
    {}
    return result;
}

void Task::updateHook()
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        //check if someone is connected
        if(it->delete_period_counter++ >= it->delete_period_max_count)
        {
            it->delete_period_counter = 0;
            if(!it->write_port->connected())
            {
                closeProxyConnection(it->task_name,it->port_name);
                break; // we have to break here because it is no longer valid
            }
        }

        // Check on periodicity 
        if(it->period_counter++ < it->period_max_count)
            continue;
        
        // read new data
        it->period_counter = 0;
        if(it->write_port->connected() && it->read_port->read(it->sample, false) == RTT::NewData)
        {
            it->write_port->write(it->sample);
            it->check_period_counter = 0;
            continue;
        }

        // Check connection on periodicity if no data are available
        if(it->check_period_counter++ >= it->check_period_max_count)
        {
            it->check_period_counter = 0;
            checkProxyConnection(*it);
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


bool Task::createProxyConnection(::port_proxy::ProxyConnection const & proxy_connection)
{
    std::string input_port_name = getInputPortName(proxy_connection.task_name,proxy_connection.port_name);
    std::string output_port_name = getOutputPortName(proxy_connection.task_name,proxy_connection.port_name);

    RTT::base::PortInterface *pi = ports()->getPort(input_port_name);
    if(pi)
    {
        // Since the connection already exists, everything is good?! 
        log(Info) << "connection " << proxy_connection.task_name << "." << proxy_connection.port_name << " is already registered" << endlog();
        return true;
    }

    RTT::types::TypeInfoRepository::shared_ptr ti = RTT::types::TypeInfoRepository::Instance();
    RTT::types::TypeInfo* type = ti->type(proxy_connection.type_name);
    if (!type)
    {
	log(Error) << "cannot find " << proxy_connection.type_name << " in the type info repository" << endlog();
	return false;
    }
    
    // Add ports to the current task
    RTT::base::InputPortInterface *in_port = type->inputPort(input_port_name);
    RTT::base::OutputPortInterface *out_port = type->outputPort(output_port_name); 
    out_port->keepLastWrittenValue(proxy_connection.keep_last_value);
    try 
    {
        ConnectionDescription connection;
        connection.task_name = proxy_connection.task_name;
        connection.port_name = proxy_connection.port_name;
        connection.read_port    = in_port;
        connection.write_port   = out_port; 
        connection.was_connected = false;

        TypeInfo const* type = in_port->getTypeInfo();
        connection.sample = type->buildValue();
        
        double taskPeriodicity = this->getActivity()->getPeriod();
        if(proxy_connection.periodicity < taskPeriodicity)
        {
            log(Error) << "requested connection periodicity " << proxy_connection.periodicity;
            log(Error) << " is lower than the core component's one "<< taskPeriodicity << endlog();
            log(Error) << "Either adapt the periodicity of the core component or lower";
            log(Error) << " your requirements. " << endlog();
            return false;
        }
           
        // Currently assuming the core periodicity will be low enough 
        // to support a resolution of 0.1 s
        connection.period_max_count = proxy_connection.periodicity / taskPeriodicity;
        connection.period_counter = connection.period_max_count;

        connection.delete_period_max_count = _timeout.get()/taskPeriodicity;
        connection.delete_period_counter = 0;
        connection.check_period_max_count = proxy_connection.check_periodicity / taskPeriodicity;
        connection.check_period_counter = connection.period_max_count;
        if(connection.check_period_max_count < connection.period_max_count)
        {
            log(Error) << "requested connection check periodicity is lower than the connection periodicity. Either adapt the periodicity or lower your requirements. " << endlog();
            return false;
        }
        // To save bandwith we only check if no data are available on the port 
        // therefore check_period_max_count must be a multiple of period_max_count
        connection.check_period_max_count = connection.check_period_max_count / connection.period_max_count;

        log(Info) << "adding connection period count for '" << proxy_connection.type_name << "." << proxy_connection.port_name;
        log(Info) << "' to " << connection.period_max_count << " while having a base period of " << taskPeriodicity << "s" << endlog();
        ports()->addPort(in_port->getName(), *in_port);
        ports()->addPort(out_port->getName(), *out_port);
        root.push_back(connection);
    } 
    catch ( RTT::internal::bad_assignment& ba ) 
    {
        log(Error) << "RTT::internal::bad_assignment: cannot create proxy connection " << input_port_name << endlog();
        return false;
    }
    return true;
}

bool Task::closeProxyConnection(std::string const& task_name,std::string const& port_name)
{
    for (Connections::iterator it = root.begin(); it != root.end(); ++it)
    {
        if(it->task_name == task_name && (it->port_name == port_name || port_name.empty()))
        {
            ports()->removePort(it->read_port->getName());
            ports()->removePort(it->write_port->getName());
            delete it->read_port;
            delete it->write_port;
            root.erase(it);
            if(port_name.empty())
                return closeProxyConnection(task_name,"");
            else
                return true;
        }
    }
    return false;
}

bool Task::closeAllProxyConnections()
{
    while(!root.empty())
        closeProxyConnection(root.front().task_name,root.front().port_name);
    return true;
}
} // end of namespace

