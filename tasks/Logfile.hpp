/* 
 * Copyright (c) 2005-2006 LAAS/CNRS <openrobots@laas.fr>
 *	Sylvain Joyeux <sylvain.joyeux@m4x.org>
 *
 * All rights reserved.
 *
 * Redistribution and use  in source  and binary  forms,  with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   1. Redistributions of  source  code must retain the  above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice,  this list of  conditions and the following disclaimer in
 *      the  documentation  and/or  other   materials provided  with  the
 *      distribution.
 *
 * THIS  SOFTWARE IS PROVIDED BY  THE  COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND  ANY  EXPRESS OR IMPLIED  WARRANTIES,  INCLUDING,  BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES  OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR  PURPOSE ARE DISCLAIMED. IN  NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR      CONTRIBUTORS  BE LIABLE FOR   ANY    DIRECT, INDIRECT,
 * INCIDENTAL,  SPECIAL,  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF  SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE   OF THIS SOFTWARE, EVEN   IF ADVISED OF   THE POSSIBILITY OF SUCH
 * DAMAGE.
 */


#ifndef LOGOUTPUT_H
#define LOGOUTPUT_H

#include "Logging.hh"

#include <vector>
//#include <map>
#include <iosfwd>
#include <boost/thread/mutex.hpp>
#include <utilmm/system/endian.hh>

namespace Typelib {
    class Registry;
}
namespace Logging
{
    class Logfile
    {
        std::ostream& m_stream;
        int m_stream_idx;

    public:
        Logfile(std::ostream& stream);
        int newStreamIndex();
        void write(std::vector<uint8_t> const& buffer);
    };

    class OutputStream
    {
        template<class T> friend OutputStream& operator << (OutputStream& output, const T& value);
        static const size_t nstream = static_cast<size_t>(-1);

    private:
        BlockType m_block;
        
        Logfile& m_file;
        size_t m_stream_idx;
        
        void writeInBuffer(const uint8_t* data, size_t data_size);

        template<class T>
        void writeInBuffer(const T& data) 
        { 
	    T little_endian = utilmm::endian::to_little(data);
	    writeInBuffer( reinterpret_cast<const uint8_t*>(&little_endian), sizeof(T) ); 
	}
       
        typedef std::vector<uint8_t> Buffer;
        Buffer m_buffer;

    public:
        OutputStream(Logfile& file);
        ~OutputStream();

        /** Start a new stream */
        void newStream();

        /** Get the stream index */
        size_t  getStreamIndex() const;

        /** Start a new block in the stream
         * The block won't be written until end() is called
         * @arg block_type the block type
         */
        void begin(BlockType block_type);

        /** Ends a block and write it into the log file */
        void end();

        /** Write raw data in the currently opened block */
        void write(const void* data, size_t data_size);
    };

    namespace details
    {
        template <bool value> struct static_check;
        template<> struct static_check<true> {};
    }

    /** Writes the file prologue */
    void writePrologue(std::ostream& stream);

    template<class T>
    OutputStream& operator << (OutputStream& output, const T& value)
    {
        details::static_check<false> test;
        return output;
    }

    template<>
    inline OutputStream& operator << (OutputStream& output, const uint8_t& value)
    { output.writeInBuffer(value); return output; }

    template<>
    inline OutputStream& operator << (OutputStream& output, const uint16_t& value)
    { output.writeInBuffer(value); return output; }

    template<>
    inline OutputStream& operator << (OutputStream& output, const uint32_t& value)
    { output.writeInBuffer(value); return output; }

    template<>
    inline OutputStream& operator << (OutputStream& output, const std::string& value)
    {
        uint32_t length(value.length());
        output.writeInBuffer(length);
        output.writeInBuffer(reinterpret_cast<const uint8_t*>(value.c_str()), length);
        return output;
    }

    template<>
    inline OutputStream& operator << (OutputStream& output, const DFKI::Time& time)
    {
	timeval tv = time.toTimeval();	
	output << (uint32_t)tv.tv_sec << (uint32_t)tv.tv_usec;
        return output;
    }

    template<>
    inline OutputStream& operator << (OutputStream& output, const BlockHeader& header)
    {
	output
	    << header.type
	    << header.padding
	    << header.stream_idx
	    << header.data_size;
	return output;
    }

    template<>
    inline OutputStream& operator << (OutputStream& output, const SampleHeader& header)
    {
	output
	    << header.realtime
	    << header.timestamp
	    << header.data_size
	    << header.compressed;
	return output;
    }

    template<>
    inline OutputStream& operator << (OutputStream& output, const BlockType& type)
    { return output << static_cast<uint8_t>(type); }

    template<>
    inline OutputStream& operator << (OutputStream& output, const StreamType& type)
    { return output << static_cast<uint8_t>(type); }

    template<>
    inline OutputStream& operator << (OutputStream& output, const CommandType& type)
    { return output << static_cast<uint8_t>(type); }


    /** The objects of this class can be used to easily log samples for a given
     * stream.
     */
    class StreamLogger
    {
        std::string const m_name;
        std::string const m_type_name;
        std::string const m_type_def;
        size_t const m_type_size;
        DFKI::Time m_sampling;
        DFKI::Time m_last;

        OutputStream m_stream;

    public:
        /** Create a new logger, with no type definition
         *
         * This logger will define a new named stream in \c stream, but without
         * saving the type definition as the other constructor would.
         *
         * @arg name the stream name
         * @arg type_name the stream type name
         * @arg stream the stream object
         */
        StreamLogger(std::string const& name, std::string const& type_name, Logfile& file);

        /** Create a new logger, with type definition
         *
         * This logger will define a new named stream in \c stream, in which a type
         * definition string is saved, allowing to re-read the log file without
         * having to care about the type definition and (possible) type changes.
         *
         * @arg name the stream name
         * @arg type_name the stream type name
         * @arg registry the Typelib registry which defines the associated type name
         * @arg stream the stream object
         */
        StreamLogger(std::string const& name, std::string const& type_name, Typelib::Registry const& type_def, Logfile& file);

        /** Registers the sample stream in the output file */
        void registerStream();

        /** Sets the sampling period. It is used in update() to filter out
         * samples that are too near from each other
         */
        void setSampling(DFKI::Time const& period);

        /** Write a sample in the stream. If \c size is given, \c data is
         * assumed to be a buffer of this size (in bytes), regardless of the
         * size of the associated type. If size is zero (the default), the
         * size of the type is used. If the type definition has not been given,
         * it is not possible to use a zero size.
         */
        void update(const DFKI::Time& timestamp, void* data, size_t size = 0);
    };
}

#endif

