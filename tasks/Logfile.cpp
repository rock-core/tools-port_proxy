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


#include "Logfile.hpp"
#include <sstream>
#include <cassert>
#include <boost/static_assert.hpp>

#include <typelib/registry.hh>
#include <typelib/pluginmanager.hh>

using namespace std;
using boost::mutex;
namespace endian = utilmm::endian;

BOOST_STATIC_ASSERT(( sizeof(Logging::Prologue) == 16 ));

const char Logging::FORMAT_MAGIC[] = "POCOSIM";

void Logging::writePrologue(std::ostream& stream)
{
    Prologue prologue;
    prologue.version    = endian::to_little<uint32_t>(Logging::FORMAT_VERSION);
#if defined(WORDS_BIGENDIAN)
    prologue.flags = 1;
#else
    prologue.flags = 0;
#endif

    stream.write(reinterpret_cast<char*>(&prologue), sizeof(prologue));
}


namespace Logging
{
    boost::mutex  OutputStream::s_mtx_static;
    size_t OutputStream::s_next_index = 0;

    OutputStream::OutputStream(std::ostream& stream)
        : m_block(UnknownBlockType), m_stream(stream), m_stream_idx(nstream) { }
    OutputStream::~OutputStream() {}

    void   OutputStream::newStream() { m_stream_idx = newStreamIndex(); }
    size_t OutputStream::getStreamIndex() const { return m_stream_idx; }

    void OutputStream::begin(BlockType block_type)
    {
        assert(m_block == UnknownBlockType);
        assert(m_stream_idx != nstream);

        m_buffer.clear();
        m_block = block_type;
        BlockHeader block_header = { block_type, 0xFF, m_stream_idx, 0 };
	*this << block_header;
    }

    void OutputStream::end()
    {
        assert(m_block != UnknownBlockType);

        long data_size = m_buffer.size() - sizeof(BlockHeader);
        reinterpret_cast<BlockHeader*>(&m_buffer[0]) -> data_size = endian::to_little<uint32_t>(data_size);
        m_stream.write(reinterpret_cast<const char*>(&m_buffer[0]), m_buffer.size());
        m_block = UnknownBlockType;
    }

    size_t OutputStream::newStreamIndex()
    { 
        mutex::scoped_lock staticlock(s_mtx_static);
        return s_next_index++;
    }

    void OutputStream::writeInBuffer(const uint8_t* data, size_t data_size)
    {
        assert(m_block != UnknownBlockType);

        int begin = m_buffer.size();
        m_buffer.resize(m_buffer.size() + data_size);
        memcpy(&m_buffer[begin], data, data_size);
    }

    void OutputStream::write(const void* data, size_t data_size)
    { writeInBuffer( static_cast<const uint8_t*>(data), data_size ); }













    StreamLogger::StreamLogger(std::string const& name, const std::string& type_name, std::ostream& stream)
        : m_name(name), m_type_name(type_name)
        , m_type_def()
        , m_type_size(0), m_stream(stream)
    { 
        m_stream.newStream();
        registerStream();
    }

    static size_t getTypeSize(Typelib::Registry const& registry, std::string const& name)
    {
        Typelib::Type const* type = registry.get(name);
        if (type)
            return type->getSize();
        return 0;
    }

    StreamLogger::StreamLogger(std::string const& name, const std::string& type_name, Typelib::Registry const& registry, std::ostream& stream)
        : m_name(name), m_type_name(type_name)
        , m_type_def(Typelib::PluginManager::save("tlb", registry))
        , m_type_size(getTypeSize(registry, type_name)), m_stream(stream)
    {
        m_stream.newStream();
        registerStream();
    }

    void StreamLogger::setSampling(DFKI::Time const& period)
    { m_sampling = period; }


    void StreamLogger::registerStream()
    {

        m_stream.begin( StreamBlockType );
            m_stream 
                << DataStreamType
                << m_name
                << m_type_name;
            if (! m_type_def.empty())
               m_stream << m_type_def;
        m_stream.end();
    }

    void StreamLogger::update(const DFKI::Time& timestamp, void* data, size_t size)
    {
        if (!m_last.isNull() && !m_sampling.isNull() && (timestamp - m_last) < m_sampling)
            return;

        if (size == 0)
            size = m_type_size;

        SampleHeader header = { DFKI::Time::now(), timestamp, size, 0 };
        m_stream.begin( DataBlockType );
	    m_stream << header;
            m_stream.write(data, size);
        m_stream.end();

        m_last = timestamp;
    }
}


