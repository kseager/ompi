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

#include "oshmem/constants.h"
#include "oshmem/mca/atomic/atomic.h"
#include "oshmem/mca/atomic/base/base.h"
#include "oshmem/mca/spml/base/base.h"

#include "atomic_ofi.h"


/*
 * Public string showing the scoll ofi component version number
 */
const char *mca_atomic_ofi_component_version_string =
"Open SHMEM ofi atomic MCA component version " OSHMEM_VERSION;

/*
 * Global variable
 */
mca_spml_ofshm_t *mca_spml_self = NULL;

/*
 * Local function
 */
static int _ofi_register(void);
static int _ofi_open(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

mca_atomic_base_component_t mca_atomic_ofi_component = {

    /* First, the mca_component_t struct containing meta information
       about the component itself */

    {
        MCA_ATOMIC_BASE_VERSION_2_0_0,

        /* Component name and version */
        "ofi",
        OSHMEM_MAJOR_VERSION,
        OSHMEM_MINOR_VERSION,
        OSHMEM_RELEASE_VERSION,

        /* component open */
        _ofi_open,
        /* component close */
        NULL,
        /* component query */
        NULL,
        /* component register */
        _ofi_register
    },
    {
        /* The component is checkpoint ready */
        MCA_BASE_METADATA_PARAM_CHECKPOINT
    },

    /* Initialization / querying functions */

    mca_atomic_ofi_init,
    mca_atomic_ofi_finalize,
    mca_atomic_ofi_query
};

static int _ofi_register(void)
{
    mca_atomic_ofi_component.priority = 100;
    mca_base_component_var_register (&mca_atomic_ofi_component.atomic_version,
                                     "priority", "Priority of the atomic:ofi "
                                     "component (default: 100)", MCA_BASE_VAR_TYPE_INT,
                                     NULL, 0, MCA_BASE_VAR_FLAG_SETTABLE,
                                     OPAL_INFO_LVL_3,
                                     MCA_BASE_VAR_SCOPE_ALL_EQ,
                                     &mca_atomic_ofi_component.priority);

    return OSHMEM_SUCCESS;
}

static int _ofi_open(void)
{
    /*
     * This component is able to work using spml:ofshm component only
     * (this check is added instead of !mca_spml_ofshm.enabled)
     */
    if (strcmp(mca_spml_base_selected_component.spmlm_version.mca_component_name, "ofshm")) {
        ATOMIC_VERBOSE(5,
                       "Can not use atomic/ofi because spml ofshm component disabled");
        return OSHMEM_ERR_NOT_AVAILABLE;
    }
    mca_spml_self = (mca_spml_ofshm_t *)mca_spml.self;

    return OSHMEM_SUCCESS;
}

inline void ofshm_get_wait(void)
{
	int ret;

	/* wait for get counter to meet outstanding count value    */
	ret = fi_cntr_wait(mca_spml_self->shmem_transport_ofi_get_cntrfd,
			mca_spml_self->shmem_transport_ofi_pending_get_counter,-1);
	if (ret) {
		SPML_ERROR("atomic cntr_wait failure ret=%d", ret);
	}

}

OBJ_CLASS_INSTANCE(mca_atomic_ofi_module_t,
                   mca_atomic_base_module_t,
                   NULL,
                   NULL);
