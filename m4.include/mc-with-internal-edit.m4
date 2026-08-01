dnl
dnl Internal editor support.
dnl
AC_DEFUN([mc_WITH_INTERNAL_EDIT], [

    AC_ARG_WITH([internal_edit],
        AS_HELP_STRING([--with-internal-edit], [Enable internal editor @<:@yes@:>@]))

    if test x$with_internal_edit != xno; then
            AC_DEFINE(USE_INTERNAL_EDIT, 1, [Define to enable internal editor])
            use_internal_edit=yes
            AC_MSG_NOTICE([using internal editor])
            edit_msg="yes"
    else
            use_internal_edit=no
            edit_msg="no"
    fi

    if test x$with_internal_edit != xno; then
            if test x"$g_module_supported" != x; then
                AC_DEFINE(HAVE_ASPELL, 1, [Define to build spell checking into the editor])
                edit_msg="yes with spell checking"
            else
                AC_MSG_NOTICE([no gmodule: the editor is built without spell checking])
            fi
    fi
])
