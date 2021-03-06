/*
 * Copyright 2017 Valve Software
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#define INVALID_ID ( ( uint32_t )-1 )

inline bool is_valid_id( uint32_t id )
{
    return ( id != INVALID_ID );
}

template < typename K, typename V >
class util_umap
{
public:
    util_umap() {}
    ~util_umap() {}

    V *get_val( const K key, const V &defval )
    {
        auto res = m_map.emplace( key, defval );
        return &res.first->second;
    }

    V *get_val( const K key )
    {
        auto i = m_map.find( key );
        if ( i != m_map.end() )
            return &i->second;
        return NULL;
    }

    void set_val( const K key, const V &val )
    {
        auto res = m_map.emplace( key, val );

       /*
        * If the insertion takes place (because no other element existed with the
        * same key), the function returns a pair object, whose first component is an
        * iterator to the inserted element, and whose second component is true.
        *
        * Otherwise, the pair object returned has as first component an iterator
        * pointing to the element in the container with the same key, and false as its
        * second component.
        */
        if ( !res.second )
            res.first->second = val;
    }

public:
    typedef std::unordered_map< K, V > map_t;
    map_t m_map;
};

class StrPool
{
public:
    StrPool() {}
    ~StrPool() {}

    const char *getstr( const char *str, size_t len = ( size_t )-1 );
    const char *findstr( uint32_t hashval );

public:
    util_umap< uint32_t, std::string > m_pool;
};

struct trace_info_t
{
    uint32_t cpus = 0;
    std::string file;
    std::string uname;
    bool timestamp_in_us;
    std::vector< std::string > cpustats;

    // Map tgid to vector of child pids
    util_umap< int, std::vector< int > > tgid_pids;
    // Map pid to tgid
    util_umap< int, int > pid_tgid_map;
    // Map pid to comm
    util_umap< int, const char * > pid_comm_map;
};

struct event_field_t
{
    const char *key;
    const char *value;
};

enum trace_flag_type_t {
    // TRACE_FLAG_IRQS_OFF = 0x01, // interrupts were disabled
    // TRACE_FLAG_IRQS_NOSUPPORT = 0x02,
    // TRACE_FLAG_NEED_RESCHED = 0x04,
    // TRACE_FLAG_HARDIRQ = 0x08, // inside an interrupt handler
    // TRACE_FLAG_SOFTIRQ = 0x10, // inside a softirq handler

    TRACE_FLAG_FTRACE_PRINT = 0x100,
    TRACE_FLAG_IS_VBLANK = 0x200,
    TRACE_FLAG_IS_TIMELINE = 0x400,

    TRACE_FLAG_IS_SW_QUEUE = 0x1000, // amdgpu_cs_ioctl
    TRACE_FLAG_IS_HW_QUEUE = 0x2000, // amdgpu_sched_run_job
    TRACE_FLAG_FENCE_SIGNALED = 0x4000, // *fence_signaled
};

struct trace_event_t
{
    bool is_fence_signaled() const
    {
        return !!( flags & TRACE_FLAG_FENCE_SIGNALED );
    }
    bool is_ftrace_print() const
    {
        return !!( flags & TRACE_FLAG_FTRACE_PRINT );
    }
    bool is_vblank() const
    {
        return !!( flags & TRACE_FLAG_IS_VBLANK );
    }
    bool is_timeline() const
    {
        return !!( flags & TRACE_FLAG_IS_TIMELINE );
    }
    const char *get_timeline_name( const char *def = NULL ) const
    {
        if ( flags & TRACE_FLAG_IS_SW_QUEUE )
            return "SW queue";
        else if ( flags & TRACE_FLAG_IS_HW_QUEUE )
            return "HW queue";
        else if ( is_fence_signaled() )
            return "Execution";

        return def;
    }

    bool is_filtered_out;
    int pid;                    // event process id
    int crtc;                   // drm_vblank_event crtc (or -1)

    uint32_t id;                // event id
    uint32_t cpu;               // cpu this event was hit on
    uint32_t flags;             // TRACE_FLAGS_IRQS_OFF, TRACE_FLAG_HARDIRQ, TRACE_FLAG_SOFTIRQ
    uint32_t context;           // event context (from fields)
    uint32_t seqno;             // event seqno (from fields)
    uint32_t id_start;          // start event if this is a graph sequence event (ie amdgpu_sched_run_job, fence_signaled)
    uint32_t graph_row_id;
    uint32_t duration;          // how long this timeline event took

    uint32_t color;

    int64_t ts;                 // timestamp
    const char *comm;           // command line
    const char *system;         // event system (ftrace-print, etc.)
    const char *name;           // event name
    const char *timeline;       // event timeline (gfx, sdma0, ...)
    const char *user_comm;      // User space comm (if we can figure this out)

    std::vector< event_field_t > fields;
};

const char *get_event_field_val( const trace_event_t &event, const char *name );

typedef std::function< int ( const trace_info_t &info, const trace_event_t &event ) > EventCallback;
int read_trace_file( const char *file, StrPool &strpool, EventCallback &cb );
