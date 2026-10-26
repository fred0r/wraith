AC_DEFUN([CHECK_LIBELF],
[
  AC_ARG_WITH([libelf],
    AS_HELP_STRING([--with-libelf=auto|system|bundled],
      [select system or bundled libelf (default: auto)]),
    [with_libelf="$withval"],
    [with_libelf=auto])

  case "$with_libelf" in
    yes|auto) with_libelf=auto ;;
    system|bundled) ;;
    no) with_libelf=bundled ;;
    *) AC_MSG_ERROR([invalid --with-libelf value '$with_libelf'; use auto, system, or bundled]) ;;
  esac

  LIBELF_DIR="lib/libelf"
  LIBELF_BUNDLED="\$(top_builddir)/${LIBELF_DIR}/libelf.a"
  LIBELF_LIB="${LIBELF_BUNDLED} -lz"
  LIBELF_INCLUDE="-I\$(top_srcdir)/${LIBELF_DIR}/include"

  if test "x$with_libelf" != "xbundled"; then
    AC_CHECK_HEADERS([gelf.h libelf.h])
    if test "x$ac_cv_header_gelf_h" = "xyes" && test "x$ac_cv_header_libelf_h" = "xyes"; then
      AC_CHECK_LIB(elf, gelf_getehdr, [
        AC_DEFINE(HAVE_LIBELF, 1, [Define to 1 if you have the 'elf' library (-lelf).])
        LIBELF_LIB="-Wl,-Bstatic -lelf -Wl,-Bdynamic"
        AC_CHECK_LIB([z], [inflate], [LIBELF_LIB="$LIBELF_LIB -lz"])
        AC_CHECK_LIB([zstd], [ZSTD_createCCtx], [LIBELF_LIB="$LIBELF_LIB -lzstd"])
        LIBELF_BUNDLED=
        LIBELF_INCLUDE=
      ]) dnl AC_CHECK_LIB
    fi
  fi

  if test "x$with_libelf" = "xsystem" && test "x$LIBELF_BUNDLED" != "x"; then
    AC_MSG_ERROR([system libelf requested but gelf.h/libelf.h or -lelf was not found])
  fi

  if test "x$LIBELF_BUNDLED" != "x"; then
    AC_CHECK_HEADERS([zlib.h], [],
      [AC_MSG_ERROR([zlib headers are required for bundled libelf])])
  fi

  CPPFLAGS="${CPPFLAGS} ${LIBELF_INCLUDE}"

  AC_SUBST([LIBELF_BUNDLED])
  AC_SUBST([LIBELF_LIB])
])
