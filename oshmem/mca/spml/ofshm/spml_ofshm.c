/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved 
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#define _GNU_SOURCE
#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#include "oshmem_config.h"
#include "opal/datatype/opal_convertor.h"
#include "orte/include/orte/types.h"
#include "orte/runtime/orte_globals.h"
#include "oshmem/mca/spml/ofshm/spml_ofshm.h"
#include "oshmem/include/shmem.h"
#include "oshmem/mca/memheap/memheap.h"
#include "oshmem/mca/memheap/base/base.h"
#include "oshmem/proc/proc.h"
#include "oshmem/mca/spml/base/base.h"
#include "oshmem/mca/spml/base/spml_base_putreq.h"
#include "oshmem/runtime/runtime.h"
#include "orte/util/show_help.h"

#include "oshmem/mca/spml/ofshm/spml_ofshm_component.h"

/* Turn ON/OFF debug output from build (default 0) */
#ifndef SPML_OFSHM_PUT_DEBUG
#define SPML_OFSHM_PUT_DEBUG    0
#endif

#define SHMEM_TRANSPORT_SFI_TYPE_BOUNCE 0x01
#define SHMEM_TRANSPORT_SFI_TYPE_LONG   0x02

fi_addr_t *addr_table;

short mtl_ofi_enabled;

typedef struct fi_context fi_context_t;

struct shmem_transport_ofi_frag_t {
    opal_free_list_item_t item;
    char mytype;
    fi_context_t context;
};

typedef struct shmem_transport_ofi_frag_t shmem_transport_ofi_frag_t;

struct shmem_transport_ofi_bounce_buffer_t {
    shmem_transport_ofi_frag_t frag;
    char *data;
};

typedef struct shmem_transport_ofi_bounce_buffer_t shmem_transport_ofi_bounce_buffer_t;

struct shmem_transport_ofi_long_frag_t {
    shmem_transport_ofi_frag_t frag;
    int reference;
};

typedef struct shmem_transport_ofi_long_frag_t shmem_transport_ofi_long_frag_t;

mca_spml_ofshm_t mca_spml_ofshm = {
    {
        /* Init mca_spml_base_module_t */
        mca_spml_ofshm_add_procs,
        mca_spml_ofshm_del_procs,
        mca_spml_ofshm_enable,
        mca_spml_ofshm_register,
        mca_spml_ofshm_deregister,
        mca_spml_ofshm_oob_get_mkeys,
        mca_spml_ofshm_put,
        mca_spml_ofshm_put_nb,
        mca_spml_ofshm_get,
        mca_spml_ofshm_recv,
        mca_spml_ofshm_send,
        mca_spml_ofshm_wait,
        mca_spml_base_wait_nb,
        mca_spml_ofshm_fence,
        (void*)&mca_spml_ofshm
    }
};

static void construct_long_frag(shmem_transport_ofi_long_frag_t* req)
{
	req->frag.mytype = SHMEM_TRANSPORT_SFI_TYPE_LONG;
	req->reference = 0;
}

static void destruct_long_frag(shmem_transport_ofi_bounce_buffer_t* req)
{
}

OBJ_CLASS_INSTANCE( shmem_transport_ofi_long_frag_t,
                   opal_free_list_item_t,
                   construct_long_frag,
                   destruct_long_frag);


static void construct_bounce_buffer(shmem_transport_ofi_bounce_buffer_t* req)
{
	req->frag.mytype = SHMEM_TRANSPORT_SFI_TYPE_BOUNCE;
	req->data = (char *) malloc(BOUNCE_BUFFER_SIZE*(sizeof(char)));
}

static void destruct_bounce_buffer(shmem_transport_ofi_bounce_buffer_t* req)
{
	free(req->data);
}

OBJ_CLASS_INSTANCE( shmem_transport_ofi_bounce_buffer_t,
                   opal_free_list_item_t,
                   construct_bounce_buffer,
                   destruct_bounce_buffer);


/*going to use get free list for another "put free list" component
 *NOTE: frag usage is blocking do not need more than 1 buffer*/
