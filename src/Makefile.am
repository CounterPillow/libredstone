# ==================
# libredstone itself
# ==================

lib_LTLIBRARIES = libredstone.la
INCLUDES = -I$(top_builddir)

H_FILES =         \
    compression.h \
    rsendian.h    \
    error.h       \
    list.h        \
    memory.h      \
    mmap.h        \
    nbt.h         \
    region.h      \
    tag.h         \
    util.h        \
    redstone.h

C_FILES =         \
    compression.c \
    rsendian.c    \
    error.c       \
    list.c        \
    memory.c      \
    mmap-none.c   \
    mmap-windows.c \
    nbt.c         \
    region.c      \
    tag.c

libredstone_la_SOURCES = \
    $(C_FILES)           \
    $(H_FILES)

libredstone_la_LDFLAGS = -version-info $(LIBREDSTONE_LT_VERSION)

pkginclude_HEADERS = $(H_FILES)
