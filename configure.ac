#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([redis-perf-test], [0.1], [derbanfee@google.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_LANG_CPLUSPLUS
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX

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

# Checks for common libraries.

AC_CHECK_LIB(pthread,main,, AC_MSG_ERROR(missing pthread library))
AC_CHECK_LIB(hiredis,main)

# Checks for graphic output function

echo -n "searching for rrd.h in /usr/..."
result=`find /usr -name 'rrd.h' 2>/dev/null | head -1`
if test -n "$result" 
then
    rrd_header_path=${result%/rrd.h}
    echo $result
else
    rrd_header_path=
    echo "none"
fi
echo -n "searching for librrd_th.so in /usr/..."
rrd_lib=`find /usr -name 'librrd_th.so' 2>/dev/null | head -1`
if test -n "$rrd_lib" 
then
    echo $rrd_lib
else
    rrd_header_path=
    echo "none" 
fi 

AC_ARG_ENABLE(graph,AS_HELP_STRING(--enable-graph, [output graphic result, it requires rrdtool library]),
   [ 
      case "$enableval" in
      yes)  
           if test -z "$rrd_header_path"
           then 
              AC_MSG_ERROR(librrd_th.so is required by --enable-graph);
           fi ;;
      no) 
          rrd_header_path=
          rrd_lib= ;;
      *)  AC_MSG_ERROR(bad value $enableval for --enable-graph) ;;
     esac
   ])

if test -n "$rrd_header_path"
then
    AC_DEFINE(RP_WITH_GRAPH,1,switch on the graphic output)
    AC_SUBST(rrd_header_path)
    AC_SUBST(rrd_lib)
fi

AM_CONDITIONAL(RP_USE_GRAPH, [test -n "$rrd_header_path"])

# checks for rp3m program, default is enabled unless any required library is missing

AC_ARG_ENABLE(rp3m,AS_HELP_STRING(--enable-rp3m, [compile program rp3m, it requires redis3m, boost_thread, boost_system and boost_regex libraries]),
    [
    case "$enableval" in
    yes)
        AC_CHECK_LIB(redis3m,main,[redis_3m_libs='-lredis3m'],AC_MSG_ERROR(redis3m library is required by --enable-rp3m))
        AC_CHECK_LIB(boost_thread,main, [redis_3m_libs="$redis_3m_libs -lboost_thread"], AC_MSG_ERROR(boost_thread library is required by --enable-rp3m))
        AC_CHECK_LIB(boost_system,main, [redis_3m_libs="$redis_3m_libs -lboost_system"], AC_MSG_ERROR(boost_system library is required by --enable-rp3m))
        AC_CHECK_LIB(boost_regex,main, [redis_3m_libs="$redis_3m_libs -lboost_regex"], AC_MSG_ERROR(boost_regex library is required by --enable-rp3m))
        ;;
    no)
        redis_3m_libs=
        ;;
    *)
        AC_MSG_ERROR(bad value $enableval for --enable-rp3m) ;;
    esac
    ],  
    [   AC_CHECK_LIB(redis3m,main,[redis_3m_libs='-lredis3m'],[redis_3m_libs=])
        if test -n "$redis_3m_libs"; then
            AC_CHECK_LIB(boost_thread,main, [redis_3m_libs="$redis_3m_libs -lboost_thread"], [redis_3m_libs=])
        fi
        if test -n "$redis_3m_libs"; then
            AC_CHECK_LIB(boost_system,main, [redis_3m_libs="$redis_3m_libs -lboost_system"], [redis_3m_libs=])
        fi 
        if test -n "$redis_3m_libs"; then
            AC_CHECK_LIB(boost_regex,main, [redis_3m_libs="$redis_3m_libs -lboost_regex"], [redis_3m_libs=])
        fi
    ])

AC_SUBST(redis_3m_libs)
AM_CONDITIONAL(RP_ENABLE_RP3M, [test -n "$redis_3m_libs"])

# checks for rpcluster program, default is enabled

AC_CHECK_LIB(redis_cluster,main,[redis_cluster_libs='-lredis_cluster'],[redis_cluster_libs=])

AC_ARG_ENABLE(rpcluster,AS_HELP_STRING(--enable-rpcluster, [compile program rpcluster]),
    [
    case "$enableval" in
    yes)
        if test -z "$redis_cluster_libs"; then
        AC_MSG_ERROR(redis_cluster library is required by --enable-rpcluster)   
        fi ;;
    no)
        redis_cluster_libs=;;
    *) AC_MSG_ERROR(bad value $enableval for --enable-rpcluster) ;;
    esac
    ])
AC_SUBST(redis_cluster_libs)
AM_CONDITIONAL(RP_ENABLE_RPCLUSTER, [test -n "$redis_cluster_libs"]) 

AC_PREFIX_DEFAULT(bin)

AC_CONFIG_FILES([
Makefile
src/Makefile
])

AC_OUTPUT

