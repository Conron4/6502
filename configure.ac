AC_INIT([6502], [0.2], [https://github.com/Conron4/6502])
AC_CONFIG_SRCDIR([source/main_6502.cxx])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_PROG_CXX

AC_PATH_PROG([SDL2_CONFIG], [sdl2-config], [no])
if test "x$SDL2_CONFIG" = "xno"; then
    AC_MSG_ERROR([sdl2-config not found; install the SDL2 development package])
fi

SDL2_CFLAGS=`$SDL2_CONFIG --cflags`
SDL2_LIBS=`$SDL2_CONFIG --libs`
AC_SUBST([SDL2_CFLAGS])
AC_SUBST([SDL2_LIBS])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT