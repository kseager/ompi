/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved 
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "oshmem_config.h"
#include <stdio.h>

#include "oshmem/constants.h"
#include "oshmem/mca/atomic/atomic.h"
#include "oshmem/mca/spml/spml.h"
#include "oshmem/mca/memheap/memheap.h"
#include "oshmem/proc/proc.h"
#include "atomic_ofi.h"

int DT_TRANSLATE[] = {
	FI_INT16,
	FI_INT32,
	FI_INT64,
	FI_INT64,
	FI_INT16,
	FI_INT32,
	FI_INT64,
	FI_FLOAT,
	FI_DOUBLE,
	FI_LONG_DOUBLE,
	FI_FLOAT_COMPLEX,
	FI_DOUBLE_COMPLEX,
	0, /*do not support fortran*/
	0,
	0,
	0,
	0,
	0,
	0, /*max type? */
};

int SizeT[2] = {
	FI_INT64, FI_INT32
};

/*
 * Initial query function that is invoked during initialization, allowing
 * this module to indicate what level of thread support it provides.
 */
int mca_atomic_ofi_init(bool enable_progress_threads, bool enable_threads)
{
	return OSHMEM_SUCCESS;
}

int mca_atomic_ofi_finalize(void)
{
	return OSHMEM_SUCCESS;
}

mca_atomic_base_module_t *
mca_atomic_ofi_query(int *priority)
{
    mca_atomic_ofi_module_t *module;

    *priority = mca_atomic_ofi_component.priority;

    module = OBJ_NEW(mca_atomic_ofi_module_t);
    if (module) {
        module->super.atomic_fadd = mca_atomic_ofi_fadd;
        module->super.atomic_cswap = mca_atomic_ofi_cswap;
        module->super.atomic_swap = mca_atomic_ofi_swap;
        module->super.atomic_atomicto = mca_atomic_ofi_atomicto;
        return &(module->super);
    }

    return NULL ;
}
