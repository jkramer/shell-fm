AC_DEFUN([SHELL_FM_PROG_CC],[
AC_MSG_CHECKING([whether configure should try to set CFLAGS])
if test -z "${CFLAGS}"; then
    enable_cflags_setting=yes
else
    enable_cflags_setting=no
fi
AC_MSG_RESULT($enable_cflags_setting)
AC_PROG_CC
if test "x$enable_cflags_setting" = xyes; then
    if test $ac_cv_prog_cc_g = yes; then
	CFLAGS="-g -Os"
    else
	CFLAGS="-Os"
    fi
fi
])