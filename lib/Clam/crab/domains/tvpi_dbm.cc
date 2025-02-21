#include <clam/config.h>
#include <clam/CrabDomain.hh>
#include <clam/RegisterAnalysis.hh>
#include "tvpi_dbm.hh"

namespace clam {
#ifdef INCLUDE_ALL_DOMAINS
REGISTER_DOMAIN(clam::CrabDomain::TVPI_DBM, tvpi_dbm_domain)
#else
UNREGISTER_DOMAIN(tvpi_dbm_domain)
#endif
} // end namespace clam

