#
# Copyright (c) 2015      Intel, Inc. All rights reserved
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

AM_CPPFLAGS = $(atomic_ofi_CPPFLAGS)

ofi_sources = \
	atomic_ofi.h \
	atomic_ofi_module.c \
	atomic_ofi_component.c \
	atomic_ofi_fadd.c \
	atomic_ofi_swap.c \
	atomic_ofi_atomicto.c \
	atomic_ofi_cswap.c


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_oshmem_atomic_ofi_DSO
component_noinst =
component_install = mca_atomic_ofi.la
else
component_noinst = libmca_atomic_ofi.la
component_install =
endif

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_atomic_ofi_la_SOURCES = $(ofi_sources)
mca_atomic_ofi_la_LIBADD = $(atomic_ofi_LIBS)
mca_atomic_ofi_la_LDFLAGS = -module -avoid-version $(atomic_ofi_LDFLAGS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_atomic_ofi_la_SOURCES =$(ofi_sources)
libmca_atomic_ofi_la_LDFLAGS = -module -avoid-version $(atomic_ofi_LDFLAGS)
