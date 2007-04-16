INCLUDES    = $(all_includes)
METASOURCES = AUTO

# Install this plugin in the KDE modules directory
kde_module_LTLIBRARIES = libkdiff3plugin.la

libkdiff3plugin_la_SOURCES = kdiff3plugin.cpp
libkdiff3plugin_la_LIBADD = -lkonq
libkdiff3plugin_la_LDFLAGS = -module $(all_libraries) $(KDE_PLUGIN)

#KDE_ICON = KDiff3

pluginsdir   = $(kde_servicesdir)
plugins_DATA = kdiff3_plugin.desktop

appsdir = $(kde_appsdir)/.hidden
apps_DATA = kdiff3plugin.desktop

messages: rc.cpp
	$(XGETTEXT) *.cpp *.h -o $(podir)/kdiff3_plugin.pot