int mca_spml_ofshm_enable(bool enable)
{
	SPML_VERBOSE(50, "*** ofshm ENABLED ****");

	opal_free_list_init(&mca_spml_base_put_requests,
			sizeof(shmem_transport_ofi_bounce_buffer_t),
			opal_cache_line_size,
                       OBJ_CLASS(shmem_transport_ofi_bounce_buffer_t),
                       0,
                       opal_cache_line_size,
                       mca_spml_ofshm.shmem_transport_ofi_queue_slots,
                       mca_spml_ofshm.shmem_transport_ofi_queue_slots,
                       mca_spml_ofshm.shmem_transport_ofi_qslot_inc,
                       NULL, 0, NULL, NULL, NULL);

	opal_free_list_init(&mca_spml_base_get_requests,
			sizeof(shmem_transport_ofi_long_frag_t),
			opal_cache_line_size,
                       OBJ_CLASS(shmem_transport_ofi_long_frag_t),
                       0,
                       opal_cache_line_size,
                       1,
                       1,
                       1,
                       NULL, 0, NULL, NULL, NULL);

	return OSHMEM_SUCCESS;
}

int mca_spml_ofshm_del_procs(oshmem_proc_t** procs, size_t nprocs)
{
	uint64_t i;
	oshmem_proc_t *proc;

	for(i = 0; i < nprocs; i++) {
		proc = oshmem_proc_group_find(oshmem_group_all, i);
		if (proc->transport_ids)
			free(proc->transport_ids);
	}

	return OSHMEM_SUCCESS;
}

int mca_spml_ofshm_oob_get_mkeys(int pe,
                                        uint32_t seg,
                                        sshmem_mkey_t *mkeys)
{
	return OSHMEM_SUCCESS;
}

/*PMI exchange */
int mca_spml_ofshm_add_procs(oshmem_proc_t** procs, size_t nprocs)
{
	int ret;
	uint64_t i;
	oshmem_proc_t *proc;
	char  epname[128];
	char * alladdrs;
	size_t epnamelen = sizeof(epname);
	int rank = oshmem_my_proc_id();

	/*all communication uses same "channel/group" / only one transport at a time */
	for(i = 0; i < nprocs; i++) {
		proc = oshmem_proc_group_find(oshmem_group_all, i);
		proc->transport_ids = (char *) malloc(sizeof(char));
		proc->transport_ids[0] = '0';
		procs[i]->num_transports = 1;
	}

#ifdef AV_TABLE
	/*AV table set-up for PE mapping*/
	addr_table     = NULL;
#else
	addr_table = (fi_addr_t *) malloc(sizeof(fi_addr_t)*nprocs);
#endif


	/* grab mtl-ofi av table */
	if (mtl_ofi_enabled) {
		mca_mtl_ofi_endpoint_t *mtl_ofi_ep;

		for(i = 0; i < nprocs; i++) {
			mtl_ofi_ep =
				(mca_mtl_ofi_endpoint_t*)
				procs[i]->proc_endpoints[OMPI_PROC_ENDPOINT_TAG_MTL];
			addr_table[i] = mtl_ofi_ep->peer_fiaddr;
		}

		return OSHMEM_SUCCESS;
	}

	ret = fi_getname((fid_t)mca_spml_ofshm.shmem_transport_ofi_epfd, epname, &epnamelen);
	if(ret!=0 || (epnamelen > sizeof(epname))){
		SPML_ERROR("getname failure ret=%d", ret);
	}

	/* Exchange Endpoint information */

	alladdrs = malloc(nprocs * epnamelen);

	oshmem_shmem_allgather(epname, alladdrs, epnamelen);

	ret = fi_av_insert(mca_spml_ofshm.shmem_transport_ofi_avfd,
			alladdrs,
			nprocs,
			addr_table,
			0,
			NULL);
	if(ret!= (int)nprocs){
		SPML_ERROR("AV Insert FAIL ret=%d pe=%d epname=%s", ret, rank, epname);
	}

	free(alladdrs);

	SPML_VERBOSE(50, "*** AV INSERTION FINISHED rank=%d****", rank);
	return OSHMEM_SUCCESS;
}

