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

#include "oshmem_config.h"
#include "orte/util/show_help.h"
#include "shmem.h"
#include "oshmem/runtime/params.h"
#include "oshmem/mca/spml/spml.h"
#include "oshmem/mca/spml/base/base.h"
#include "spml_ofshm_component.h"
#include "oshmem/mca/spml/ofshm/spml_ofshm.h"

#include "orte/util/show_help.h"
#include "opal/util/opal_environ.h"

static int mca_spml_ofshm_component_register(void);
static int mca_spml_ofshm_component_open(void);
static int mca_spml_ofshm_component_close(void);
static mca_spml_base_module_t*
mca_spml_ofshm_component_init(int *priority,
			bool enable_progress_threads,
			bool enable_mpi_threads);
static int mca_spml_ofshm_component_fini(void);
mca_spml_base_component_2_0_0_t mca_spml_ofshm_component = {

    /* First, the mca_base_component_t struct containing meta
       information about the component itself */

    {
      MCA_SPML_BASE_VERSION_2_0_0,

      "ofshm",                        /* MCA component name */
      OSHMEM_MAJOR_VERSION,           /* MCA component major version */
      OSHMEM_MINOR_VERSION,           /* MCA component minor version */
      OSHMEM_RELEASE_VERSION,         /* MCA component release version */
      mca_spml_ofshm_component_open,  /* component open */
      mca_spml_ofshm_component_close, /* component close */
      NULL,
      mca_spml_ofshm_component_register
    },
    {
	/* The component is checkpoint ready */
	MCA_BASE_METADATA_PARAM_CHECKPOINT
    },

    mca_spml_ofshm_component_init,    /* component init */
    mca_spml_ofshm_component_fini     /* component finalize */

};


static inline void mca_spml_ofshm_param_register_int(const char *param_name,
						int default_value,
						const char *help_msg,
						int *storage)
{
    *storage = default_value;
    (void) mca_base_component_var_register(&mca_spml_ofshm_component.spmlm_version,
                                           param_name,
                                           help_msg,
                                           MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           storage);
}

static inline void  mca_spml_ofshm_param_register_string(const char* param_name,
                                                    char* default_value,
                                                    const char *help_msg,
                                                    char **storage)
{
    *storage = default_value;
    (void) mca_base_component_var_register(&mca_spml_ofshm_component.spmlm_version,
                                           param_name,
                                           help_msg,
                                           MCA_BASE_VAR_TYPE_STRING, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           storage);
}

static int mca_spml_ofshm_component_register(void)
{

    mca_spml_ofshm_param_register_int("free_list_num/free_list_max", 32768,
                                      0,
                                      (int*)&mca_spml_ofshm.shmem_transport_ofi_queue_slots);
    mca_spml_ofshm_param_register_int("free_list_inc", 16,
                                      0,
                                      &mca_spml_ofshm.shmem_transport_ofi_qslot_inc);
    mca_spml_ofshm_param_register_int("priority", 25,
                                      "[integer] ofshm priority",
                                      &mca_spml_ofshm.priority);


    return OSHMEM_SUCCESS;
}

