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

/** whether region domain without ghost variables should be used or
    not **/
#cmakedefine HAS_FAST_REGION_DOMAIN ${HAS_FAST_REGION_DOMAIN}

/** whether region domain that suppors is_deferenceable intrisics
    should be used or not. If HAS_FAST_REGION_DOMAIN is ON then
    USE_REGION_IS_DEREF_DOMAIN is ignored. **/
#cmakedefine USE_REGION_IS_DEREF_DOMAIN ${USE_REGION_IS_DEREF_DOMAIN}




