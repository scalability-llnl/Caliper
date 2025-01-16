#include "SkylakeTopdown.h"

#include "../Services.h"

#include "caliper/common/Log.h"
#include "caliper/common/Variant.h"

namespace cali
{
namespace topdown
{

SkylakeTopdown::SkylakeTopdown(IntelTopdownLevel level)
    : cali::topdown::TopdownCalculator(
        level,
        // top_counters
        "IDQ_UOPS_NOT_DELIVERED:CORE"
        ",UOPS_ISSUED:ANY"
        ",UOPS_RETIRED:RETIRE_SLOTS"
        ",INT_MISC:RECOVERY_CYCLES"
        ",CPU_CLK_UNHALTED:THREAD_P",
        // all_counters
        "IDQ_UOPS_NOT_DELIVERED:CORE"
        ",UOPS_ISSUED:ANY"
        ",UOPS_RETIRED:RETIRE_SLOTS"
        ",INT_MISC:RECOVERY_CYCLES"
        ",CPU_CLK_UNHALTED:THREAD_P"
        ",IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE"
        ",BR_MISP_RETIRED:ALL_BRANCHES"
        ",MACHINE_CLEARS:COUNT"
        ",CYCLE_ACTIVITY:STALLS_MEM_ANY"
        ",EXE_ACTIVITY:BOUND_ON_STORES"
        ",CYCLE_ACTIVITY:STALLS_TOTAL"
        ",EXE_ACTIVITY:1_PORTS_UTIL"
        ",EXE_ACTIVITY:2_PORTS_UTIL",
        // Note: PAPI doesn't seem to have UOPS_RETIRED.MACRO_FUSED,
        //       so we can't currently calculate L2 metrics under retiring.
        //       The commented counters below are unique to these metrics.
        // ",UOPS_RETIRED:MACRO_FUSED"
        // ",INST_RETIRED:ANY_P"
        // res_top
        { "retiring", "backend_bound", "frontend_bound", "bad_speculation" },
        // res_all
        { "retiring",
          // "light_operations",
          // "heavy_operations",
          "backend_bound",
          "memory_bound",
          "core_bound",
          "frontend_bound",
          "fetch_latency",
          "fetch_bandwidth",
          "bad_speculation",
          "branch_mispredicts",
          "machine_clears" }
    )
{}

bool SkylakeTopdown::setup_config(Caliper& c, Channel& channel) const
{
    channel.config().set("CALI_PAPI_COUNTERS", m_level == All ? m_all_counters : m_top_counters);
    channel.config().set("CALI_PAPI_ENABLE_MULTIPLEXING", "true");
    if (!cali::services::register_service(&c, &channel, "papi")) {
        Log(0).stream() << channel.name() << ": topdown: Unable to register papi service, skipping topdown"
                        << std::endl;
        return false;
    }
    return true;
}

std::vector<Entry> SkylakeTopdown::compute_toplevel(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_idq_uops_not_delivered_core = get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CORE");
    Variant v_uops_issued_any             = get_val_from_rec(rec, "UOPS_ISSUED:ANY");
    Variant v_uops_retired_retire_slots   = get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
    Variant v_int_misc_recovery_cycles    = get_val_from_rec(rec, "INT_MISC:RECOVERY_CYCLES");
    Variant v_cpu_clk_unhalted_thread     = get_val_from_rec(rec, "CPU_CLK_UNHALTED:THREAD_P");

    bool is_incomplete = v_idq_uops_not_delivered_core.empty() || v_uops_issued_any.empty()
                         || v_uops_retired_retire_slots.empty() || v_int_misc_recovery_cycles.empty()
                         || v_cpu_clk_unhalted_thread.empty();
    bool is_nonzero = v_idq_uops_not_delivered_core.to_double() > 0.0 && v_uops_issued_any.to_double() > 0.0
                      && v_uops_retired_retire_slots.to_double() > 0.0 && v_int_misc_recovery_cycles.to_double() > 0.0
                      && v_idq_uops_not_delivered_core.to_double() > 0.0;

    double thread_slots = 4 * v_cpu_clk_unhalted_thread.to_double();

    if (is_incomplete || is_nonzero || thread_slots < 1.0) {
        return ret;
    }

    double frontend_bound  = std::max(v_idq_uops_not_delivered_core.to_double() / thread_slots, 0.0);
    double bad_speculation = std::max(
        (v_uops_issued_any.to_double() - v_uops_retired_retire_slots.to_double()
         + 4 * v_int_misc_recovery_cycles.to_double())
            / thread_slots,
        0.0
    );
    double backend_bound = std::max(
        1 - frontend_bound
            - (v_uops_issued_any.to_double() + 4 * v_int_misc_recovery_cycles.to_double()) / thread_slots,
        0.0
    );
    double retiring = std::max(v_uops_retired_retire_slots.to_double() / thread_slots, 0.0);

    ret.reserve(4);
    ret.push_back(Entry(m_result_attrs["retiring"], Variant(retiring)));
    ret.push_back(Entry(m_result_attrs["backend_bound"], Variant(backend_bound)));
    ret.push_back(Entry(m_result_attrs["frontend_bound"], Variant(frontend_bound)));
    ret.push_back(Entry(m_result_attrs["bad_speculation"], Variant(bad_speculation)));

    return ret;
}

std::size_t SkylakeTopdown::get_num_expected_toplevel() const
{
    return 4;
}

std::vector<Entry> SkylakeTopdown::compute_retiring(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;
    
    // TODO uncomment when we can figure out the raw counter corresponding to
    //      UOPS_RETIRED:MACRO_FUSED

    // Variant v_uops_retired_retire_slots = get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
    // Variant v_uops_retired_macro_fused  = get_val_from_rec(rec, "UOPS_RETIRED:MACRO_FUSED");
    // Variant v_inst_retired_any          = get_val_from_rec(rec, "INST_RETIRED:ANY_P");
    // Variant v_cpu_clk_unhalted_thread   = get_val_from_rec(rec, "CPU_CLK_UNHALTED:THREAD_P");

    // bool is_incomplete = v_uops_retired_retire_slots.empty() || v_uops_retired_macro_fused.empty()
    //                      || v_inst_retired_any.empty() || v_cpu_clk_unhalted_thread.empty();

    // double thread_slots = 4 * v_cpu_clk_unhalted_thread.to_double();

    // if (is_incomplete || !(thread_slots > 1.0)) {
    //     return ret;
    // }

    // double retiring = std::max(v_uops_retired_retire_slots.to_double() / thread_slots, 0.0);

    // double heavy_operations = std::max(
    //     (v_uops_retired_retire_slots.to_double() + v_uops_retired_macro_fused.to_double()
    //      - v_inst_retired_any.to_double())
    //         / thread_slots,
    //     0.0
    // );

    // ret.reserve(2);
    // ret.push_back(Entry(m_result_attrs["heavy_operations"], Variant(heavy_operations)));
    // ret.push_back(Entry(m_result_attrs["light_operations"], Variant(std::max(retiring - heavy_operations, 0.0))));

    return ret;
}

std::size_t SkylakeTopdown::get_num_expected_retiring() const
{
    return 0;
}

std::vector<Entry> SkylakeTopdown::compute_backend_bound(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_cycle_activity_stalls_mem_any = get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_MEM_ANY");
    Variant v_exe_activity_bound_on_stores  = get_val_from_rec(rec, "EXE_ACTIVITY:BOUND_ON_STORES");
    Variant v_cycle_activity_stalls_total   = get_val_from_rec(rec, "CYCLE_ACTIVITY:STALLS_TOTAL");
    Variant v_exe_activity_1_ports_util     = get_val_from_rec(rec, "EXE_ACTIVITY:1_PORTS_UTIL");
    Variant v_exe_activity_2_ports_util     = get_val_from_rec(rec, "EXE_ACTIVITY:2_PORTS_UTIL");

    Variant v_idq_uops_not_delivered_core = get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CORE");
    Variant v_uops_issued_any             = get_val_from_rec(rec, "UOPS_ISSUED:ANY");
    Variant v_uops_retired_retire_slots   = get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
    Variant v_int_misc_recovery_cycles    = get_val_from_rec(rec, "INT_MISC:RECOVERY_CYCLES");
    Variant v_cpu_clk_unhalted_thread     = get_val_from_rec(rec, "CPU_CLK_UNHALTED:THREAD_P");

    bool is_incomplete = v_idq_uops_not_delivered_core.empty() || v_uops_issued_any.empty()
                         || v_uops_retired_retire_slots.empty() || v_int_misc_recovery_cycles.empty()
                         || v_cpu_clk_unhalted_thread.empty() || v_cycle_activity_stalls_mem_any.empty()
                         || v_exe_activity_bound_on_stores.empty() || v_cycle_activity_stalls_total.empty()
                         || v_exe_activity_1_ports_util.empty() || v_exe_activity_2_ports_util.empty();

    double thread_slots = 4 * v_cpu_clk_unhalted_thread.to_double();

    if (is_incomplete || !(thread_slots > 1.0)) {
        return ret;
    }

    double frontend_bound = std::max(v_idq_uops_not_delivered_core.to_double() / thread_slots, 0.0);
    double backend_bound  = std::max(
        1 - frontend_bound
            - (v_uops_issued_any.to_double() + 4 * v_int_misc_recovery_cycles.to_double()) / thread_slots,
        0.0
    );
    double retiring = std::max(v_uops_retired_retire_slots.to_double() / thread_slots, 0.0);

    double memory_bound = std::max(
        ((v_cycle_activity_stalls_mem_any.to_double() + v_exe_activity_bound_on_stores.to_double())
         / (v_cycle_activity_stalls_total.to_double()
            + (v_exe_activity_1_ports_util.to_double() + retiring * v_exe_activity_2_ports_util.to_double())
            + v_exe_activity_bound_on_stores.to_double()))
            * backend_bound,
        0.0
    );

    ret.reserve(2);

    ret.push_back(Entry(m_result_attrs["memory_bound"], Variant(memory_bound)));
    ret.push_back(Entry(m_result_attrs["core_bound"], Variant(std::max(backend_bound - memory_bound, 0.0))));

    return ret;
}

std::size_t SkylakeTopdown::get_num_expected_backend_bound() const
{
    return 2;
}

std::vector<Entry> SkylakeTopdown::compute_frontend_bound(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_idq_uops_not_delivered_cycles_0_uops_deliv_core =
        get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CYCLES_0_UOPS_DELIV_CORE");
    Variant v_idq_uops_not_delivered_core = get_val_from_rec(rec, "IDQ_UOPS_NOT_DELIVERED:CORE");
    Variant v_cpu_clk_unhalted_thread     = get_val_from_rec(rec, "CPU_CLK_UNHALTED:THREAD_P");

    bool is_incomplete = v_idq_uops_not_delivered_cycles_0_uops_deliv_core.empty()
                         || v_idq_uops_not_delivered_core.empty() || v_cpu_clk_unhalted_thread.empty();

    double thread_slots = 4 * v_cpu_clk_unhalted_thread.to_double();

    if (is_incomplete || !(thread_slots > 1.0)) {
        return ret;
    }

    double frontend_bound = std::max(v_idq_uops_not_delivered_core.to_double() / thread_slots, 0.0);
    double fetch_latency =
        std::max(4 * v_idq_uops_not_delivered_cycles_0_uops_deliv_core.to_double() / thread_slots, 0.0);

    ret.reserve(2);

    ret.push_back(Entry(m_result_attrs["fetch_latency"], Variant(fetch_latency)));
    ret.push_back(Entry(m_result_attrs["fetch_bandwidth"], Variant(std::max(frontend_bound - fetch_latency, 0.0))));

    return ret;
}

std::size_t SkylakeTopdown::get_num_expected_frontend_bound() const
{
    return 2;
}

std::vector<Entry> SkylakeTopdown::compute_bad_speculation(const std::vector<Entry>& rec)
{
    std::vector<Entry> ret;

    Variant v_br_misp_retired_all_branches = get_val_from_rec(rec, "BR_MISP_RETIRED:ALL_BRANCHES");
    Variant v_machine_clears_count         = get_val_from_rec(rec, "MACHINE_CLEARS:COUNT");
    Variant v_uops_issued_any              = get_val_from_rec(rec, "UOPS_ISSUED:ANY");
    Variant v_uops_retired_retire_slots    = get_val_from_rec(rec, "UOPS_RETIRED:RETIRE_SLOTS");
    Variant v_int_misc_recovery_cycles     = get_val_from_rec(rec, "INT_MISC:RECOVERY_CYCLES");
    Variant v_cpu_clk_unhalted_thread      = get_val_from_rec(rec, "CPU_CLK_UNHALTED:THREAD_P");

    bool is_incomplete = v_br_misp_retired_all_branches.empty() || v_machine_clears_count.empty()
                         || v_uops_issued_any.empty() || v_uops_retired_retire_slots.empty()
                         || v_int_misc_recovery_cycles.empty() || v_cpu_clk_unhalted_thread.empty();

    double thread_slots = 4 * v_cpu_clk_unhalted_thread.to_double();

    if (is_incomplete || !(thread_slots > 1.0)) {
        return ret;
    }

    double bad_speculation = std::max(
        (v_uops_issued_any.to_double() - v_uops_retired_retire_slots.to_double()
         + 4 * v_int_misc_recovery_cycles.to_double())
            / thread_slots,
        0.0
    );
    double branch_mispredicts = std::max(
        (v_br_misp_retired_all_branches.to_double()
         / (v_br_misp_retired_all_branches.to_double() + v_machine_clears_count.to_double()))
            * bad_speculation,
        0.0
    );

    ret.reserve(2);

    ret.push_back(Entry(m_result_attrs["branch_mispredicts"], Variant(branch_mispredicts)));
    ret.push_back(Entry(m_result_attrs["machine_clears"], Variant(std::max(bad_speculation - branch_mispredicts, 0.0)))
    );

    return ret;
}

std::size_t SkylakeTopdown::get_num_expected_bad_speculation() const
{
    return 2;
}

} // namespace topdown
} // namespace cali