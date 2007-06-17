AC_DEFUN([SHELL_FM_DEBUG],[
AH_TEMPLATE([NDEBUG], [Define to 1 in non-debug version])
shell_fm_enable_debug=no
AC_MSG_CHECKING([for debug version])
AC_ARG_ENABLE(debug,
	[AC_HELP_STRING([--enable-debug], [build debug version])],
	[case "$enableval" in
	yes|no)
		shell_fm_enable_debug="$enableval";;
	*)
		shell_fm_enable_debug=yes
	esac
	]
)
AC_MSG_RESULT([$shell_fm_enable_debug])
if test x"$shell_fm_enable_debug" = xyes; then
	CFLAGS="`echo "$CFLAGS" | sed 's/ *-O[[s1-9]]\>//'`"
else
	AC_DEFINE(NDEBUG)
fi
])
