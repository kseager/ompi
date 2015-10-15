/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved 
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef MCA_SPML_UD_OFI_H
#define MCA_SPML_UD_OFI_H

#include "oshmem_config.h"
#include "oshmem/request/request.h"
#include "oshmem/mca/spml/spml.h"
#include "oshmem/util/oshmem_util.h"
#include "oshmem/mca/spml/base/spml_base_putreq.h"
#include "oshmem/proc/proc.h"
#include "oshmem/mca/spml/base/spml_base_request.h"
#include "oshmem/mca/spml/base/spml_base_getreq.h"
#include "oshmem/mca/spml/base/base.h"
#include "ompi/mca/mtl/ofi/mtl_ofi.h"
#include "ompi/mca/bml/base/base.h"
#include "opal/class/opal_list.h"

#include "orte/runtime/orte_globals.h"

#include <rdma/fabric.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_tagged.h>
#include <rdma/fi_rma.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_atomic.h>
#include <unistd.h>


#define SFI_MAJOR_VERSION 1
#define SFI_MINOR_VERSION 0

extern short mtl_ofi_enabled;

extern fi_addr_t *addr_table;

/* map by default unless statically defined otherwise
 * -- WARNING cannot use table with mtl-ofi*/

#ifdef AV_TABLE
#define GET_DEST(dest) (fi_addr_t)dest
#else
#define GET_DEST(dest) (fi_addr_t)(addr_table[dest])
#endif

BEGIN_C_DECLS

typedef struct fid_fabric* fabric_t;
typedef struct fid_domain* domain_t;
typedef struct fid_ep*     ep_t;
typedef struct fid_sep*    sep_t;
typedef struct fid_av*     av_t;
typedef struct fid_cq*     cq_t;
typedef struct fid_cntr*   cntr_t;
typedef struct fid_mr*     mr_t;
typedef struct fid_stx*    stx_t;

#define BOUNCE_BUFFER_SIZE 2048

/*definition of resources */
struct mca_spml_ofshm_t {
    mca_spml_base_module_t super;

    fabric_t                shmem_transport_ofi_fabfd;
    domain_t                shmem_transport_ofi_domainfd;
    ep_t                    shmem_transport_ofi_epfd;
    ep_t                    shmem_transport_ofi_cntr_epfd;
    stx_t                   shmem_transport_ofi_stx;
    av_t                    shmem_transport_ofi_avfd;
    cq_t                    shmem_transport_ofi_put_nb_cqfd;
    cntr_t                  shmem_transport_ofi_target_cntrfd;
    cntr_t                  shmem_transport_ofi_put_cntrfd;
    cntr_t                  shmem_transport_ofi_get_cntrfd;
    mr_t                    shmem_transport_ofi_target_mrfd;
    uint64_t                shmem_transport_ofi_pending_put_counter;
    uint64_t                shmem_transport_ofi_pending_get_counter;
    uint64_t                shmem_transport_ofi_pending_cq_count;
    size_t                  shmem_transport_ofi_max_buffered_send;
    size_t 	            shmem_transport_ofi_max_atomic_size;
    size_t      	    shmem_transport_ofi_queue_slots;
    int         	    shmem_transport_ofi_qslot_inc;
    int                     priority;
    struct fi_info	    *p_info;
};

#define COMPILER_FENCE() do { __asm__ __volatile__ ("" ::: "memory"); } while (0)

typedef struct mca_spml_ofshm_t mca_spml_ofshm_t;

extern mca_spml_ofshm_t mca_spml_ofshm;

extern int mca_spml_ofshm_enable(bool enable);
extern int mca_spml_ofshm_get(void* dst_addr,
                              size_t size,
                              void* src_addr,
                              int src);
/* extension. used 4 fence implementation b4 fence was added to ofi */
extern int mca_spml_ofshm_get_async(void *src_addr,
                                    size_t size,
                                    void *dst_addr,
                                    int src);

extern int mca_spml_ofshm_put(void* dst_addr,
                              size_t size,
                              void* src_addr,
                              int dst);
extern int mca_spml_ofshm_put_nb(void* dst_addr,
                                 size_t size,
                                 void* src_addr,
                                 int dst,
                                 void **handle);

extern int mca_spml_ofshm_wait(void* addr,
				 int cmp,
				 void* value,
				 int datatype);

extern int mca_spml_ofshm_recv(void* buf, size_t size, int src);
extern int mca_spml_ofshm_send(void* buf,
                               size_t size,
                               int dst,
                               mca_spml_base_put_mode_t mode);

extern sshmem_mkey_t *mca_spml_ofshm_register(void* addr,
                                                size_t size,
                                                uint64_t shmid,
                                                int *count);
extern int mca_spml_ofshm_deregister(sshmem_mkey_t *mkeys);
extern int mca_spml_ofshm_oob_get_mkeys(int pe,
                                        uint32_t seg,
                                        sshmem_mkey_t *mkeys);

extern int mca_spml_ofshm_add_procs(oshmem_proc_t** procs, size_t nprocs);
extern int mca_spml_ofshm_del_procs(oshmem_proc_t** procs, size_t nprocs);
extern int mca_spml_ofshm_fence(void);
extern int spml_ofshm_progress(void);

END_C_DECLS

#endif
