#ifndef CALI_TOPDOWN_SKYLAKE_TOPDOWN_H
#define CALI_TOPDOWN_SKYLAKE_TOPDOWN_H

#include "TopdownCalculator.h"

namespace cali
{
namespace topdown
{

// Topdown calculations for:
// * Skylake
// * Skylake-X
// * Cascade Lake
// * Cascade Lake X
class SkylakeTopdown : public TopdownCalculator
{
public:

    SkylakeTopdown(IntelTopdownLevel level);

    virtual ~SkylakeTopdown() = default;

    virtual bool setup_config(Caliper& c, Channel& channel) const override;

    virtual std::vector<Entry> compute_toplevel(const std::vector<Entry>& rec) override;
    virtual std::size_t        get_num_expected_toplevel() const override;
    virtual std::vector<Entry> compute_retiring(const std::vector<Entry>& rec) override;
    virtual std::size_t        get_num_expected_retiring() const override;
    virtual std::vector<Entry> compute_backend_bound(const std::vector<Entry>& rec) override;
    virtual std::size_t        get_num_expected_backend_bound() const override;
    virtual std::vector<Entry> compute_frontend_bound(const std::vector<Entry>& rec) override;
    virtual std::size_t        get_num_expected_frontend_bound() const override;
    virtual std::vector<Entry> compute_bad_speculation(const std::vector<Entry>& rec) override;
    virtual std::size_t        get_num_expected_bad_speculation() const override;
};

} // namespace topdown
} // namespace cali

#endif /* CALI_TOPDOWN_SKYLAKE_TOPDOWN_H */