static void domain_resources_open(void){

	int ret;
	struct fi_cq_attr   cq_attr = {0};
	struct fi_av_attr   av_attr = {0};
	struct fi_cntr_attr cntr_attr = {0};

	/* ---------------------------------------------*/
	/* Define Resources for Domain to Attach to EP(s)  */
	/* ----------------------------------------------*/

	/* Create counter for counting completions of outgoing writes*/
	cntr_attr.events   = FI_CNTR_EVENTS_COMP;

	ret = fi_cntr_open(mca_spml_ofshm.shmem_transport_ofi_domainfd,
		&cntr_attr, &mca_spml_ofshm.shmem_transport_ofi_put_cntrfd, NULL);
	if(ret!=0){
		SPML_ERROR("put cntr_open failure ret=%d", ret);
	}


	/* Create counter for counting completions of outbound reads */
	cntr_attr.events   = FI_CNTR_EVENTS_COMP;

	ret = fi_cntr_open(mca_spml_ofshm.shmem_transport_ofi_domainfd, &cntr_attr,
		  &mca_spml_ofshm.shmem_transport_ofi_get_cntrfd, NULL);
	if(ret!=0){
		SPML_ERROR("get cntr_open failure ret=%d", ret);
	}

	/* Create CQ to be used for NB puts */
	cq_attr.format    = FI_CQ_FORMAT_CONTEXT;//event type for CQ,only context stored/reported
	cq_attr.size      = mca_spml_ofshm.shmem_transport_ofi_queue_slots;

	ret = fi_cq_open(mca_spml_ofshm.shmem_transport_ofi_domainfd, &cq_attr,
		    &mca_spml_ofshm.shmem_transport_ofi_put_nb_cqfd, NULL);
	if(ret!=0){
		SPML_ERROR("put_nb cq_open failure ret=%d", ret);
	}

#ifdef AV_TABLE
	/*AV table set-up for PE mapping*/
	av_attr.type   = FI_AV_TABLE;
#else
	av_attr.type   = FI_AV_MAP;
#endif

	if (mtl_ofi_enabled) {
		mca_mtl_ofi_module_t * mtl_ofi =
		      (mca_mtl_ofi_module_t *) ompi_mtl;
		mca_spml_ofshm.shmem_transport_ofi_avfd =
			mtl_ofi->av;

	} else {

		ret = fi_av_open(mca_spml_ofshm.shmem_transport_ofi_domainfd,
			&av_attr,
			&mca_spml_ofshm.shmem_transport_ofi_avfd,
			NULL);
		if(ret!=0){
			SPML_ERROR("av_open failure ret=%d", ret);
		}
	}

	/* -----------------------------------------*/
	/* POST enable Domain resources for to EP(s) */
	/* ------------------------------------------*/
	/* Create memory regions for incoming reads/writes
	* and outgoing non-blocking Puts, specifying entire VA range */

	ret = fi_mr_reg(mca_spml_ofshm.shmem_transport_ofi_domainfd, 0, UINT64_MAX,
		FI_REMOTE_READ | FI_REMOTE_WRITE, 0, 0ULL, 0,
		&mca_spml_ofshm.shmem_transport_ofi_target_mrfd, NULL);
	if(ret!=0){
		SPML_ERROR("mr_reg failure ret=%d", ret);
	}

	/* Create counter for incoming writes */
	cntr_attr.events   = FI_CNTR_EVENTS_COMP;
	cntr_attr.flags    = 0;

	ret = fi_cntr_open(mca_spml_ofshm.shmem_transport_ofi_domainfd, &cntr_attr,
		  &mca_spml_ofshm.shmem_transport_ofi_target_cntrfd, NULL);
	if(ret!=0){
		SPML_ERROR("rcntr open failure ret=%d", ret);
	}

	/* Bind counter with target memory region for incoming messages*/
	ret = fi_mr_bind(mca_spml_ofshm.shmem_transport_ofi_target_mrfd,
		&mca_spml_ofshm.shmem_transport_ofi_target_cntrfd->fid,
		FI_REMOTE_WRITE | FI_REMOTE_READ);
	if(ret!=0){
		SPML_ERROR("mr_bind failure ret=%d", ret);
	}



}


