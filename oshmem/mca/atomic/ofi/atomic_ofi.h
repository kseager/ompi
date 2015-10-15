/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved 
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_ATOMIC_MXM_H
#define MCA_ATOMIC_MXM_H

#include "oshmem_config.h"

#include "opal/mca/mca.h"
#include "oshmem/mca/atomic/atomic.h"
#include "oshmem/util/oshmem_util.h"

/* This component does uses SPML:OFSHM */
#include "oshmem/mca/spml/ofshm/spml_ofshm.h"

/*translation of mapping in op.h */
int extern DT_TRANSLATE[19];

int extern SizeT[2];

BEGIN_C_DECLS

/* Globally exported variables */

OSHMEM_MODULE_DECLSPEC extern mca_atomic_base_component_1_0_0_t
mca_atomic_ofi_component;

/* this component works with spml:ofshm only */
extern mca_spml_ofshm_t *mca_spml_self;
extern void ofshm_get_wait(void);

OSHMEM_DECLSPEC void atomic_ofi_lock(int pe);
OSHMEM_DECLSPEC void atomic_ofi_unlock(int pe);

/* API functions */

int mca_atomic_ofi_init(bool enable_progress_threads, bool enable_threads);
int mca_atomic_ofi_finalize(void);
mca_atomic_base_module_t*
mca_atomic_ofi_query(int *priority);

int mca_atomic_ofi_fadd(void *target,
                        void *prev,
                        const void *value,
                        size_t nlong,
                        int pe,
                        struct oshmem_op_t *op);
int mca_atomic_ofi_cswap(void *target,
                         void *prev,
                         const void *cond,
                         const void *value,
                         size_t nlong,
                         int pe);
int mca_atomic_ofi_atomicto(void *target,
                        const void *value,
                        size_t nlong,
                        int pe,
                        struct oshmem_op_t *op);
int mca_atomic_ofi_swap(void *target,
                         void *prev,
                         const void *value,
                         size_t nlong,
                         int pe);

struct mca_atomic_ofi_module_t {
    mca_atomic_base_module_t super;
};
typedef struct mca_atomic_ofi_module_t mca_atomic_ofi_module_t;
OBJ_CLASS_DECLARATION(mca_atomic_ofi_module_t);

END_C_DECLS

#endif /* MCA_ATOMIC_MXM_H */
