// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

namespace cali
{

extern ConfigManager::ConfigInfo event_trace_controller_info;
extern ConfigManager::ConfigInfo nvprof_controller_info;
extern ConfigManager::ConfigInfo hatchet_region_profile_controller_info;
extern ConfigManager::ConfigInfo hatchet_sample_profile_controller_info;
extern ConfigManager::ConfigInfo runtime_report_controller_info;

ConfigManager::ConfigInfo* builtin_controllers_table[] = {
    &event_trace_controller_info,
    &nvprof_controller_info,
    &hatchet_region_profile_controller_info,
    &hatchet_sample_profile_controller_info,
    &runtime_report_controller_info,
    nullptr
};

const char* builtin_option_specs =
    "["
    "{"
    " \"name\"        : \"profile.mpi\","
    " \"type\"        : \"bool\","
    " \"description\" : \"Profile MPI functions\","
    " \"category\"    : \"region\","
    " \"services\"    : [ \"mpi\" ],"
    " \"extra_config_flags\": { \"CALI_MPI_BLACKLIST\": \"MPI_Comm_rank,MPI_Comm_size,MPI_Wtick,MPI_Wtime\" },"
    " \"query args\"  : { \"level\": \"runtime\", \"group by\": [ \"mpi.function\" ] }"
    "},"
    "{"
    " \"name\"        : \"profile.cuda\","
    " \"type\"        : \"bool\","
    " \"description\" : \"Profile CUDA API functions\","
    " \"category\"    : \"region\","
    " \"services\"    : [ \"cupti\"  ],"
    " \"query args\"  : { \"level\": \"runtime\", \"group by\": [ \"cupti.runtimeAPI\" ] }"
    "},"
    "{"
    " \"name\"        : \"io.bytes.written\","
    " \"description\" : \"Report I/O bytes written\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"io\" ],"
    " \"query args\"  : "
    " ["
    "   { \"level\": \"serial\", \"select\": [ { \"expr\": \"sum(sum#io.bytes.written)\", \"as\": \"Bytes written\" } ] },"
    "   { \"level\": \"local\",  \"select\": [ { \"expr\": \"sum(sum#io.bytes.written)\" } ] },"
    "   { \"level\": \"cross\",  \"select\":"
    "     [ { \"expr\": \"avg(sum#sum#io.bytes.written)\", \"as\": \"Avg written\"},"
    "       { \"expr\": \"sum(sum#sum#io.bytes.written)\", \"as\": \"Total written\"}"
    "     ]"
    "   }"
    " ]"
    "},"
    "{"
    " \"name\"        : \"io.bytes.read\","
    " \"description\" : \"Report I/O bytes read\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"io\" ],"
    " \"query args\"  : "
    " ["
    "   { \"level\": \"serial\", \"select\": [ { \"expr\": \"sum(sum#io.bytes.read)\", \"as\": \"Bytes read\" } ] },"
    "   { \"level\": \"local\",  \"select\": [ { \"expr\": \"sum(sum#io.bytes.written)\" } ] },"
    "   { \"level\": \"cross\",  \"select\":"
    "     [ { \"expr\": \"avg(sum#sum#io.bytes.read)\", \"as\": \"Avg read\" },"
    "       { \"expr\": \"sum(sum#sum#io.bytes.read)\", \"as\": \"Total read\" }"
    "     ]"
    "   }"
    " ]"
    "},"
    "{"
    " \"name\"        : \"io.bytes\","
    " \"description\" : \"Report I/O bytes written and read\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"inherit\"     : [ \"io.bytes.read\", \"io.bytes.written\" ]"
    "},"
    "{"
    " \"name\"        : \"io.read.bandwidth\","
    " \"description\" : \"Report I/O read bandwidth\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"io\" ],"
    " \"query args\"  :"
    " ["
    "  { \"level\": \"serial\", \"group by\": \"io.region\", \"select\":"
    "   ["
    "    { \"expr\": \"io.region\", \"as\": \"I/O\" },"
    "    { \"expr\": \"ratio(sum#io.bytes.read,sum#time.duration,8e-6)\", \"as\": \"Read Mbit/s\" }"
    "   ]"
    "  },"
    "  { \"level\": \"local\", \"group by\": \"io.region\", \"select\":"
    "   [ { \"expr\": \"ratio(sum#io.bytes.read,sum#time.duration,8e-6)\" } ]"
    "  },"
    "  { \"level\": \"cross\", \"select\":"
    "   [ "
    "    { \"expr\": \"avg(sum#io.bytes.read/sum#time.duration)\", \"as\": \"Avg read Mbit/s\" },"
    "    { \"expr\": \"max(sum#io.bytes.read/sum#time.duration)\", \"as\": \"Max read Mbit/s\" }"
    "   ]"
    "  }"
    " ]"
    "},"
    "{"
    " \"name\"        : \"io.write.bandwidth\","
    " \"description\" : \"Report I/O write bandwidth\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"io\" ],"
    " \"query args\"  :"
    " ["
    "  { \"level\": \"serial\", \"group by\": \"io.region\", \"select\":"
    "   ["
    "    { \"expr\": \"io.region\", \"as\": \"I/O\" },"
    "    { \"expr\": \"ratio(sum#io.bytes.written,sum#time.duration,8e-6)\", \"as\": \"Write Mbit/s\" }"
    "   ]"
    "  },"
    "  { \"level\": \"local\", \"group by\": \"io.region\", \"select\":"
    "   [ { \"expr\": \"ratio(sum#io.bytes.written,sum#time.duration,8e-6)\" } ]"
    "  },"
    "  { \"level\": \"cross\", \"select\":"
    "   [ "
    "    { \"expr\": \"avg(sum#io.bytes.written/sum#time.duration)\", \"as\": \"Avg write Mbit/s\" },"
    "    { \"expr\": \"max(sum#io.bytes.written/sum#time.duration)\", \"as\": \"Max write Mbit/s\" }"
    "   ]"
    "  }"
    " ]"
    "},"
    "{"
    " \"name\"        : \"mem.highwatermark\","
    " \"description\" : \"Record memory high-water mark for regions\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"alloc\", \"sysalloc\" ],"
    " \"extra_config_flags\" : { \"CALI_ALLOC_TRACK_ALLOCATIONS\": \"false\", \"CALI_ALLOC_RECORD_HIGHWATERMARK\": \"true\" },"
    " \"query args\"  : "
    " ["
    "   { \"level\": \"serial\", \"select\": [ { \"expr\": \"max(max#alloc.region.highwatermark)\", \"as\": \"Max Alloc'd Mem\" } ] },"
    "   { \"level\": \"local\",  \"select\": [ { \"expr\": \"max(max#alloc.region.highwatermark)\" } ] },"
    "   { \"level\": \"cross\",  \"select\": [ { \"expr\": \"max(max#max#alloc.region.highwatermark)\", \"as\": \"Max Alloc'd Mem\" } ] }"
    " ]"
    "},"
    "{"
    " \"name\"        : \"output\","
    " \"description\" : \"Output location ('stdout', 'stderr', or filename)\","
    " \"type\"        : \"string\","
    " \"category\"    : \"output\""
    "}"
    "]";

}