/* do not need to re-register with fabric, whole address space is exposed by default*/
sshmem_mkey_t *mca_spml_ofshm_register(void* addr,
                                         size_t size,
                                         uint64_t shmid,
                                         int *count)
{
	return NULL ;
}

int mca_spml_ofshm_deregister(sshmem_mkey_t *mkeys)
{
	return 0;
}


static inline void ofshm_get_wait(void)
{
	int ret;

	/* wait for get counter to meet outstanding count value    */
	ret = fi_cntr_wait(mca_spml_ofshm.shmem_transport_ofi_get_cntrfd,
			mca_spml_ofshm.shmem_transport_ofi_pending_get_counter,-1);
	if (ret) {
		SPML_ERROR("cntr_wait failure ret=%d", ret);
	}

}

int mca_spml_ofshm_get(void *src_addr, size_t size, void *dst_addr, int src)
{
	int ret;
        uint64_t dst = (uint64_t) src;

	ret = fi_read(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd,
			dst_addr,
			size,
			NULL,
			GET_DEST(dst),
			(uint64_t) src_addr,
			0,
			NULL);
	if (ret) {
		SPML_ERROR("fi_read failure ret=%d", ret);
	}

	mca_spml_ofshm.shmem_transport_ofi_pending_get_counter++;

	ofshm_get_wait();

	return OSHMEM_SUCCESS;
}

/*N/A*/
int mca_spml_ofshm_put_nb(void* dst_addr,
                          size_t size,
                          void* src_addr,
                          int dst,
                          void **handle)
{
	return OSHMEM_SUCCESS;
}


static inline
int
shmem_transport_ofi_drain_cq(void)
{

	ssize_t ret = 0;
	struct fi_cq_entry buf;

	SPML_VERBOSE(50, "*** OFSHM CQ Drain ****");

	do
	{
		ret = fi_cq_read(mca_spml_ofshm.shmem_transport_ofi_put_nb_cqfd,
				(void *)&buf, 1);
		/*error cases*/
		if (ret < 0 && ret != -FI_EAGAIN ) {
			if(ret == -FI_EAVAIL) {
				struct fi_cq_err_entry e;
				fi_cq_readerr(mca_spml_ofshm.shmem_transport_ofi_put_nb_cqfd,
				              (void *)&e, 0);
				SPML_ERROR("fi_cq_read failure e.err=%d", e.err);
			}
			else
				SPML_ERROR("fi_cq_read failure ret=%d", ret);
		}

	} while(ret == -FI_EAGAIN);

	mca_spml_ofshm.shmem_transport_ofi_queue_slots++;

	shmem_transport_ofi_frag_t *frag =
        container_of(buf.op_context, struct shmem_transport_ofi_frag_t,
		     context);

	if(SHMEM_TRANSPORT_SFI_TYPE_BOUNCE == frag->mytype) {
		opal_free_list_return(&mca_spml_base_put_requests,
				(opal_free_list_item_t*) frag);

		mca_spml_ofshm.shmem_transport_ofi_pending_cq_count--;

	} else {
		shmem_transport_ofi_long_frag_t *long_frag =
		(shmem_transport_ofi_long_frag_t*) frag;

		assert(long_frag->frag.mytype == SHMEM_TRANSPORT_SFI_TYPE_LONG);

		if (0 >= --long_frag->reference) {
			long_frag->reference = 0;
			opal_free_list_return(&mca_spml_base_get_requests,
				(opal_free_list_item_t*) frag);
		}
		mca_spml_ofshm.shmem_transport_ofi_pending_cq_count--;
	}

	return 0;

}