/*OFI Domain initialization */
static int mca_spml_ofshm_component_open(void)
{
	int ret;
	mca_mtl_ofi_module_t * mtl_ofi;
	struct fi_info      hints = {0}, *p_info = mca_spml_ofshm.p_info = NULL;
	struct fi_domain_attr domain_attr = {0};
	struct fi_fabric_attr fabric_attr = {0};
	struct fi_ep_attr   ep_attr = {0};
	struct fi_tx_attr	tx_attr = {0};
	mca_spml_ofshm.shmem_transport_ofi_max_buffered_send = sizeof(long double);

	char *provname = getenv("SHMEM_OFI_USE_PROVIDER");
        fabric_attr.prov_name = provname;

	hints.caps	  = FI_RMA |      /* request rma capability
					    implies FI_READ/WRITE FI_REMOTE_READ/WRITE */
		FI_ATOMICS; /* request atomics capability */
	hints.addr_format         = FI_FORMAT_UNSPEC;
	hints.mode		      = FI_CONTEXT;
	domain_attr.data_progress = FI_PROGRESS_AUTO;
	domain_attr.mr_mode       = FI_MR_SCALABLE;/* VA space-doesn't have to be pre-allocated */
	domain_attr.threading     = FI_THREAD_ENDPOINT; /* we promise to
							 serialize access
						       to endpoints.
							 we have only one
						       thread active at a time */

	hints.domain_attr         = &domain_attr;
	ep_attr.type		  = FI_EP_RDM;	/* reliable connectionless */
	tx_attr.op_flags          = FI_DELIVERY_COMPLETE | FI_INJECT_COMPLETE; /* completions indicate complete remotely */
	tx_attr.inject_size       = 0;
	hints.fabric_attr         = &fabric_attr;
	hints.tx_attr	          = &tx_attr;
	hints.rx_attr	          = NULL;
	hints.ep_attr             = &ep_attr;

#if MCA_oshmem_spml_DIRECT_CALL
	SPML_VERBOSE(50, "*** OFSHM component DIRECT MODE ENABLED ****");
#else
	SPML_VERBOSE(50, "*** OFSHM component NOT DIRECT MODE ****");
#endif
	SPML_VERBOSE(50, "*** OFSHM component open ****");
	if (ompi_mtl_base_selected_component &&
	   !strcmp(ompi_mtl_base_selected_component->mtl_version.mca_component_name, "ofi")) {
		mtl_ofi_enabled = 1;
	}

	if (mtl_ofi_enabled) {
		mtl_ofi = (mca_mtl_ofi_module_t *) ompi_mtl;
		mca_spml_ofshm.p_info = mtl_ofi->p_info;

	 } else {

		ret = fi_getinfo( FI_VERSION(SFI_MAJOR_VERSION,
			SFI_MINOR_VERSION), NULL, NULL, 0, &hints,
			&mca_spml_ofshm.p_info);
		if(ret!=0){
			SPML_ERROR("getinfo failure ret=%d", ret);
		}

	}

	p_info = mca_spml_ofshm.p_info;

	if (!p_info) {
		printf("fi_getinfo didn't find any providers\n");
	}

        if(p_info->tx_attr->inject_size < sizeof(long double)) {
		SPML_ERROR("inject size returned is too small\n");
        }
        else
		mca_spml_ofshm.shmem_transport_ofi_max_buffered_send = p_info->tx_attr->inject_size;

	/*Need to check if mtl-ofi is used --- special case for set-up if so */
	if (mtl_ofi_enabled) {
		SPML_VERBOSE(5,
			"NOTICE: Using mtl-ofi with spml-ofi: enabling endpoint sharing\n");
		mca_spml_ofshm.shmem_transport_ofi_fabfd =
			mtl_ofi->fabric;
		mca_spml_ofshm.shmem_transport_ofi_domainfd =
			mtl_ofi->domain;

	} else { /*no sharing needed:  make resources from scratch */

		/* fabric domain: define domain of resources physical and logical*/
		/* find fabric provider to use that is able to support RMA and ATOMICS */
		ret = fi_fabric(p_info->fabric_attr,
			&mca_spml_ofshm.shmem_transport_ofi_fabfd, NULL);
		if(ret!=0){
			SPML_ERROR("fi_fabric failure ret=%d", ret);
		}

		/*access domain: define communication resource
		 *limits/boundary within fabric domain */
		ret = fi_domain(mca_spml_ofshm.shmem_transport_ofi_fabfd, p_info,
				&mca_spml_ofshm.shmem_transport_ofi_domainfd,NULL);
		if(ret!=0){
			SPML_ERROR("fi_domain failure ret=%d", ret);
		}

	}

	if(provname)
		p_info->fabric_attr->prov_name = NULL;

	if (mtl_ofi_enabled) {
		mca_spml_ofshm.shmem_transport_ofi_stx =
			mtl_ofi->stx;
	} else {
		/*transmit context: allocate one transmit context for this SHMEM PE
		* and share it across different multiple endpoints. Since we have only
		* one thread per PE, a single context is sufficient and allows more
		* more PEs/node (i.e. doesn't exhaust contexts)  */
		ret = fi_stx_context(mca_spml_ofshm.shmem_transport_ofi_domainfd, NULL,
				&mca_spml_ofshm.shmem_transport_ofi_stx, NULL);
		if(ret!=0) {
			SPML_ERROR("stx_context failure ret=%d", ret);
		}
	}

	domain_resources_open();


	return OSHMEM_SUCCESS;
}

