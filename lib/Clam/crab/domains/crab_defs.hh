#pragma once

/**
 * Types and macros to define abstract domains.
 **/

// AG: Something is wrong. Formatting breaks compilation
// clang-format off

#include <crab/config.h>
#include <clam/crab/crab_lang.hh>

#include <crab/domains/array_adaptive.hpp>
#include <crab/domains/flat_boolean_domain.hpp>
#ifdef HAS_OBJECT_DOMAIN
#include <crab/domains/object_domain.hpp>
#else
#include <crab/domains/region_domain.hpp>
#endif
#ifdef HAS_TVPI_DBM_DOM
#include <crab/domains/tvpi_dbm.hpp>
#endif
#include <crab/domains/term_equiv.hpp>
#ifdef HAS_VAR_PACK_TVPI_DOM
#include <crab/domains/numerical_packing.hpp>
#include <crab/domains/fixed_tvpi_domain.hpp>
#endif

namespace clam {
using namespace crab::domains;
using namespace ikos;

// Variable factory based on strings used by domains which create
// ghost variables (e.g., region and term domains)
using str_var_allocator = crab::var_factory_impl::str_var_alloc_col;
using str_varname_t = str_var_allocator::varname_t;

/* ====================================================================== */  
/* BEGIN MACROS to create the hierarchy of domains. Only for internal use */
/* ====================================================================== */    
// The base numerical domain 
#define BASE(DOM) base_ ## DOM
// Term functor domain 
#define TERM_FUN(DOM) \
  term_domain<term::TDomInfo<number_t, region_subdom_varname_t, DOM>>
// Reduced product of boolean domain with numerical domain
#define BOOL_NUM(DOM) flat_boolean_numerical_domain<DOM>
// Array functor domain where the parameter domain is DOM
#define ARRAY_FUN(DOM) array_adapt_domain<DOM>
// Region functor domain -- the root of the hierarchy of domains.
#ifdef HAS_OBJECT_DOMAIN
#define RGN_FUN(DOM) object_domain<ObjectParams<DOM>>
#else
#define RGN_FUN(DOM) region_domain<RegionParams<DOM>>
#endif
#ifdef HAS_VAR_PACK_TVPI_DOM
#define VAR_TVPI_FUN(DOM) numerical_packing_domain<fixed_tvpi_domain<DOM>>
#else
#ifdef HAS_TVPI_DBM_DOM
#define VAR_TVPI_FUN(DOM) tvpi_dbm_domain<DOM>
#else
#define VAR_TVPI_FUN(DOM) DOM
#endif
#endif
/* ====================================================================== */    
/* END MACROS to create the hierarchy of domains. Only for internal use   */
/* ====================================================================== */    

/* ====================================================================== */      
/* BEGIN array adaptive domain                                            */
/* ====================================================================== */      
// class ArrayAdaptParams {
// public:
//  /* options for array smashing */  
//   enum { is_smashable = 1 };
//   enum { smash_at_nonzero_offset = 1};
//   enum { max_smashable_cells = 64};
//   /* options for array expansion */  
//   enum { max_array_size = 64 };
// };
template<class Dom>
using array_adapt_domain = array_adaptive_domain<Dom>;
/* ====================================================================== */      
/* END array adaptive domain                                              */
/* ====================================================================== */      

/* ====================================================================== */        
/* BEGIN region domain                                                    */
/* ====================================================================== */
//using region_subdom_varname_t = str_varname_t;
using region_subdom_varname_t = clam::varname_t;  
#ifdef HAS_OBJECT_DOMAIN
using region_dom_varname_t = clam::varname_t;
template<class BaseAbsDom>
class ObjectParams {
public:
  using number_t = clam::number_t;
  using varname_t = clam::varname_t;
  using varname_allocator_t = typename varname_t::variable_factory_t;
  using base_abstract_domain_t = BaseAbsDom;
  using field_abstract_domain_t = BaseAbsDom;
  using base_varname_t = typename BaseAbsDom::varname_t;
};
#else
template<class BaseAbsDom>
class RegionParams {
public:
  using number_t = z_number;
  using varname_t = clam::varname_t;
  //using varname_allocator_t = str_var_allocator;
  using varname_allocator_t = typename varname_t::variable_factory_t;
  using base_abstract_domain_t = BaseAbsDom;
  using base_varname_t = typename BaseAbsDom::varname_t;
  static_assert(std::is_same<region_subdom_varname_t, base_varname_t>::value,
		"Region subdomain has a wrong varname type");
};
#endif
/* ====================================================================== */    
/* END region domain                                                      */
/* ====================================================================== */

#define REGISTER_DOMAIN(domain_type, name)			\
  bool register_ ## name() {					\
    using namespace clam;					\
    using namespace crab;					\
    auto &map = DomainRegistry::getFactoryMap();		\
    name ## _t obj{};						\
    clam_abstract_domain val(std::move(obj));		        \
    bool res = map.insert({domain_type, val}).second;		\
    CrabStats::reset();						\
    return res;							\
  }
#define UNREGISTER_DOMAIN(name)					\
  bool register_ ## name() {					\
    return false;						\
  }
  
} // end namespace clam
// clang-format on
