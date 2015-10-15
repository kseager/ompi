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
#include "oshmem/op/op.h"
#include "oshmem/mca/spml/spml.h"
#include "oshmem/mca/atomic/atomic.h"
#include "oshmem/mca/atomic/base/base.h"
#include "oshmem/mca/memheap/memheap.h"
#include "oshmem/runtime/runtime.h"

#include "atomic_ofi.h"

/*AFAIK its all FI_SUM */
int mca_atomic_ofi_atomicto(void *target,
                        const void *value,
                        size_t nlong,
                        int pe,
                        struct oshmem_op_t *op)
{
        int ret;
        uint64_t dst = (uint64_t) pe;
	int datatype = DT_TRANSLATE[op->dt];

	assert(nlong <= sizeof(double complex));

	/*there is no atomic_nb for the standard, however fence is required */
	ret = fi_inject_atomic(mca_spml_self->shmem_transport_ofi_cntr_epfd,
			value,
			1,
			GET_DEST(dst),
			(uint64_t) target,
			0,
			datatype,
			FI_SUM);

	mca_spml_self->shmem_transport_ofi_pending_put_counter++;

	if (ret) {
		SPML_ERROR("atomicto failure ret=%d", ret);
	}

	return OSHMEM_SUCCESS;
}
