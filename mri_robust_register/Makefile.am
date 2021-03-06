##
## Makefile.am 
##

AM_CFLAGS=-I$(top_srcdir)/include
AM_CXXFLAGS=-I$(top_srcdir)/include $(VXL_CFLAGS)

if HAVE_MAC_OSX
OTHERLDFLAGS=-framework Accelerate
else
OTHERLDFLAGS=-z muldefs
endif

BUILT_SOURCES=mri_robust_template.help.xml.h \
	mri_robust_register.help.xml.h \
	mri_robust_template.help.xml.html \
	mri_robust_register.help.xml.html

foodir=$(prefix)/docs/xml
foo_DATA=mri_robust_template.help.xml mri_robust_register.help.xml
foo2dir=$(prefix)/docs/html
foo2_DATA=mri_robust_template.help.xml.html mri_robust_register.help.xml.html

bin_PROGRAMS = mri_robust_register \
	mri_robust_template \
	lta_diff \
	mri_create_tests \
	mri_gradient_info

mri_robust_register_SOURCES= JointHisto.cpp \
	CostFunctions.cpp MyMatrix.cpp MyMRI.cpp \
	Quaternion.cpp Registration.cpp RegRobust.cpp RegPowell.cpp \
	mri_robust_register.cpp mri_robust_register.help.xml.h
mri_robust_register_LDADD= $(addprefix $(top_builddir)/, $(LIBS_MGH)) \
  $(LIB_LAPACK) $(LIB_BLAS) $(LIB_G2C_A) $(LIB_GFORTRAN)
mri_robust_register_LDFLAGS=$(OS_LDFLAGS) $(OTHERLDFLAGS)

mri_robust_template_SOURCES= Registration.cpp  RegRobust.cpp \
	CostFunctions.cpp MyMatrix.cpp MyMRI.cpp Quaternion.cpp \
	MultiRegistration.cpp mri_robust_template.cpp mri_robust_template.help.xml.h
mri_robust_template_LDADD= $(addprefix $(top_builddir)/, $(LIBS_MGH)) \
  $(LIB_LAPACK) $(LIB_BLAS) $(LIB_G2C_A) $(LIB_GFORTRAN)
mri_robust_template_LDFLAGS=$(OS_LDFLAGS) $(OTHERLDFLAGS)

lta_diff_SOURCES=lta_diff.cpp Registration.cpp \
	CostFunctions.cpp MyMatrix.cpp MyMRI.cpp Quaternion.cpp
lta_diff_LDADD= $(addprefix $(top_builddir)/, $(LIBS_MGH)) \
  $(LIB_LAPACK) $(LIB_BLAS) $(LIB_G2C_A) $(LIB_GFORTRAN)
lta_diff_LDFLAGS=$(OS_LDFLAGS) $(OTHERLDFLAGS)

mri_create_tests_SOURCES=mri_create_tests.cpp Quaternion.cpp \
	MyMatrix.cpp MyMRI.cpp CostFunctions.cpp
mri_create_tests_LDADD= $(addprefix $(top_builddir)/, $(LIBS_MGH)) \
  $(LIB_LAPACK) $(LIB_BLAS) $(LIB_G2C_A) $(LIB_GFORTRAN)
mri_create_tests_LDFLAGS=$(OS_LDFLAGS) $(OTHERLDFLAGS)

mri_gradient_info_SOURCES=mri_gradient_info.cpp
mri_gradient_info_LDADD = $(addprefix $(top_builddir)/, $(LIBS_MGH))
mri_gradient_info_LDFLAGS=$(OS_LDFLAGS) $(OTHERLDFLAGS)

# trick to get test data into the build directory
foo_SOURCES=
foo:
	if ! test -f "$(top_builddir)/mri_robust_register/testdata.tar.gz"; then \
		cp -v $(top_srcdir)/mri_robust_register/testdata.tar.gz \
					$(top_builddir)/mri_robust_register/; fi;

check_PROGRAMS=foo

TESTS=test_libgfortran test_mri_robust_template $(top_builddir)/scripts/help_xml_validate

clean-local:
	rm -f $(BUILT_SOURCES) 

EXTRA_DIST=Regression.h Regression.cpp Registration.h RegistrationStep.h \
	MultiRegistration.h Quaternion.h RobustGaussian.cpp RegPowell.cpp \
	CostFunctions.h RobustGaussian.h RegPowell.h MyMatrix.h MyMRI.h \
	testdata.tar.gz $(foo_DATA) Registration.cpp test_mri_robust_template \
	$(BUILT_SOURCES) JointHisto.h RegRobust.h Transformation.h test_libgfortran

# mri_robust_register is called by Eugenios hippocampal subfield binaries. 
# Mac OSX systems 10.11 (El Capitan) and greater implemented SIP
# which necessitates a wrapper script to all binaries called by other binaries
# in order to pass the DYLD_LIBRARY_PATH env var.

install-exec-hook:
	cp $(top_builddir)/mri_robust_register/mri_robust_register $(DESTDIR)$(bindir)/mri_robust_register.bin
	echo "#!/bin/bash" > $(DESTDIR)$(bindir)/mri_robust_register
	echo "source \$$FREESURFER_HOME/sources.sh" >> $(DESTDIR)$(bindir)/mri_robust_register
	echo "mri_robust_register.bin \"\$$@\"" >> $(DESTDIR)$(bindir)/mri_robust_register


# Our release target. Include files to be excluded here. They will be
# found and removed after 'make install' is run during the 'make
# release' target.
EXCLUDE_FILES=
include $(top_srcdir)/Makefile.extra
