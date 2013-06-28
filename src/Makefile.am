AM_CFLAGS = -Wall -std=c99 -I $(top_srcdir)/include
AM_LDFLAGS = -lpthread
bin_PROGRAMS = hixo
hixo_SOURCES = conf.c hixo.c \
               modules/main_core_module.c \
			   modules/event_core_module.c \
			   modules/epoll_module.c
