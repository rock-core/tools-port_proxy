#ifndef PORT_PROXY_TYPES_HPP
#define PORT_PROXY_TYPES_HPP

#include <string>
#include <stdint.h>

namespace port_proxy
{
    // A proxy connection is a connection to an input or output of a TaskContext port which is proxied by 
    // another TaskContext to improve responsiveness of single threaded processes which does not 
    // want to be blocked by pulling data with a given frequency
    // A proxy connection can also be used to monitor a TaskContext without waiting for a corba timeout
    // if the TaskContext is unresponsive
    //
    // The connection policy is always DATA, LOCK_FREE, PULLED
    struct ProxyConnection
    {
        ProxyConnection()
        {
            periodicity = 0.1;
            check_periodicity = 1.0;
            keep_last_value = false;
        }

        // name of the task those port shall be proxied 
        std::string task_name;
        
        // name of the port which shall be proxied 
        std::string port_name;

        // type name if of port sample
        std::string type_name;

        // read / write update periodicity
        double periodicity;

        // periodicity for checking data connection if no new samples are arriving
        double check_periodicity;

        // keeps the last value 
        bool keep_last_value;
    };
}

#endif
