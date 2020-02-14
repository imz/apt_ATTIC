AC_DEFUN([rc_GLIBC_VER],
	[AC_MSG_CHECKING([glibc version])
	dummy=if$$
	cat <<_GLIBC_>$dummy.c
#include <features.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) { printf("%d",__GLIBC_MINOR__); exit(0); }
_GLIBC_
	${CC-cc} $dummy.c -o $dummy > /dev/null 2>&1
	if test "$?" = 0; then
		GLIBC_MINOR=`./$dummy`
		if test "$GLIBC_MINOR" -gt 9; then
			GLIBC_MINOR=9
		fi
		GLIBC_VER="libc6.$GLIBC_MINOR"
		AC_MSG_RESULT([$GLIBC_VER])
		dnl CNC:2003-03-25
		GLIBC_VER="$GLIBC_VER"
	else
		AC_MSG_WARN([cannot determine GNU C library minor version number])
	fi
	rm -f $dummy $dummy.c
	AC_SUBST(GLIBC_VER)
])

AC_DEFUN([rc_LIBSTDCPP_VER],
	[AC_MSG_CHECKING([libstdc++ version])
	dummy=if$$
	cat <<_LIBSTDCPP_>$dummy.cc
#include <iostream>
int main(void) { std::cout << std::endl; return 0; }
_LIBSTDCPP_
	${CXX-c++} $dummy.cc -o $dummy > /dev/null 2>&1

	if test "$?" = 0; then
                LIBSTDCPP_VER=`objdump -p ./$dummy | sed -ne 's/.*NEEDED.*libstdc++\(-libc.*\(-.*\)\)\?.so.\(.*\)/\3\2/p'`
	fi
	rm -f $dummy $dummy.cc

	if test -z "$LIBSTDCPP_VER"; then
		AC_MSG_WARN([cannot determine standard C++ library version number])
	else
		AC_MSG_RESULT([$LIBSTDCPP_VER])
		dnl CNC:2003-03-25
		LIBSTDCPP_VER="$LIBSTDCPP_VER"
	fi
	AC_SUBST(LIBSTDCPP_VER)
])

AC_DEFUN([rc_CXX11ABI],
	[AC_MSG_CHECKING([whether C++11 ABI is used])
	dummy=if$$
	cat <<_CXX11ABI_>$dummy.cc
#include <string>
_CXX11ABI_
	${CXX-c++} -E -dM $dummy.cc -o $dummy.ii
	if test "$?" = 0; then
		if ! grep -Fe _GLIBCXX_USE_CXX11_ABI $dummy.ii; then
			NO_CXX11ABI_SUFFIX=-nocxx11abi
			AC_MSG_RESULT([no])
		else
			NO_CXX11ABI_SUFFIX=
			AC_MSG_RESULT([yes])
		fi
	else
		NO_CXX11ABI_SUFFIX=-strangecxx
		AC_MSG_WARN([cannot use -E -dM with the compiler])
	fi
	rm -f $dummy.ii $dummy.cc
	AC_SUBST(NO_CXX11ABI_SUFFIX)
])
