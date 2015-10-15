/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved 
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

# MCA_oshmem_atomic_ofi_CONFIG([action-if-can-compile],
#                    [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_oshmem_atomic_ofi_CONFIG],[
    AC_CONFIG_FILES([oshmem/mca/atomic/ofi/Makefile])
    OPAL_CHECK_LIBFABRIC([atomic_ofi],
                   [save_CPPFLAGS="$CPPFLAGS"
                   save_LDFLAGS="$LDFLAGS"
                   save_LIBS="$LIBS"

                   CPPFLAGS="$CPPFLAGS -I$ompi_check_ofi_dir/include"
                   LDFLAGS="$LDFLAGS -L$ompi_check_ofi_dir/lib"
                   LIBS="$LIBS -lfabric"
                   AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
                                int main() {
                                /* if compiler sees these constansts then ofi has atomic support*/
                                return 0;
                                }]])],
                    [AC_DEFINE([OSHMEM_HAS_ATOMIC_MXM], [1], [ofi support is available]) atomic_ofi_happy="yes"],
                   [atomic_ofi_happy="no"],
                   [atomic_ofi_happy="no"])
                   CPPFLAGS=$save_CPPFLAGS
                   LDFLAGS=$save_LDFLAGS
                   LIBS=$save_LIBS
                   ],
                   [atomic_ofi_happy="no"])

    AS_IF([test "$atomic_ofi_happy" = "yes"],
          [atomic_ofi_WRAPPER_EXTRA_LDFLAGS="$atomic_ofi_LDFLAGS"
           atomic_ofi_WRAPPER_EXTRA_LIBS="$atomic_ofi_LIBS"
           $1],
          [$2])


    # substitute in the things needed to build ofi
    AC_SUBST([atomic_ofi_CFLAGS])
    AC_SUBST([atomic_ofi_CPPFLAGS])
    AC_SUBST([atomic_ofi_LDFLAGS])
    AC_SUBST([atomic_ofi_LIBS])

    AC_MSG_CHECKING([if oshmem/atomic/ofi component can be compiled])
    AC_MSG_RESULT([$atomic_ofi_happy])
])dnl
