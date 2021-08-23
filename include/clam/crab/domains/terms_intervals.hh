#pragma once

#include <clam/crab/crab_defs.hh>
#include <crab/config.h>
#include <crab/domains/intervals.hpp>

namespace clam {
using str_interval_domain_t = interval_domain<number_t, str_varname_t>;
using term_int_domain_t =
  RGN_FUN(ARRAY_FUN(BOOL_NUM(TERM_FUN(str_interval_domain_t))));
} // end namespace clam
