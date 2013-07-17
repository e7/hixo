COMPILE_FLAGS = -Wall -pipe -std=c99 -I $(top_srcdir)/include
COMPILE_FLAGS += `getconf LFS_CFLAGS`
COMPILE_FLAGS += -DPAGE_SIZE=`getconf PAGE_SIZE`

AM_CFLAGS = ${COMPILE_FLAGS}
AM_LDFLAGS = -lpthread

bin_PROGRAMS = hixo
hixo_SOURCES = conf.c hixo.c buffer.c resource.c socket.c \
               modules/main_core_module.c \
			   modules/event_core_module.c \
			   modules/epoll_module.c
