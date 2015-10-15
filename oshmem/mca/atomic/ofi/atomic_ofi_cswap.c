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
#include <stdlib.h>

#include "oshmem/constants.h"
#include "oshmem/mca/spml/spml.h"
#include "oshmem/mca/atomic/atomic.h"
#include "oshmem/mca/atomic/base/base.h"
#include "oshmem/mca/memheap/memheap.h"
#include "oshmem/runtime/runtime.h"

#include "atomic_ofi.h"

int mca_atomic_ofi_cswap(void *target,
                         void *prev,
                         const void *cond,
                         const void *value,
                         size_t nlong,
                         int pe)
{

	int ret;
        uint64_t dst = (uint64_t) pe;
	int datatype = SizeT[((nlong/4)%2)];

        assert(nlong <= sizeof(double complex));


	ret = fi_compare_atomic(mca_spml_self->shmem_transport_ofi_cntr_epfd,
				value,
				1,
				NULL,
				(void *)cond,
				NULL,
				(void *)prev,
				NULL,
				GET_DEST(dst),
				(uint64_t) target,
				0,
				datatype,
				FI_CSWAP,
				NULL);
	if (ret) {
		SPML_ERROR("compare atomic failure ret=%d", ret);
	}

	mca_spml_self->shmem_transport_ofi_pending_get_counter++;

	ofshm_get_wait();

	return OSHMEM_SUCCESS;
}
