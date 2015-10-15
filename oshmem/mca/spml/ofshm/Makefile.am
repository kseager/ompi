#
# Copyright (c) 2015	   Intel, Inc. All rights reserved 
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dist_ompidata_DATA = \
	help-oshmem-spml-ofshm.txt

AM_CPPFLAGS = $(spml_ofshm_CPPFLAGS)

ofshm_sources  = \
	spml_ofshm.c \
	spml_ofshm.h \
	spml_ofshm_component.c \
	spml_ofshm_component.h

if MCA_BUILD_oshmem_spml_ofshm_DSO
component_noinst =
component_install = mca_spml_ofshm.la
else
component_noinst = libmca_spml_ofshm.la
component_install =
endif

mcacomponentdir = $(ompilibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_spml_ofshm_la_SOURCES = $(ofshm_sources)
mca_spml_ofshm_la_LIBADD = $(spml_ofshm_LIBS)
mca_spml_ofshm_la_LDFLAGS = -module -avoid-version $(spml_ofshm_LDFLAGS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_spml_ofshm_la_SOURCES = $(ofshm_sources)
libmca_spml_ofshm_la_LIBADD = $(spml_ofshm_LIBS)
libmca_spml_ofshm_la_LDFLAGS = -module -avoid-version $(spml_ofshm_LDFLAGS)
