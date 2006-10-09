AC_DEFUN([SHELL_FM_HAVE_AO],[
shell_fm_have_libao=no
AH_TEMPLATE([__HAVE_LIBAO__], [Define to 1 if you have libao])
AC_CHECK_LIB([ao], [ao_initialize], [
	AC_CHECK_HEADER([ao/ao.h], [
		shell_fm_have_libao=yes
		AC_DEFINE(__HAVE_LIBAO__)
		LIBS="${LIBS}${LIBS+ }-lao"
	])
])
])
