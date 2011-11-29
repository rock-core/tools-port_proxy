require 'orocos/test/component'
require 'pp'

require 'pocolog'

class TC_BasicBehaviour < Test::Unit::TestCase
    include Orocos::Test::Component
    run 'task', 'rock_logger', 'logger'

    def setup
        super
        task.file = logfile_path
    end

    def logfile_path
        "rock_logger_test.0.log"
    end

    def logfile
        @logfile ||= Pocolog::Logfiles.open(logfile_path)
    end

    def test_basics
        assert(!task.has_port?('time'))
        assert(task.createLoggingPort('time', '/base/Time'))
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
        return

        samples = logfile.stream('time').samples.to_a.map(&:last)
        assert_equal(expected, samples)
    end

    def test_metadata
    end

    def test_annotation_stream
    end
end

