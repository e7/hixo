COMPILE_FLAGS = -Wall -pipe -std=c99 -I $(top_srcdir)/include
COMPILE_FLAGS += `getconf LFS_CFLAGS`
COMPILE_FLAGS += -DPAGE_SIZE=`getconf PAGE_SIZE`
COMPILE_FLAGS += -DDEBUG_FLAG=1

AM_CFLAGS = ${COMPILE_FLAGS}
AM_LDFLAGS = -lpthread -rdynamic

bin_PROGRAMS = hixo
hixo_SOURCES = adv_string.c rbtree.c buffer.c resource.c \
			   conf.c socket.c hixo.c slub.c \
			   modules/main_core_module.c \
			   modules/event_core_module.c \
			   modules/epoll_module.c \
			   modules/echo.c \
			   modules/simple_http.c
