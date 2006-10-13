AC_DEFUN([SHELL_FM_PROG_CC],[
AC_PROG_CC
# For gcc, prefer -Os over -O2
if test x"$GCC" = xyes; then
	CFLAGS="`echo "$CFLAGS" | sed 's/ -O2\>/ -Os/'`"
	CFLAGS="$CFLAGS${CFLAGS+ }-Wall"
fi
])
