##
## Makefile.am 
##

AM_CFLAGS=-I$(top_srcdir)/include
AM_CXXFLAGS=-I$(top_srcdir)/include

bin_PROGRAMS = mris_register_label_map
mris_register_label_map_SOURCES=mris_register_label_map.c
mris_register_label_map_LDADD= $(addprefix $(top_builddir)/, $(LIBS_MGH))
mris_register_label_map_LDFLAGS=$(OS_LDFLAGS)

# Our release target. Include files to be excluded here. They will be
# found and removed after 'make install' is run during the 'make
# release' target.
EXCLUDE_FILES=
include $(top_srcdir)/Makefile.extra