static int mca_spml_ofshm_component_close(void)
{
	SPML_VERBOSE(50, "*** OFSHM component close ****");

        fi_freeinfo(mca_spml_ofshm.p_info);

#ifndef AV_TABLE
	free(addr_table);
#endif

	if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_stx->fid)) {
		SPML_ERROR("ofi_stx close failure");
	}

	if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_target_mrfd->fid)) {
		SPML_ERROR("target_mrfd close failure");
	}

	if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_put_nb_cqfd->fid)) {
		SPML_ERROR("put_nb close failure");
	}

	if(fi_close(&mca_spml_ofshm.shmem_transport_ofi_put_cntrfd->fid)){
		SPML_ERROR("put_cntrfd close failure");
	}

	if(fi_close(&mca_spml_ofshm.shmem_transport_ofi_get_cntrfd->fid)){
		SPML_ERROR("get_cntr close failure");
	}

	if(fi_close(&mca_spml_ofshm.shmem_transport_ofi_target_cntrfd->fid)){
		SPML_ERROR("target_cntrfd close failure");
	}

	if(!mtl_ofi_enabled) {
		if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_avfd->fid)) {
			SPML_ERROR("avfd close failure");
		}

		if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_domainfd->fid)) {
			SPML_ERROR("domainfd close failure");
		}

		if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_fabfd->fid)) {
			SPML_ERROR("fabfd close failure");
		}
	}

	SPML_VERBOSE(50, "*** OFSHM component close DONE ****");
	return OSHMEM_SUCCESS;
}


