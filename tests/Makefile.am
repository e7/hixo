noinst_PROGRAMS = hixo_resource_test rbtree_test

hixo_resource_test_CFLAGS = -Wall -std=c99 -I $(top_srcdir)/include
hixo_resource_test_LDADD = $(top_srcdir)/src/resource.o
hixo_resource_test_LDFLAGS = -lpthread
hixo_resource_test_SOURCES = hixo_resource_test.c

rbtree_test_CFLAGS = -Wall -std=c99 -I $(top_srcdir)/include
rbtree_test_LDADD = $(top_srcdir)/src/rbtree.o
rbtree_test_LDFLAGS = -lpthread
rbtree_test_SOURCES = rbtree_test.c
