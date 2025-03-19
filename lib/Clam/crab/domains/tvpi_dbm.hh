#pragma once

#include <crab/domains/tvpi_dbm.hpp>
#include "crab_defs.hh"
// #include <crab/domains/fixed_tvpi_domain.hpp>
#include "split_dbm.hh"
#include <crab/domains/tvpi_split_dbm.hpp>

namespace clam {
using BASE(tvpi_dbm_domain_t) = tvpi_dbm_domain<BASE(split_dbm_domain_t)>;
using tvpi_dbm_domain_t = RGN_FUN(ARRAY_FUN(BOOL_NUM(BASE(tvpi_dbm_domain_t))));

using BASE(fixed_tvpi_dbm_domain_t) =
    tvpi_split_dbm_domain<number_t, region_subdom_varname_t, DBMParams>;
using fixed_tvpi_dbm_domain_t =
    RGN_FUN(ARRAY_FUN(BOOL_NUM(BASE(fixed_tvpi_dbm_domain_t))));
} // end namespace clam
