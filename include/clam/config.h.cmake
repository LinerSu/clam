#pragma once

/** Define whether llvm-seahorn is available */
#cmakedefine HAVE_LLVM_SEAHORN ${HAVE_LLVM_SEAHORN}

/** Whether to use big numbers for representing weights in DBM-based domains **/
#cmakedefine USE_DBM_BIGNUM ${USE_DBM_BIGNUM}

/** Whether to use safe or unsafe for representing weights
 ** in DBM-based domains. Only if USE_DBM_BIGNUM is disabled.  **/
#cmakedefine USE_DBM_SAFEINT ${USE_DBM_SAFEINT}

/** Include all default abstract domains.**/
#cmakedefine INCLUDE_ALL_DOMAINS ${INCLUDE_ALL_DOMAINS}

/** whether Clam is compiled as a standalone application **/
#cmakedefine CLAM_IS_TOPLEVEL ${CLAM_IS_TOPLEVEL}

/** whether Crab has object domain **/
#cmakedefine HAS_OBJECT_DOMAIN ${HAS_OBJECT_DOMAIN}

/** whether Crab has tvpi dbm domain **/
#cmakedefine HAS_TVPI_DBM_DOM ${HAS_TVPI_DBM_DOM}

/** whether Crab has var pack + fixed tvpi domain **/
#cmakedefine HAS_VAR_PACK_TVPI_DOM ${HAS_VAR_PACK_TVPI_DOM}
