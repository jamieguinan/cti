# This is in a separate file so as not to have to rebuild the whole
# tree when only changing this small feature.
# Uncomment the CPPFLAGS lines for debugging segfaults, memory leaks and
# other issues.
CPPFLAGS += -DSTACK_DEBUG_INSTRUMENT_FUNCTIONS
CPPFLAGS += -finstrument-functions
