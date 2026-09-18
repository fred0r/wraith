#.PHONY: default check_gmake debug static dynamic clean distclean test

MAKE=env -u MAKELEVEL gmake

TARGETS=	\
		all \
		debug \
		libdefs \
		dynamic \
		clean \
		distclean \
		test \
		check

.for target in ${TARGETS}
${target}: check_gmake .PHONY .SILENT
	@if [ ! -f Makefile ]; then \
	  case "${.TARGET}" in \
	    clean|distclean) \
	      echo "[*] No Makefile/config here yet; removing any stray build artifacts."; \
	      rm -f Makefile src/Makefile lib/Makefile src/config.h src/buildinfo.h \
	            build/config.cache build/config.log build/config.status build/confdefs.h; \
	      rm -rf src/.defs src/.deps src/compat/.deps src/mod/*.mod/.deps src/crypto/.deps; \
	      find . \( -name '*.o' -o -name '*.So' \) -delete 2>/dev/null; \
	      exit 0 ;; \
	  esac; \
	  echo "[*] No Makefile; running ./configure"; \
	  ./configure --silent || exit 1; \
	fi; \
	${MAKE} ${.TARGET}
.endfor

check_gmake: .PHONY .SILENT
	@which gmake > /dev/null 2>&1 || { echo "Error: gmake not found. Install gmake first."; exit 1; }
