#pragma once

#include <crab/domains/tvpi_dbm.hpp>
#include "crab_defs.hh"
#include "split_dbm.hh"

namespace clam {
using BASE(tvpi_dbm_domain_t) = tvpi_dbm_domain<BASE(split_dbm_domain_t)>;
using tvpi_dbm_domain_t = RGN_FUN(ARRAY_FUN(BOOL_NUM(BASE(tvpi_dbm_domain_t))));
} // end namespace clam
