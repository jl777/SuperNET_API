/* picocoin-config.h.  Generated from picocoin-config.h.in by configure.  */
/* picocoin-config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the `fdatasync' function. */
#define HAVE_FDATASYNC 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strndup' function. */
#define HAVE_STRNDUP 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* OSX naturally supports 64-bit files */
		#ifndef O_LARGEFILE
		# define O_LARGEFILE 0
		#endif

/* Name of package */
#define PACKAGE "picocoin"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "Jeff Garzik <jgarzik@exmulti.com>"

/* Define to the full name of this package. */
#define PACKAGE_NAME "picocoin"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "picocoin 0.1git"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "picocoin"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.1git"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.1git"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Enable LFS extensions on systems that have them.  */
#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE 1
#endif

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
