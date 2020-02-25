// Copyright (c) 2019, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

#include "caliper/caliper-config.h"

#include "caliper/ConfigManager.h"

namespace cali
{

extern ConfigManager::ConfigInfo cuda_activity_controller_info;
extern ConfigManager::ConfigInfo event_trace_controller_info;
extern ConfigManager::ConfigInfo nvprof_controller_info;
extern ConfigManager::ConfigInfo hatchet_region_profile_controller_info;
extern ConfigManager::ConfigInfo hatchet_sample_profile_controller_info;
extern ConfigManager::ConfigInfo intel_topdown_controller_info;
extern ConfigManager::ConfigInfo runtime_report_controller_info;

ConfigManager::ConfigInfo* builtin_controllers_table[] = {
    &cuda_activity_controller_info,
    &event_trace_controller_info,
    &nvprof_controller_info,
    &hatchet_region_profile_controller_info,
    &hatchet_sample_profile_controller_info,
    &intel_topdown_controller_info,
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
    "   { \"level\": \"local\",  \"select\": [ { \"expr\": \"sum(sum#io.bytes.read)\" } ] },"
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
    "  { \"level\": \"runtime\", \"group by\": \"io.region\" },"
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
    " \"name\"        : \"topdown.toplevel\","
    " \"description\" : \"Intel top-down analysis (top-level)\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"topdown\" ],"
    " \"query args\"  : "
    " ["
    "  { \"level\": \"serial\", \"select\": "
    "   ["
    "    { \"expr\": \"percent_total(sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)\", \"as\": \"CPU %\" },"
    "    { \"expr\": \"any(topdown.retiring)\", \"as\": \"Retiring\" },"
    "    { \"expr\": \"any(topdown.backend_bound)\", \"as\": \"Backend bound\" },"
    "    { \"expr\": \"any(topdown.frontend_bound)\", \"as\": \"Frontend bound\" },"
    "    { \"expr\": \"any(topdown.bad_speculation)\", \"as\": \"Bad speculation\" }"
    "   ]"
    "  },"
    "  { \"level\": \"local\", \"select\": "
    "   ["
    "    { \"expr\": \"sum(sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)\" },"
    "    { \"expr\": \"any(topdown.retiring)\", \"as\": \"Retiring\" },"
    "    { \"expr\": \"any(topdown.backend_bound)\", \"as\": \"Backend bound\" },"
    "    { \"expr\": \"any(topdown.frontend_bound)\", \"as\": \"Frontend bound\" },"
    "    { \"expr\": \"any(topdown.bad_speculation)\", \"as\": \"Bad speculation\" }"
    "   ]"
    "  },"
    "  { \"level\": \"cross\", \"select\": "
    "   ["
    "    { \"expr\": \"percent_total(sum#sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)\", \"as\": \"CPU\\ %\" },"
    "    { \"expr\": \"any(any#topdown.retiring)\", \"as\": \"Retiring\" },"
    "    { \"expr\": \"any(any#topdown.backend_bound)\", \"as\": \"Backend bound\" },"
    "    { \"expr\": \"any(any#topdown.frontend_bound)\", \"as\": \"Frontend bound\" },"
    "    { \"expr\": \"any(any#topdown.bad_speculation)\", \"as\": \"Bad speculation\" }"
    "   ]"
    "  }"
    " ]"
    "},"
    "{"
    " \"name\"        : \"topdown-counters.toplevel\","
    " \"description\" : \"Raw counter values for Intel top-down analysis (top-level)\","
    " \"type\"        : \"bool\","
    " \"category\"    : \"metric\","
    " \"services\"    : [ \"papi\" ],"
    " \"extra_config_flags\" : "
    " {"
    "   \"CALI_PAPI_COUNTERS\": "
    "     \"CPU_CLK_THREAD_UNHALTED:THREAD_P,UOPS_RETIRED:RETIRE_SLOTS,UOPS_ISSUED:ANY,INT_MISC:RECOVERY_CYCLES,IDQ_UOPS_NOT_DELIVERED:CORE\""
    " },"
    " \"query args\"  : "
    " ["
    "  { \"level\": \"serial\", \"select\": "
    "   ["
    "    { \"expr\": \"inclusive_sum(sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)\", \"as\": \"cpu_clk_thread_unhalted:thread_p\" },"
    "    { \"expr\": \"inclusive_sum(sum#papi.UOPS_RETIRED:RETIRE_SLOTS)\",   \"as\": \"uops_retired:retire_slots\"    },"
    "    { \"expr\": \"inclusive_sum(sum#papi.UOPS_ISSUED:ANY)\",             \"as\": \"uops_issued:any\"              },"
    "    { \"expr\": \"inclusive_sum(sum#papi.INT_MISC:RECOVERY_CYCLES)\",    \"as\": \"int_misc:recovery_cycles\"     },"
    "    { \"expr\": \"inclusive_sum(sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)\", \"as\": \"idq_uops_note_delivered:core\" }"
    "   ]"
    "  },"
    "  { \"level\": \"local\", \"select\": "
    "   ["
    "    { \"expr\": \"inclusive_sum(sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)\", \"as\": \"cpu_clk_thread_unhalted:thread_p\" },"
    "    { \"expr\": \"inclusive_sum(sum#papi.UOPS_RETIRED:RETIRE_SLOTS)\",   \"as\": \"uops_retired:retire_slots\"    },"
    "    { \"expr\": \"inclusive_sum(sum#papi.UOPS_ISSUED:ANY)\",             \"as\": \"uops_issued:any\"              },"
    "    { \"expr\": \"inclusive_sum(sum#papi.INT_MISC:RECOVERY_CYCLES)\",    \"as\": \"int_misc:recovery_cycles\"     },"
    "    { \"expr\": \"inclusive_sum(sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)\", \"as\": \"idq_uops_note_delivered:core\" }"
    "   ]"
    "  },"
    "  { \"level\": \"cross\", \"select\": "
    "   ["
    "    { \"expr\": \"sum(inclusive#sum#papi.CPU_CLK_THREAD_UNHALTED:THREAD_P)\", \"as\": \"cpu_clk_thread_unhalted:thread_p\" },"
    "    { \"expr\": \"sum(inclusive#sum#papi.UOPS_RETIRED:RETIRE_SLOTS)\",   \"as\": \"uops_retired:retire_slots\"    },"
    "    { \"expr\": \"sum(inclusive#sum#papi.UOPS_ISSUED:ANY)\",             \"as\": \"uops_issued:any\"              },"
    "    { \"expr\": \"sum(inclusive#sum#papi.INT_MISC:RECOVERY_CYCLES)\",    \"as\": \"int_misc:recovery_cycles\"     },"
    "    { \"expr\": \"sum(inclusive#sum#papi.IDQ_UOPS_NOT_DELIVERED:CORE)\", \"as\": \"idq_uops_note_delivered:core\" }"
    "   ]"
    "  }"
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
