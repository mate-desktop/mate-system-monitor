@YELP_HELP_RULES@

HELP_ID = mate-system-monitor
HELP_FILES = index.docbook legal.xml
HELP_MEDIA = figures/addColumn.png \
	      figures/changePriority.png \
	      figures/colorPanel.png \
  	      figures/columnContextMenu.png \
	      figures/mate-system-monitor_window.png \
	      figures/memoryMaps.png \
	      figures/moreInfo.png \
	      figures/processListing.png \
	      figures/sortButton.png \
	      figures/sortMenu2.png \
	      figures/sortMenu.png \
	      figures/systemMonitor.png \
	      figures/viewMenu.png

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )
