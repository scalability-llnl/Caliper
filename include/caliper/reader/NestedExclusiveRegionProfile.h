// Copyright (c) 2015-2022, Lawrence Livermore National Security, LLC.
// See top-level LICENSE file for details.

/// \file NestedExclusiveRegionProfile.h
/// NestedExclusiveRegionProfile class

#pragma once

#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace cali
{

class CaliperMetadataAccessInterface;
class Entry;

/// \brief Calculate a flat exclusive region profile
class NestedExclusiveRegionProfile
{
    struct NestedExclusiveRegionProfileImpl;
    std::shared_ptr<NestedExclusiveRegionProfileImpl> mP;

public:

    NestedExclusiveRegionProfile(
        CaliperMetadataAccessInterface& db,
        const char*                     metric_attr_name,
        const char*                     region_attr_name = ""
    );

    void operator() (CaliperMetadataAccessInterface& db, const std::vector<Entry>& rec);

    /// \brief Return tuple with { { region name -> value } map, sum in given region type, total sum }
    std::tuple<std::map<std::string, double>, double, double> result() const;
};

} // namespace cali
