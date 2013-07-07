COMPILE_FALGS = -Wall -pipe -std=c99 -I $(top_srcdir)/include
COMPILE_FALGS += `getconf LFS_CFLAGS`

AM_CFLAGS = ${COMPILE_FALGS}
AM_LDFLAGS = -lpthread
bin_PROGRAMS = hixo
hixo_SOURCES = conf.c hixo.c \
               modules/main_core_module.c \
			   modules/event_core_module.c \
			   modules/epoll_module.c
