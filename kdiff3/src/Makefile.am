
# set the include path for X, qt and KDE
INCLUDES = $(all_includes)

# these are the headers for your project
noinst_HEADERS = kdiff3_part.h kdiff3_shell.h kdiff3.h common.h diff.h \
	directorymergewindow.h merger.h optiondialog.h fileaccess.h version.h \
	smalldialogs.h difftextwindow.h mergeresultwindow.h

# let automoc handle all of the meta source files (moc)
METASOURCES = AUTO

messages: rc.cpp
	$(XGETTEXT) *.cpp -o $(podir)/kdiff3.pot

KDE_ICON = kdiff3

# this Makefile creates both a KPart application and a KPart
#########################################################################
# APPLICATION SECTION
#########################################################################
# this is the program that gets installed.  it's name is used for all
# of the other Makefile.am variables
bin_PROGRAMS = kdiff3

# the application source, library search path, and link libraries
kdiff3_SOURCES = main.cpp kdiff3_shell.cpp
kdiff3_LDFLAGS = $(KDE_RPATH) $(all_libraries)
kdiff3_LDADD   = $(LIB_KPARTS)

# this is where the desktop file will go
shelldesktopdir   = $(kde_appsdir)/Development
shelldesktop_DATA = kdiff3.desktop

# this is where the shell's XML-GUI resource file goes
shellrcdir   = $(kde_datadir)/kdiff3
shellrc_DATA = kdiff3_shell.rc

#########################################################################
# KPART SECTION
#########################################################################
kde_module_LTLIBRARIES = libkdiff3part.la

# the Part's source, library search path, and link libraries
libkdiff3part_la_SOURCES = kdiff3_part.cpp kdiff3.cpp directorymergewindow.cpp \
		merger.cpp pdiff.cpp difftextwindow.cpp diff.cpp optiondialog.cpp \
		mergeresultwindow.cpp fileaccess.cpp gnudiff_analyze.cpp gnudiff_io.cpp gnudiff_xmalloc.cpp \
		common.cpp smalldialogs.cpp
libkdiff3part_la_LDFLAGS = -module $(KDE_PLUGIN) $(all_libraries)
libkdiff3part_la_LIBADD = $(LIB_KDEPRINT) $(LIB_KPARTS) $(LIB_KFILE)

# this is where the desktop file will go
partdesktopdir   = $(kde_servicesdir)
partdesktop_DATA = kdiff3part.desktop

# this is where the part's XML-GUI resource file goes
partrcdir   = $(kde_datadir)/kdiff3part
partrc_DATA = kdiff3_part.rc