/*only need to force completion with frag put */
int mca_spml_ofshm_put(void* dst_addr, size_t size, void* src_addr, int dst)
{
	int ret;
        uint64_t dst_in = (uint64_t) dst;

	SPML_VERBOSE(50, "*** OFSHM PUT ****");

	if (size <= mca_spml_ofshm.shmem_transport_ofi_max_buffered_send) {

		ret = fi_inject_write(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd,
					src_addr,
					size,
					GET_DEST(dst_in),
					(uint64_t) dst_addr,
					0);
		/* automatically get local completion but need remote completion for fence/quiet*/
		mca_spml_ofshm.shmem_transport_ofi_pending_put_counter++;

		if (ret) {
			return OSHMEM_ERROR;
                }

	} else if (size <= BOUNCE_BUFFER_SIZE) {

		SPML_VERBOSE(50, "*** OFSHM BB PUT ****");

		while(0 >= --mca_spml_ofshm.shmem_transport_ofi_queue_slots) {
			mca_spml_ofshm.shmem_transport_ofi_queue_slots++;
			shmem_transport_ofi_drain_cq();
		}

		opal_free_list_item_t* item;

		item = opal_free_list_wait(&mca_spml_base_put_requests);

		shmem_transport_ofi_bounce_buffer_t *buff =
			(shmem_transport_ofi_bounce_buffer_t *)	item;

		/*if LL empty = error, should've been avoided with EQ drain*/
		if (NULL == buff)
			return OSHMEM_ERROR;

		assert(buff->frag.mytype == SHMEM_TRANSPORT_SFI_TYPE_BOUNCE);

		memcpy(buff->data, src_addr, size);

		ret = fi_write(mca_spml_ofshm.shmem_transport_ofi_epfd,
				buff->data, size, NULL,
				GET_DEST(dst_in), (uint64_t) dst_addr, 0,
				&buff->frag.context);
		if (ret) {
			return OSHMEM_ERROR;
                }

		mca_spml_ofshm.shmem_transport_ofi_pending_cq_count++;

	} else {
		SPML_VERBOSE(50, "*** OFSHM FRAG PUT ****");

		while(0 >= --mca_spml_ofshm.shmem_transport_ofi_queue_slots) {
			mca_spml_ofshm.shmem_transport_ofi_queue_slots++;
			shmem_transport_ofi_drain_cq();
		}

		opal_free_list_item_t* item;
		shmem_transport_ofi_long_frag_t * long_frag;

		item = opal_free_list_wait(&mca_spml_base_get_requests);

		long_frag = (shmem_transport_ofi_long_frag_t*) item;

		if (NULL == long_frag)
			return OSHMEM_ERROR;

		assert(long_frag->frag.mytype == SHMEM_TRANSPORT_SFI_TYPE_LONG);
		assert(long_frag->reference == 0);

		ret = fi_write(mca_spml_ofshm.shmem_transport_ofi_epfd,
				src_addr, size, NULL,
				GET_DEST(dst_in), (uint64_t) dst_addr, 0, &long_frag->frag.context);
		if (ret) {
			return OSHMEM_ERROR;
		}

		long_frag->reference++;
		mca_spml_ofshm.shmem_transport_ofi_pending_cq_count++;

		/*frag's are blocking only BB is not */
		shmem_transport_ofi_drain_cq();
	}


	return OSHMEM_SUCCESS;

}


int mca_spml_ofshm_fence(void)
{
	int ret;
	SPML_VERBOSE(50, "*** OFSHM FENCE cntr = %d****", mca_spml_ofshm.shmem_transport_ofi_pending_put_counter);

	while(mca_spml_ofshm.shmem_transport_ofi_pending_cq_count) {
		ret = shmem_transport_ofi_drain_cq();
		if (ret)
			return ret;
	}

       /* wait for put counter to meet outstanding count value    */
        ret = fi_cntr_wait(mca_spml_ofshm.shmem_transport_ofi_put_cntrfd,
                     mca_spml_ofshm.shmem_transport_ofi_pending_put_counter,-1);

	SPML_VERBOSE(50, "*** OFSHM FENCE DONE ****");
	if(ret)
		return OSHMEM_ERROR;
	else
		return OSHMEM_SUCCESS;
}


