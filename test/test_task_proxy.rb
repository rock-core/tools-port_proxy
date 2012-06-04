require 'test/unit'
require 'orocos'
Orocos.initialize

class TaskProxyTest < Test::Unit::TestCase
    def setup
    end

    def test_creatingProxyConnection
        Orocos.run "port_proxy::Task" => "port_proxy" ,:output => "%m.log" do 
            task = Orocos::TaskContext.get "port_proxy"
            connection = Types::PortProxy::ProxyConnection.new
            connection.task_name = "test_task"
            connection.port_name = "test_port"
            connection.type_name = "/base/Time"
            connection.periodicity = 0.1
            connection.check_periodicity = 1
            connection.keep_last_value = false
            task.start
            assert(task.createProxyConnection(connection))
            assert(task.has_port? "in_test_task_test_port")
            assert(task.has_port? "out_test_task_test_port")
            assert(task.has_port? task.getInputPortName("test_task","test_port"))
            assert(task.has_port? task.getOutputPortName("test_task","test_port"))
            reader = task.out_test_task_test_port.reader(:init => false)
            writer = task.in_test_task_test_port.writer
            assert(task.isConnected("test_task","test_port"))
            assert(task.isProxingPort("test_task","test_port"))
            writer.write Time.now
            sleep(0.2)
            assert(reader.read)
            assert(!reader.read_new)
            assert(task.closeProxyConnection("test_task","test_port"))
            assert(!task.isConnected("test_task","test_port"))
            assert(!task.has_port?("in_test_task_test_port"))
            assert(!task.has_port?("out_test_task_test_port"))
        end
    end

    #a proxy connection should be automatically deleted if no consumer 
    #is connected for longer than timout 
    def test_AutoDeleteConnection
        Orocos.run "port_proxy::Task" => "port_proxy" ,:output => "%m.log" do 
            task = Orocos::TaskContext.get "port_proxy"
            task.timeout = 1.0
            connection = Types::PortProxy::ProxyConnection.new
            connection.task_name = "test_task"
            connection.port_name = "test_port"
            connection.type_name = "/base/Time"
            connection.periodicity = 0.1
            connection.check_periodicity = 1
            task.start
            assert(task.createProxyConnection(connection))
            assert(task.has_port? "in_test_task_test_port")
            assert(task.has_port? "out_test_task_test_port")
            reader = task.out_test_task_test_port.reader
            writer = task.in_test_task_test_port.writer
            assert(task.isConnected("test_task","test_port"))
            writer.write Time.now 
            sleep(0.2)
            assert(reader.read)
            task.out_test_task_test_port.disconnect_all
            sleep(1.1)
            assert(!task.isConnected("test_task","test_port"))
            assert(!task.has_port?("in_test_task_test_port"))
            assert(!task.has_port?("out_test_task_test_port"))
        end
    end

    #a proxy connection should automatically connect to the given task and port
    #as soon the given task and port is reachable 
    def test_AutoReconnect
        Orocos.run "port_proxy::Task" => "port_proxy",:output => "%m.log" do 
            #setup port porxy 
            task = Orocos::TaskContext.get "port_proxy"
            connection = Types::PortProxy::ProxyConnection.new
            connection.task_name = "test_task"
            connection.port_name = "out_dummy_port"
            connection.type_name = "/base/Time"
            connection.periodicity = 0.1
            connection.check_periodicity = 0.2
            task.start
            assert(task.createProxyConnection(connection))
            assert(task.has_port? "in_test_task_out_dummy_port")
            assert(task.has_port? "out_test_task_out_dummy_port")

            reader = task.out_test_task_out_dummy_port.reader
            assert(!reader.read)

            Orocos.run "port_proxy::Task" => "test_task",:output => "%m2.log" do 
                #setup source
                connection.task_name = "dummy"
                connection.port_name = "port"
                connection.type_name = "/base/Time"
                connection.periodicity = 0.1
                connection.check_periodicity = 1
                task2 = Orocos::TaskContext.get "test_task"
                task2.start
                #make sure that the port proxy is discovering the right task but no port
                sleep(0.5)    
                assert(task2.createProxyConnection(connection))
                assert(task2.has_port? "out_dummy_port")
                assert(task2.has_port? "in_dummy_port")

                #check connection
                sleep(0.5)
                assert(task.isConnected("test_task","out_dummy_port"))
                writer = task2.in_dummy_port.writer
                writer.write Time.now 
                sleep(0.3)
                assert(reader.read)
            end
            #now the source is killed 
            assert(reader.connected?)
            assert(!task.isConnected("test_task","out_dummy_port"))

            #check if the port proxy can recover after the task is reachable again
            Orocos.run "port_proxy::Task" => "test_task",:output => "%m3.log" do 
                task2 = Orocos::TaskContext.get "test_task"
                task2.start
                assert(task2.createProxyConnection(connection))
                assert(task2.has_port? "out_dummy_port")
                assert(task2.has_port? "in_dummy_port")

                #check connection
                sleep(0.5)
                assert(task.isConnected("test_task","out_dummy_port"))
                writer = task2.in_dummy_port.writer
                time = Time.now
                writer.write time
                sleep(0.3)
                assert_equal(time,reader.read)
            end
            assert(reader.connected?)
            assert(!task.isConnected("test_task","out_dummy_port"))
        end
    end
end
