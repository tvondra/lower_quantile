MODULE_big = lower_quantile
OBJS = lower_quantile.o

EXTENSION = lower_quantile
DATA = sql/lower_quantile--1.0.0.sql
MODULES = lower_quantile

CFLAGS=`pg_config --includedir-server`

TESTS        = $(wildcard test/sql/*.sql)
REGRESS      = $(patsubst test/sql/%.sql,%,$(TESTS))
REGRESS_OPTS = --inputdir=test

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