static int spml_ofshm_ofi_init(void)
{
	int ret;
	struct fi_info      *p_info = mca_spml_ofshm.p_info;

	/* ------------------------------------*/
	/* 		Allocate Endpoints	   */
	/* ------------------------------------*/

	/* this endpoint is used to get completion events and
	* used to expose memory to incoming reads/writes */
	p_info->caps  = FI_RMA |     /* request rma capability
                                            implies FI_READ/WRITE FI_REMOTE_READ/WRITE */
            FI_ATOMICS; /* request atomics capability */
	p_info->ep_attr->tx_ctx_cnt = FI_SHARED_CONTEXT;

        p_info->tx_attr->op_flags = FI_DELIVERY_COMPLETE;
	ret = fi_endpoint(mca_spml_ofshm.shmem_transport_ofi_domainfd,
		    p_info, &mca_spml_ofshm.shmem_transport_ofi_epfd, NULL);
	if(ret!=0){
		SPML_ERROR("fi_endpoint open failure ret=%d", ret);
	}

	/* this endpoint is used only to get read and write
	* counter updates */
	p_info->caps	= FI_RMA | FI_WRITE | FI_READ | /*SEND ONLY */
			FI_ATOMICS; /* request atomics capability */

        p_info->tx_attr->op_flags = FI_DELIVERY_COMPLETE | FI_INJECT_COMPLETE;
	ret = fi_endpoint(mca_spml_ofshm.shmem_transport_ofi_domainfd,
		    p_info, &mca_spml_ofshm.shmem_transport_ofi_cntr_epfd, NULL);
	if(ret!=0){
		SPML_ERROR("fi_endpoint2 open failure ret=%d", ret);
	}

		/*   Bind and enable 	*/

	/* attach the endpoints to the shared context */
	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_epfd,
		    &mca_spml_ofshm.shmem_transport_ofi_stx->fid, 0);
	if(ret!=0){
		SPML_ERROR("bind epfd2stx failure ret=%d", ret);
	}

	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd,
		    &mca_spml_ofshm.shmem_transport_ofi_stx->fid, 0);
	if(ret!=0){
		SPML_ERROR("bind cntrep2stx failure ret=%d", ret);
	}

	/* attaching to endpoint enables counting "writes"
	*for calls used with this endpoint*/
	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd,
		    &mca_spml_ofshm.shmem_transport_ofi_put_cntrfd->fid, FI_WRITE);
	if(ret!=0){
		SPML_ERROR("bind ep2putcntr failure ret=%d", ret);
	}

	/* attach to endpoint */
	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd,
		&mca_spml_ofshm.shmem_transport_ofi_get_cntrfd->fid, FI_READ);
	if(ret!=0){
		SPML_ERROR("bind ep2getcntrfailure ret=%d", ret);
	}

	/* attach CQ for obtaining completions for large puts (NB puts) */
	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_epfd,
		    &mca_spml_ofshm.shmem_transport_ofi_put_nb_cqfd->fid, FI_SEND);
	if(ret!=0){
		SPML_ERROR("bind ep2cq failure ret=%d", ret);
	}

	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_epfd,
			&mca_spml_ofshm.shmem_transport_ofi_avfd->fid, 0);
	if(ret!=0){
		SPML_ERROR("bind ep2av failure ret=%d", ret);
	}

	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd,
			&mca_spml_ofshm.shmem_transport_ofi_avfd->fid, 0);
	if(ret!=0){
		SPML_ERROR("bind cntr2av failure ret=%d", ret);
	}

	/*enable active endpoint state: can now perform data transfers*/
	ret = fi_enable(mca_spml_ofshm.shmem_transport_ofi_epfd);
	if(ret!=0){
		SPML_ERROR("fi_enable epfd failure ret=%d", ret);
	}
	ret = fi_enable(mca_spml_ofshm.shmem_transport_ofi_cntr_epfd);
	if(ret!=0){
		SPML_ERROR("fi_enable failure ret=%d", ret);
	}

	/* ------------------------------------*/
	/* 		Post Enable	       */
	/* ------------------------------------*/

	/*bind to endpoint, incoming communication associated
	* with endpoint now has defined resources*/
	ret = fi_ep_bind(mca_spml_ofshm.shmem_transport_ofi_epfd,
		    &mca_spml_ofshm.shmem_transport_ofi_target_mrfd->fid,
		    FI_REMOTE_READ | FI_REMOTE_WRITE);
	if(ret!=0){
		SPML_ERROR("ep_bind mr2ep failure ret=%d", ret);
	}

	return OSHMEM_SUCCESS;
}

static mca_spml_base_module_t*
mca_spml_ofshm_component_init(int* priority,
                              bool enable_progress_threads,
                              bool enable_mpi_threads)
{

	SPML_VERBOSE( 10, "in OFSHM, my priority is %d\n", mca_spml_ofshm.priority);

	if ((*priority) > mca_spml_ofshm.priority) {
		*priority = mca_spml_ofshm.priority;
		return NULL ;
	}

	*priority = mca_spml_ofshm.priority;

	if (OSHMEM_SUCCESS != spml_ofshm_ofi_init())
		return NULL ;

	SPML_VERBOSE(50, "*** OFSHM component init ****");

	return &mca_spml_ofshm.super;
}

static int mca_spml_ofshm_component_fini(void)
{
	SPML_VERBOSE(50, "*** OFSHM component fini ****");
	if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_epfd->fid)) {
		SPML_ERROR("epfd close failure");
	}

	if (fi_close(&mca_spml_ofshm.shmem_transport_ofi_cntr_epfd->fid)) {
		SPML_ERROR("cntr_epfd close failure");
	}

	SPML_VERBOSE(50, "*** OFSHM component fini DONE ****");
	return OSHMEM_SUCCESS;
}
