/*
 * Copyright (c) 2015	   Intel, Inc. All rights reserved 
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

# MCA_oshmem_mtl_ofi_CONFIG([action-if-can-compile],
#                    [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_oshmem_spml_ofshm_CONFIG],[
    AC_CONFIG_FILES([oshmem/mca/spml/ofshm/Makefile])

  # ensure we already ran the common libfabric config
    AC_REQUIRE([MCA_opal_common_libfabric_CONFIG])

    AS_IF([test "$opal_common_libfabric_happy" = "yes"],
          [$1],
          [$2])

    # substitute in the things needed to build ofi
    AC_SUBST([spml_ofshm_CFLAGS])
    AC_SUBST([spml_ofshm_CPPFLAGS])
    AC_SUBST([spml_ofshm_LDFLAGS])
    AC_SUBST([spml_ofshm_LIBS])
])dnl
\n
