dnl Check for libao
AC_DEFUN([SHELL_FM_HAVE_AO],[

shell_fm_disable_libao=no
AC_MSG_CHECKING([if requested not to build with libao])
AC_ARG_ENABLE(ao,
	[AC_HELP_STRING([--disable-ao], [do not use libao])],
	[case "$enableval" in
	no)
		shell_fm_disable_libao=yes;;
	*)
		shell_fm_disable_libao=no
	esac
	]
)
AC_MSG_RESULT([$shell_fm_disable_libao])

shell_fm_have_libao=no
if test x"$shell_fm_disable_libao" = xno; then
	AH_TEMPLATE([__HAVE_LIBAO__], [Define to 1 if you have libao])
	AC_CHECK_LIB([ao], [ao_initialize], [
		AC_CHECK_HEADER([ao/ao.h], [
			shell_fm_have_libao=yes
			AC_DEFINE(__HAVE_LIBAO__)
			LIBS="${LIBS}${LIBS+ }-lao"
		])
	])
fi

unset shell_fm_disable_libao
])