#define SPML_BASE_DO_CMP(res, addr, op, val) \
    switch((op)) { \
        case SHMEM_CMP_EQ: \
            res = *(addr) == (val) ? 1 : 0; \
            break; \
        case SHMEM_CMP_NE: \
            res = *(addr) != (val) ? 1 : 0; \
            break; \
        case SHMEM_CMP_GT: \
            res =  *(addr) > (val) ? 1 : 0; \
            break; \
        case SHMEM_CMP_LE: \
            res = *(addr) <= (val) ? 1 : 0; \
            break; \
        case SHMEM_CMP_LT: \
            res = *(addr) < (val) ? 1: 0; \
            break; \
        case SHMEM_CMP_GE: \
            res = *(addr) >= (val) ? 1 : 0; \
            break; \
    }

#define SHMEM_WAIT_UNTIL(var, cond, value)                              \
    do {                                                                \
        int ret, cmpret;                                                \
	uint64_t count;							\
        SPML_BASE_DO_CMP(cmpret, var, cond, value); 			\
	while(!cmpret) {						\
	   count =  fi_cntr_read(mca_spml_ofshm.shmem_transport_ofi_target_cntrfd);    	\
           COMPILER_FENCE();                                            \
	   SPML_BASE_DO_CMP(cmpret, var, cond, value); 			\
	   if (cmpret) break;                                           \
	   ret = fi_cntr_wait(mca_spml_ofshm.shmem_transport_ofi_target_cntrfd, 	        \
			      (count+1),-1);                     		\
	   if (ret) { exit(1); }				\
	   SPML_BASE_DO_CMP(cmpret, var, cond, value);		\
	}								\
    } while(0)

int mca_spml_ofshm_wait(void* addr, int cmp, void* value, int datatype)
{
    int *int_addr, int_value;
    long *long_addr, long_value;
    short *short_addr, short_value;
    long long *longlong_addr, longlong_value;
    int32_t *int32_addr, int32_value;
    int64_t *int64_addr, int64_value;

    switch (datatype) {

    /* Int */
    case SHMEM_INT:
        int_value = *(int*) value;
        int_addr = (int*) addr;
	SHMEM_WAIT_UNTIL(int_addr, cmp, int_value);
	break;

        /* Short */
    case SHMEM_SHORT:
        short_value = *(short*) value;
        short_addr = (short*) addr;
	SHMEM_WAIT_UNTIL(short_addr, cmp, short_value);
        break;

        /* Long */
    case SHMEM_LONG:
        long_value = *(long*) value;
        long_addr = (long*) addr;
	SHMEM_WAIT_UNTIL(long_addr, cmp, long_value);
        break;

        /* Long-Long */
    case SHMEM_LLONG:
        longlong_value = *(long long*) value;
        longlong_addr = (long long*) addr;
	SHMEM_WAIT_UNTIL(longlong_addr, cmp, longlong_value);
        break;

       /* Int32_t*/
    case SHMEM_INT32_T:
        int32_value = *(int32_t*) value;
        int32_addr = (int32_t*) addr;
	SHMEM_WAIT_UNTIL(int32_addr, cmp, int32_value);
        break;

       /* Int64_t */
    case SHMEM_INT64_T:
        int64_value = *(int64_t*) value;
        int64_addr = (int64_t*) addr;
	SHMEM_WAIT_UNTIL(int64_addr, cmp, int64_value);
        break;
    }

    return OSHMEM_SUCCESS;
}

/*todo: used for scoll basic */
int mca_spml_ofshm_recv(void* buf, size_t size, int src)
{
	SPML_VERBOSE(10, "WARNING: send/recv NOT implemented yet in spml-ofi");
	return OSHMEM_SUCCESS;
}

/*todo: used for scoll basic */
int mca_spml_ofshm_send(void* buf,
                        size_t size,
                        int dst,
                        mca_spml_base_put_mode_t mode)
{
	SPML_VERBOSE(10, "WARNING: send/recv NOT implemented yet in spml-ofi");
	return OSHMEM_SUCCESS;
}
