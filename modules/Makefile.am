## Process this file with automake to produce Makefile.in

NULL = 

perl_MODULES =					\
	XML/Parser/Style/OrigTree.pm		\
	$(NULL)

EXTRA_DIST =					\
	$(perl_MODULES)				\
	$(NULL)

install:
	$(top_srcdir)/mkinstalldirs $(DESTDIR)$(datadir)/intltool/XML/Parser/Style
	$(INSTALL_DATA) $(top_srcdir)/modules/XML/Parser/Style/OrigTree.pm $(DESTDIR)$(datadir)/intltool/XML/Parser/Style
