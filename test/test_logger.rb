require 'orocos/test/component'
require 'pp'

require 'pocolog'
require 'fileutils'

class TC_BasicBehaviour < Test::Unit::TestCase
    include Orocos::Test::Component
    start 'task', 'rock_logger', 'logger'

    def setup
        super
        task.file = logfile_path
    end

    def teardown
        path = task.file
        super
        if !path.empty?
            FileUtils.rm_f(path.gsub(/\.idx$/, ".log"))
        end
    end

    def logfile_path
        @logfile_io ||= Tempfile.open('rock_logger_test.0.log')
        @logfile_io.path
    end

    def logfile
        @logfile ||= Pocolog::Logfiles.open(logfile_path)
    end

    def generate_and_check_logfile
        assert(task.has_port?('time'))

        task.configure
        task.start

        writer = task.time.writer(:type => :buffer, :size => 2000)
        expected = []
        10.times do |i|
            expected << Time.at(i)
            writer.write(expected.last)
        end
        sleep 0.1

        # Make sure the I/O is properly flushed
        task.stop
        task.cleanup

        stream = logfile.stream('time')
        samples = stream.samples.to_a.map(&:last)
        assert_equal(expected, samples)
        stream
    end

    def test_basics
        assert(!task.has_port?('time'))
        assert(task.createLoggingPort('time', '/base/Time', []))
        generate_and_check_logfile
    end

    def test_metadata
        assert(!task.has_port?('time'))
        meta = []
        meta << Hash['key' => 'key0', 'value' => 'value0']
        meta << Hash['key' => 'key1', 'value' => 'value1']
        assert(task.createLoggingPort('time', '/base/Time', meta))
        stream = generate_and_check_logfile
        assert_equal({'key0' => 'value0', 'key1' => 'value1'}, stream.metadata)
    end

    def test_create_port_log
        # Use a logger as source task. Just simply use the state port
        source = start('rock_logger', 'source_logger', 'source')

        task.create_log(source.port('state'))
        assert(task.has_port?('source_logger.state'))
        task.configure
        task.start
        task.stop

        logfile = Pocolog::Logfiles.open(logfile_path)
        stream = logfile.stream('source_logger.state')
        expected_metadata = {
            'rock_stream_type' => 'port',
            'rock_task_model' => 'logger::Logger',
            'rock_task_name' => 'source_logger',
            'rock_task_object_name' => 'state'
        }
        assert_equal expected_metadata, stream.metadata
    end

    def test_log_port
        # Use a logger as source task. Just simply use the state port
        source = start('rock_logger', 'source_logger', 'source')
        task.log(source.port('state'))
        assert(task.has_port?('source_logger.state'))
        task.configure
        task.start

        source_io = Tempfile.open('rock_logger_test_source.0.log')
        source.file = source_io.path
        source.start
        source.stop
        sleep 0.1

        task.stop

        logfile = Pocolog::Logfiles.open(logfile_path)
        stream = logfile.stream('source_logger.state')
        expected_metadata = {
            'rock_stream_type' => 'port',
            'rock_task_model' => 'logger::Logger',
            'rock_task_name' => 'source_logger',
            'rock_task_object_name' => 'state'
        }
        assert_equal expected_metadata, stream.metadata

        assert_equal [5, 4], stream.samples.map(&:last)
    end

    def test_create_property_log
        # Use a logger as source task. Just simply use the state port
        source = start('rock_logger', 'source_logger', 'source')

        task.create_log(source.property('file'))
        assert(task.has_port?('source_logger.file'))
        task.configure
        task.start
        task.stop

        logfile = Pocolog::Logfiles.open(logfile_path)
        stream = logfile.stream('source_logger.file')
        expected_metadata = {
            'rock_stream_type' => 'property',
            'rock_task_model' => 'logger::Logger',
            'rock_task_name' => 'source_logger',
            'rock_task_object_name' => 'file'
        }
        assert_equal expected_metadata, stream.metadata
    end

    def test_log_property
        # Use a logger as source task. Just simply use the state port
        source = start('rock_logger', 'source_logger', 'source')
        task.log(source.property('file'))
        assert(task.has_port?('source_logger.file'))
        task.configure
        task.start

        source.file = "bla.0.log"
        source.start
        source.stop
        sleep 0.1

        task.stop

        logfile = Pocolog::Logfiles.open(logfile_path)
        stream = logfile.stream('source_logger.file')
        expected_metadata = {
            'rock_stream_type' => 'property',
            'rock_task_model' => 'logger::Logger',
            'rock_task_name' => 'source_logger',
            'rock_task_object_name' => 'file'
        }
        assert_equal expected_metadata, stream.metadata
        assert_equal ["", "bla.0.log"], stream.samples.map(&:last)
    end
end

