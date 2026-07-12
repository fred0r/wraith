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
	@${MAKE} ${.TARGET}
.endfor

check_gmake: .PHONY .SILENT
	@which gmake > /dev/null 2>&1 || { echo "Error: gmake not found. Install gmake first."; exit 1; }
