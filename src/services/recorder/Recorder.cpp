// Copyright (c) 2015, Lawrence Livermore National Security, LLC.  
// Produced at the Lawrence Livermore National Laboratory.
//
// This file is part of Caliper.
// Written by David Boehme, boehme3@llnl.gov.
// LLNL-CODE-678900
// All rights reserved.
//
// For details, see https://github.com/scalability-llnl/Caliper.
// Please also see the LICENSE file for our additional BSD notice.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
//  * Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the disclaimer below.
//  * Redistributions in binary form must reproduce the above copyright notice, this list of
//    conditions and the disclaimer (as noted below) in the documentation and/or other materials
//    provided with the distribution.
//  * Neither the name of the LLNS/LLNL nor the names of its contributors may be used to endorse
//    or promote products derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// LAWRENCE LIVERMORE NATIONAL SECURITY, LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Recorder.cpp
// Caliper event recorder

#include "caliper/CaliperService.h"

#include "caliper/Caliper.h"
#include "caliper/SnapshotRecord.h"

#include "caliper/common/Log.h"
#include "caliper/common/Node.h"
#include "caliper/common/OutputStream.h"
#include "caliper/common/RuntimeConfig.h"

#include "caliper/reader/CaliWriter.h"

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <mutex>
#include <fstream>
#include <random>
#include <string>

using namespace cali;
using namespace std;

namespace 
{

class Recorder
{    
    static const ConfigSet::Entry s_configdata[];

    ConfigSet m_config;
    CaliWriter m_writer;
    
    // --- helpers

    string random_string(string::size_type len) {
        static std::mt19937 rgen(chrono::system_clock::now().time_since_epoch().count());
        
        static const char characters[] = "0123456789"
            "abcdefghiyklmnopqrstuvwxyz"
            "ABCDEFGHIJKLMNOPQRSTUVWXZY";

        std::uniform_int_distribution<> random(0, sizeof(characters)-2);

        string s(len, '-');

        generate_n(s.begin(), len, [&](){ return characters[random(rgen)]; });

        return s;
    }

    string create_filename() {
        char   timestring[16];
        time_t tm = chrono::system_clock::to_time_t(chrono::system_clock::now());
        strftime(timestring, sizeof(timestring), "%y%m%d-%H%M%S", localtime(&tm));

        int  pid = static_cast<int>(getpid());

        return string(timestring) + "_" + std::to_string(pid) + "_" + random_string(12) + ".cali";
    }

    void pre_flush_cb(Caliper* c, Channel* chn, const SnapshotRecord* flush_info) {
        // Generate m_filename from pattern in the config file and the attributes
        // in flush_info.
        // The actual output stream will be created on-demand 
        // with the first flush_snapshot call.

        std::string filename  = m_config.get("filename").to_string();
        std::string directory = m_config.get("directory").to_string();

        if (filename.empty())
            filename = create_filename();
        if (!directory.empty())
            filename = directory + "/" + filename;

        OutputStream stream;
        stream.set_filename(filename.c_str(), *c, flush_info->to_entrylist());
        
        m_writer = CaliWriter(stream);
    }

    void write_snapshot(Caliper* c, const SnapshotRecord* snapshot) {        
        SnapshotRecord::Data   data = snapshot->data();
        SnapshotRecord::Sizes sizes = snapshot->size();

        cali_id_t node_ids[128];
        size_t    nn = std::min<size_t>(sizes.n_nodes, 128);
        
        for (size_t i = 0; i < nn; ++i)
            node_ids[i] = data.node_entries[i]->id();

        m_writer.write_snapshot(*c, nn, node_ids,
                                sizes.n_immediate, data.immediate_attr, data.immediate_data);
    }

    void post_flush_cb(Caliper* c, Channel* chn) {
        m_writer.write_globals(*c, c->get_globals(chn));
    }

    void finish_cb(Caliper* c, Channel* chn) {
        Log(1).stream() << chn->name()
            << ": Recorder: Wrote " << m_writer.num_written() << " records." << std::endl;
    }    

    Recorder(Caliper* c, Channel* chn)
        : m_config(chn->config().init("recorder", s_configdata))
    { }

public:

    ~Recorder() 
        { }

    static void recorder_register(Caliper* c, Channel* chn) {
        Recorder* instance = new Recorder(c, chn);

        chn->events().pre_flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* flush_info){
                instance->pre_flush_cb(c, chn, flush_info);
            });
        chn->events().write_snapshot.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* flush_info, const SnapshotRecord* snapshot){
                instance->write_snapshot(c, snapshot);
            });
        chn->events().post_flush_evt.connect(
            [instance](Caliper* c, Channel* chn, const SnapshotRecord* flush_info){
                instance->post_flush_cb(c, chn);
            });
        chn->events().finish_evt.connect(
            [instance](Caliper* c, Channel* chn){
                instance->finish_cb(c, chn);
                delete instance;
            });
    }
};

const ConfigSet::Entry Recorder::s_configdata[] = {
    { "filename", CALI_TYPE_STRING, "",
      "File name for event record stream. Auto-generated by default.",
      "File name for event record stream. Either one of\n"
      "   stdout: Standard output stream,\n"
      "   stderr: Standard error stream.\n"
      " or a file name pattern. By default, a filename is auto-generated.\n"
    },
    { "directory", CALI_TYPE_STRING, "",
      "A directory to write .cali files to."
      "A directory to write .cali files to. The directory must exist,\n"
      "Caliper does not create it\n"
    },
    ConfigSet::Terminator
};

} // namespace

namespace cali
{
    CaliperService recorder_service { "recorder", ::Recorder::recorder_register };
}
