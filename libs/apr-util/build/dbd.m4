dnl -------------------------------------------------------- -*- autoconf -*-
dnl Copyright 2005 The Apache Software Foundation or its licensors, as
dnl applicable.
dnl
dnl Licensed under the Apache License, Version 2.0 (the "License");
dnl you may not use this file except in compliance with the License.
dnl You may obtain a copy of the License at
dnl
dnl     http://www.apache.org/licenses/LICENSE-2.0
dnl
dnl Unless required by applicable law or agreed to in writing, software
dnl distributed under the License is distributed on an "AS IS" BASIS,
dnl WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
dnl See the License for the specific language governing permissions and
dnl limitations under the License.

dnl
dnl DBD module
dnl

dnl
dnl APU_CHECK_DBD: compile backends for apr_dbd.
dnl
AC_DEFUN([APU_CHECK_DBD], [
  apu_have_pgsql=0

  AC_ARG_WITH([pgsql], [
  --with-pgsql=DIR          specify PostgreSQL location
  ], [
    apu_have_pgsql=0
    if test "$withval" = "yes"; then
      AC_CHECK_HEADERS(libpq-fe.h, AC_CHECK_LIB(pq, PQsendQueryPrepared, [apu_have_pgsql=1]))
      if test "$apu_have_pgsql" = "0"; then
        AC_CHECK_HEADERS(postgresql/libpq-fe.h, AC_CHECK_LIB(pq, PQsendQueryPrepared, [apu_have_pgsql=1]))
      fi
    elif test "$withval" = "no"; then
      apu_have_pgsql=0
    else
      old_cppflags="$CPPFLAGS"
      old_ldflags="$LDFLAGS"

      pgsql_CPPFLAGS="-I$withval/include"
      pgsql_LDFLAGS="-L$withval/lib "

      APR_ADDTO(CPPFLAGS, [$pgsql_CPPFLAGS])
      APR_ADDTO(LDFLAGS, [$pgsql_LDFLAGS])

      AC_MSG_NOTICE(checking for pgsql in $withval)
      AC_CHECK_HEADERS(libpq-fe.h, AC_CHECK_LIB(pq, PQsendQueryPrepared, [apu_have_pgsql=1]))
      if test "$apu_have_pgsql" != "0"; then
        APR_ADDTO(APRUTIL_LDFLAGS, [-L$withval/lib])
        APR_ADDTO(APRUTIL_INCLUDES, [-I$withval/include])
      fi
      if test "$apu_have_pgsql" != "1"; then
        AC_CHECK_HEADERS(postgresql/libpq-fe.h, AC_CHECK_LIB(pq, PQsendQueryPrepared, [apu_have_pgsql=1]))
        if test "$apu_have_pgsql" != "0"; then
          APR_ADDTO(APRUTIL_INCLUDES, [-I$withval/include/postgresql])
          APR_ADDTO(APRUTIL_LDFLAGS, [-L$withval/lib])
        fi
      fi

      CPPFLAGS="$old_cppflags"
      LDFLAGS="$old_ldflags"
    fi
  ], [
    apu_have_pgsql=0
    AC_CHECK_HEADERS(libpq-fe.h, AC_CHECK_LIB(pq, PQsendQueryPrepared, [apu_have_pgsql=1]))
  ])
  AC_SUBST(apu_have_pgsql)
  dnl Since we have already done the AC_CHECK_LIB tests, if we have it, 
  dnl we know the library is there.
  if test "$apu_have_pgsql" = "1"; then
    APR_ADDTO(APRUTIL_EXPORT_LIBS,[-lpq])
    APR_ADDTO(APRUTIL_LIBS,[-lpq])
  fi
])
dnl
AC_DEFUN([APU_CHECK_DBD_MYSQL], [
  apu_have_mysql=0

  AC_CHECK_FILES([dbd/apr_dbd_mysql.c],[
    AC_ARG_WITH([mysql], [
    --with-mysql=DIR          **** SEE INSTALL.MySQL ****
    ], [
      apu_have_mysql=0
      if test "$withval" = "yes"; then
        old_cppflags="$CPPFLAGS"
        old_ldflags="$LDFLAGS"

        AC_PATH_PROG([MYSQL_CONFIG],[mysql_config])
        if test "x$MYSQL_CONFIG" != 'x'; then
          mysql_CPPFLAGS="`$MYSQL_CONFIG --include`"
          mysql_LDFLAGS="`$MYSQL_CONFIG --libs_r`"

          APR_ADDTO(CPPFLAGS, [$mysql_CPPFLAGS])
          APR_ADDTO(LDFLAGS, [$mysql_LDFLAGS])
        fi

        AC_CHECK_HEADERS(mysql.h, AC_CHECK_LIB(mysqlclient_r, mysql_init, [apu_have_mysql=1]))
        if test "$apu_have_mysql" = "0"; then
          AC_CHECK_HEADERS(mysql/mysql.h, AC_CHECK_LIB(mysqlclient_r, mysql_init, [apu_have_mysql=1]))
        else
          if test "x$MYSQL_CONFIG" != 'x'; then
            APR_ADDTO(APRUTIL_INCLUDES, [$mysql_CPPFLAGS])
            APR_ADDTO(APRUTIL_LDFLAGS, [$mysql_LDFLAGS])
          fi
        fi

        CPPFLAGS="$old_cppflags"
        LDFLAGS="$old_ldflags"
      elif test "$withval" = "no"; then
        apu_have_mysql=0
      else
        old_cppflags="$CPPFLAGS"
        old_ldflags="$LDFLAGS"

        AC_PATH_PROG([MYSQL_CONFIG],[mysql_config],,[$withval/bin])
        if test "x$MYSQL_CONFIG" != 'x'; then
          mysql_CPPFLAGS="`$MYSQL_CONFIG --include`"
          mysql_LDFLAGS="`$MYSQL_CONFIG --libs_r`"
        else
          mysql_CPPFLAGS="-I$withval/include"
          mysql_LDFLAGS="-L$withval/lib "
        fi

        APR_ADDTO(CPPFLAGS, [$mysql_CPPFLAGS])
        APR_ADDTO(LDFLAGS, [$mysql_LDFLAGS])

        AC_MSG_NOTICE(checking for mysql in $withval)
        AC_CHECK_HEADERS(mysql.h, AC_CHECK_LIB(mysqlclient_r, mysql_init, [apu_have_mysql=1]))
        if test "$apu_have_mysql" != "0"; then
          APR_ADDTO(APRUTIL_INCLUDES, [$mysql_CPPFLAGS])
          APR_ADDTO(APRUTIL_LDFLAGS, [$mysql_LDFLAGS])
        fi

        if test "$apu_have_mysql" != "1"; then
          AC_CHECK_HEADERS(mysql/mysql.h, AC_CHECK_LIB(mysqlclient_r, mysql_init, [apu_have_mysql=1]))
          if test "$apu_have_mysql" != "0"; then
            APR_ADDTO(APRUTIL_INCLUDES, [-I$withval/include/mysql])
            APR_ADDTO(APRUTIL_LDFLAGS, [-L$withval/lib])
          fi
        fi

        CPPFLAGS="$old_cppflags"
        LDFLAGS="$old_ldflags"
      fi
    ], [
      apu_have_mysql=0

      old_cppflags="$CPPFLAGS"
      old_ldflags="$LDFLAGS"

      AC_PATH_PROG([MYSQL_CONFIG],[mysql_config])
      if test "x$MYSQL_CONFIG" != 'x'; then
        mysql_CPPFLAGS="`$MYSQL_CONFIG --include`"
        mysql_LDFLAGS="`$MYSQL_CONFIG --libs_r`"

        APR_ADDTO(CPPFLAGS, [$mysql_CPPFLAGS])
        APR_ADDTO(LDFLAGS, [$mysql_LDFLAGS])
      fi

      AC_CHECK_HEADERS(mysql.h, AC_CHECK_LIB(mysqlclient_r, mysql_init, [apu_have_mysql=1]))

      if test "$apu_have_mysql" != "0"; then
        if test "x$MYSQL_CONFIG" != 'x'; then
          APR_ADDTO(APRUTIL_INCLUDES, [$mysql_CPPFLAGS])
          APR_ADDTO(APRUTIL_LDFLAGS, [$mysql_LDFLAGS])
        fi
      fi

      CPPFLAGS="$old_cppflags"
      LDFLAGS="$old_ldflags"
    ])
  ])

  AC_SUBST(apu_have_mysql)

  dnl Since we have already done the AC_CHECK_LIB tests, if we have it, 
  dnl we know the library is there.
  if test "$apu_have_mysql" = "1"; then
    APR_ADDTO(APRUTIL_EXPORT_LIBS,[-lmysqlclient_r])
    APR_ADDTO(APRUTIL_LIBS,[-lmysqlclient_r])
  fi
])
dnl
AC_DEFUN([APU_CHECK_DBD_SQLITE3], [
  apu_have_sqlite3=0

  AC_ARG_WITH([sqlite3], [
  --with-sqlite3=DIR         
  ], [
    apu_have_sqlite3=0
    if test "$withval" = "yes"; then
      AC_CHECK_HEADERS(sqlite3.h, AC_CHECK_LIB(sqlite3, sqlite3_open, [apu_have_sqlite3=1]))
    elif test "$withval" = "no"; then
      apu_have_sqlite3=0
    else
      old_cppflags="$CPPFLAGS"
      old_ldflags="$LDFLAGS"

      sqlite3_CPPFLAGS="-I$withval/include"
      sqlite3_LDFLAGS="-L$withval/lib "

      APR_ADDTO(CPPFLAGS, [$sqlite3_CPPFLAGS])
      APR_ADDTO(LDFLAGS, [$sqlite3_LDFLAGS])

      AC_MSG_NOTICE(checking for sqlite3 in $withval)
      AC_CHECK_HEADERS(sqlite3.h, AC_CHECK_LIB(sqlite3, sqlite3_open, [apu_have_sqlite3=1]))
      if test "$apu_have_sqlite3" != "0"; then
        APR_ADDTO(APRUTIL_LDFLAGS, [-L$withval/lib])
        APR_ADDTO(APRUTIL_INCLUDES, [-I$withval/include])
      fi

      CPPFLAGS="$old_cppflags"
      LDFLAGS="$old_ldflags"
    fi
  ], [
    apu_have_sqlite3=0
    AC_CHECK_HEADERS(sqlite3.h, AC_CHECK_LIB(sqlite3, sqlite3_open, [apu_have_sqlite3=1]))
  ])

  AC_SUBST(apu_have_sqlite3)

  dnl Since we have already done the AC_CHECK_LIB tests, if we have it, 
  dnl we know the library is there.
  if test "$apu_have_sqlite3" = "1"; then
    APR_ADDTO(APRUTIL_EXPORT_LIBS,[-lsqlite3])
    APR_ADDTO(APRUTIL_LIBS,[-lsqlite3])
  fi
])
dnl
AC_DEFUN([APU_CHECK_DBD_SQLITE2], [
  apu_have_sqlite2=0

  AC_ARG_WITH([sqlite2], [
  --with-sqlite2=DIR         
  ], [
    apu_have_sqlite2=0
    if test "$withval" = "yes"; then
      AC_CHECK_HEADERS(sqlite.h, AC_CHECK_LIB(sqlite, sqlite_open, [apu_have_sqlite2=1]))
    elif test "$withval" = "no"; then
      apu_have_sqlite2=0
    else
      old_cppflags="$CPPFLAGS"
      old_ldflags="$LDFLAGS"

      sqlite2_CPPFLAGS="-I$withval/include"
      sqlite2_LDFLAGS="-L$withval/lib "

      APR_ADDTO(CPPFLAGS, [$sqlite2_CPPFLAGS])
      APR_ADDTO(LDFLAGS, [$sqlite2_LDFLAGS])

      AC_MSG_NOTICE(checking for sqlite2 in $withval)
      AC_CHECK_HEADERS(sqlite.h, AC_CHECK_LIB(sqlite, sqlite_open, [apu_have_sqlite2=1]))
      if test "$apu_have_sqlite2" != "0"; then
        APR_ADDTO(APRUTIL_LDFLAGS, [-L$withval/lib])
        APR_ADDTO(APRUTIL_INCLUDES, [-I$withval/include])
      fi

      CPPFLAGS="$old_cppflags"
      LDFLAGS="$old_ldflags"
    fi
  ], [
    apu_have_sqlite2=0
    AC_CHECK_HEADERS(sqlite.h, AC_CHECK_LIB(sqlite, sqlite_open, [apu_have_sqlite2=1]))
  ])

  AC_SUBST(apu_have_sqlite2)

  dnl Since we have already done the AC_CHECK_LIB tests, if we have it, 
  dnl we know the library is there.
  if test "$apu_have_sqlite2" = "1"; then
    APR_ADDTO(APRUTIL_EXPORT_LIBS,[-lsqlite])
    APR_ADDTO(APRUTIL_LIBS,[-lsqlite])
  fi
])
dnl

