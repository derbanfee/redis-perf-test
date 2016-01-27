#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([redis-perf-test], [0.1], [derbanfee@google.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_LANG_CPLUSPLUS

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB(pthread,main,, AC_MSG_ERROR(missing pthread library))
AC_CHECK_LIB(redis3m,main,, AC_MSG_ERROR(missing redis3m library))
AC_CHECK_LIB(hiredis,main,, AC_MSG_ERROR(missing hiredis library))
AC_CHECK_LIB(boost_thread,main,, AC_MSG_ERROR(missing boost_thread library))
AC_CHECK_LIB(boost_system,main,, AC_MSG_ERROR(missing boost_system library))
AC_CHECK_LIB(boost_regex,main,, AC_MSG_ERROR(missing boost_regex library))
#AC_CHECK_LIB(redis3m,main,, AC_MSG_ERROR(missing redis3m library))

#AC_CHECK_LIB(boost_system,generic_category,, AC_MSG_ERROR(missing libboost-system library required by redis3m))
#AC_CHECK_LIB(boost_thread,,, AC_MSG_ERROR(missing libboost-thread library required by redis3m))

# Checks for header files.
AC_CHECK_HEADERS([stdint.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday setlocale strchr strerror])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_OUTPUT
