#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.50)
AC_INIT([shell-fm], [0.1.4], [bugs@example.com])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/settings.c])
AM_CONFIG_HEADER(config.h)

# These two substitutions are needed for some automake versions (eg. 1.4)
AC_SUBST(PACKAGE, [$PACKAGE_NAME])
AC_SUBST(VERSION, [$PACKAGE_VERSION])

# Checks for programs.
#AC_PROG_CXX
SHELL_FM_PROG_CC
AC_PROG_INSTALL


# Debugging
SHELL_FM_DEBUG


# Checks for libraries.

# FIXME: Replace `main' with a function in `-lcrypto':
AC_CHECK_LIB([crypto], [main],, [
	AC_MSG_ERROR([libcrypto required but not found])
])

# FIXME: Replace `main' with a function in `-lcurses':
AC_CHECK_LIB([curses], [main],, [
	AC_MSG_ERROR([libcurses required but not found])
])

# FIXME: Replace `main' with a function in `-lmad':
AC_CHECK_LIB([mad], [main],, [
	AC_MSG_ERROR([libmad required but not found])
])

AC_CHECK_LIB([readline], [using_history],, [
	AC_MSG_ERROR([libreadline required but not found])
])

SHELL_FM_HAVE_AO

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h termios.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_PID_T
AC_HEADER_TIME

# Checks for library functions.
AC_FUNC_FORK
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([atexit gethostbyname getpass memmove memset select socket strchr strdup strerror strncasecmp strndup strrchr strstr strtol])

AC_CONFIG_FILES([Makefile src/Makefile src/include/Makefile m4/Makefile])
AC_OUTPUT

SHELL_FM_SUMMARY
