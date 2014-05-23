/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2008 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC.  All rights
 *                         reserved.
 * Copyright (c) 2014      Research Organization for Information Science
 *                         and Technology (RIST). All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"
#include <stdio.h>

#include "ompi/mpi/c/bindings.h"
#include "ompi/runtime/params.h"
#include "ompi/communicator/communicator.h"
#include "ompi/errhandler/errhandler.h"
#include "ompi/datatype/ompi_datatype.h"
#include "ompi/memchecker.h"
#include "ompi/communicator/comm_helpers.h"

#if OPAL_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Neighbor_alltoallw = PMPI_Neighbor_alltoallw
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Neighbor_alltoallw";


int MPI_Neighbor_alltoallw(const void *sendbuf, const int sendcounts[], const MPI_Aint sdispls[],
                           const MPI_Datatype sendtypes[], void *recvbuf,
                           const int recvcounts[], const MPI_Aint rdispls[],
                           const MPI_Datatype recvtypes[], MPI_Comm comm)
{
    int i, err;
    int indegree, outdegree, weighted;
    size_t sendtype_size, recvtype_size;
    bool zerosend=true, zerorecv=true;

    MEMCHECKER(
        ptrdiff_t recv_ext;
        ptrdiff_t send_ext;

        memchecker_comm(comm);

        err = ompi_comm_neighbors_count(comm, &indegree, &outdegree, &weighted);
        if (MPI_SUCCESS == err) {
            for ( i = 0; i < outdegree; i++ ) {
                memchecker_datatype(sendtypes[i]);

                ompi_datatype_type_extent(sendtypes[i], &send_ext);

                memchecker_call(&opal_memchecker_base_isdefined,
                                (char *)(sendbuf)+sdispls[i]*send_ext,
                                sendcounts[i], sendtypes[i]);
            }
            for ( i = 0; i < indegree; i++ ) {
                memchecker_datatype(recvtypes[i]);
                ompi_datatype_type_extent(recvtypes[i], &recv_ext);
                memchecker_call(&opal_memchecker_base_isaddressable,
                                (char *)(recvbuf)+sdispls[i]*recv_ext,
                                recvcounts[i], recvtypes[i]);
            }
        }
    );

    if (MPI_PARAM_CHECK) {

        /* Unrooted operation -- same checks for all ranks */

        err = MPI_SUCCESS;
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (ompi_comm_invalid(comm) || !(OMPI_COMM_IS_CART(comm) || OMPI_COMM_IS_GRAPH(comm) ||
                                         OMPI_COMM_IS_DIST_GRAPH(comm))) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_COMM,
                                          FUNC_NAME);
        }

        if (MPI_IN_PLACE == sendbuf) {
            sendcounts = recvcounts;
            sdispls    = rdispls;
            sendtypes  = recvtypes;
        }

        if ((NULL == sendcounts) || (NULL == sdispls) || (NULL == sendtypes) ||
            (NULL == recvcounts) || (NULL == rdispls) || (NULL == recvtypes) ||
             MPI_IN_PLACE == recvbuf) {
            return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_ARG, FUNC_NAME);
        }

        err = ompi_comm_neighbors_count(comm, &indegree, &outdegree, &weighted);
        OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
        for (i = 0; i < outdegree; ++i) {
            OMPI_CHECK_DATATYPE_FOR_SEND(err, sendtypes[i], sendcounts[i]);
            OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
        }
        for (i = 0; i < indegree; ++i) {
            OMPI_CHECK_DATATYPE_FOR_RECV(err, recvtypes[i], recvcounts[i]);
            OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
        }
    }

    /* Do we need to do anything? */

    err = ompi_comm_neighbors_count(comm, &indegree, &outdegree, &weighted);
    OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
    for (i = 0; i < indegree; ++i) {
        ompi_datatype_type_size(recvtypes[i], &recvtype_size);
        if (0 != recvtype_size && 0 != recvcounts[i]) {
            zerorecv = false;
            break;
        }
    }
    if (MPI_IN_PLACE == sendbuf) {
        zerosend = zerorecv;
    } else {
        for (i = 0; i < outdegree; ++i) {
            ompi_datatype_type_size(sendtypes[i], &sendtype_size);
            if (0 != sendtype_size && 0 != sendcounts[i]) {
                zerosend = false;
                break;
            }
        }
    }
    if (zerosend && zerorecv) {
        return MPI_SUCCESS;
    }

    OPAL_CR_ENTER_LIBRARY();

    /* Invoke the coll component to perform the back-end operation */
    /* XXX -- CONST -- do not cast away const -- update mca/coll */
    err = comm->c_coll.coll_neighbor_alltoallw((void *) sendbuf, (int *) sendcounts, (MPI_Aint *) sdispls, (ompi_datatype_t **) sendtypes,
                                               recvbuf, (int *) recvcounts, (MPI_Aint *) rdispls, (ompi_datatype_t **) recvtypes,
                                               comm, comm->c_coll.coll_neighbor_alltoallw_module);
    OMPI_ERRHANDLER_RETURN(err, comm, err, FUNC_NAME);
}

