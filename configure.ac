AC_INIT([tcmenu], [1.0], [thierry@example.com])
AC_CONFIG_SRCDIR([tcmenu.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs
AC_PROG_CC

# Ensure C89 support
m4_version_prereq([2.70], [], [AC_PROG_CC_C89])
if test "x$ac_cv_prog_cc_c89" = "xno" && test "x$ac_cv_prog_cc_stdc" = "xno"; then
    AC_MSG_ERROR([C89 support is required])
fi

# Checks for libraries
AC_CHECK_LIB([ncurses], [initscr], [],
    [AC_MSG_ERROR([ncurses library not found])])
AC_CHECK_LIB([menu], [new_menu], [],
    [AC_MSG_ERROR([menu library (part of ncurses) not found])],
    [-lncurses])

# Checks for header files
AC_CHECK_HEADERS([ncurses.h menu.h signal.h stdlib.h string.h], [],
    [AC_MSG_ERROR([required header file not found])])

# Checks for typedefs, structures, and compiler characteristics
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions
AC_CHECK_FUNCS([calloc strncpy strlen strcmp atoi], [],
    [AC_MSG_ERROR([required function not found])])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
