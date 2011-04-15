CLEANFILES = moc_*.cpp *~
DISTCLEANFILES =
MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/Makefile.buildvars

pkglib_LTLIBRARIES = oscilloscope.la

oscilloscope_la_LDFLAGS = -module -avoid-version

oscilloscope_la_SOURCES = \
		oscilloscope.h \
		panel.h \
		properties.h \
		scope.h \
		oscilloscope.cpp \
		panel.cpp \
		properties.cpp \
		scope.cpp
nodist_oscilloscope_la_SOURCES = \
		moc_oscilloscope.cpp \
		moc_panel.cpp \
		moc_properties.cpp \
		moc_scope.cpp

# MOC Rule - builds meta-object files as needed
moc_%.cpp: %.h
	$(MOC) -o $@ $<
