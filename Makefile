CC=gcc
CXX=g++

PLATFORM = $(shell file /bin/ls | cut -d' ' -f3 | cut -d'-' -f1)
BSD_VERSION = $(shell uname -v 2>&1 | cut -d' ' -f2 | cut -d'.' -f1)
SVR_VERSION = $(shell cat __REVISION__)

default: libthecore libpoly libgame liblua libsql game db
	@echo "--------------------------------------"
	@echo "Build Done"
	@echo "--------------------------------------"

liblua: .
	$(MAKE) -C $@ clean
	$(MAKE) -C $@

libsql: .
	@touch $@/Depend
	$(MAKE) -C $@ dep
	$(MAKE) -C $@ clean
	$(MAKE) -C $@

libgame: .
	@touch $@/src/Depend
	$(MAKE) -C $@/src dep
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src

libpoly: .
	@touch $@/Depend
	$(MAKE) -C $@ dep
	$(MAKE) -C $@ clean
	$(MAKE) -C $@ 

libthecore: .
	@touch $@/src/Depend
	$(MAKE) -C $@/src dep
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src

game: .
	@touch $@/src/Depend
	$(MAKE) -C $@/src dep
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src

db: .
	@touch $@/src/Depend
	$(MAKE) -C $@/src dep
	$(MAKE) -C $@/src clean
	$(MAKE) -C $@/src
	
install:
	$(MAKE) -C game/src install
	$(MAKE) -C db/src install

all: 
	@echo "--------------------------------------"
	@echo "Full Build Start"
	@echo "--------------------------------------"

	# $(MAKE) -C liblua clean
	$(MAKE) -C liblua

	@touch libsql/Depend
	$(MAKE) -C libsql dep
	# $(MAKE) -C libsql clean
	$(MAKE) -C libsql

	@touch libgame/src/Depend
	$(MAKE) -C libgame/src dep
	# $(MAKE) -C libgame/src clean
	$(MAKE) -C libgame/src

	@touch libpoly/Depend
	$(MAKE) -C libpoly dep
	# $(MAKE) -C libpoly clean
	$(MAKE) -C libpoly 

	@touch libthecore/src/Depend
	$(MAKE) -C libthecore/src dep
	# $(MAKE) -C libthecore/src clean
	$(MAKE) -C libthecore/src

	@touch game/src/Depend
	$(MAKE) -C game/src dep
	# $(MAKE) -C game/src clean
	$(MAKE) -C game/src

	@touch db/src/Depend
	$(MAKE) -C db/src dep
	# $(MAKE) -C db/src clean
	$(MAKE) -C db/src

	@echo "--------------------------------------"
	@echo "Full Build End"
	@echo "--------------------------------------"
	
all2: 
	@echo "--------------------------------------"
	@echo "Full Build Start"
	@echo "--------------------------------------"

	@touch game/src/Depend
	$(MAKE) -C game/src dep
	# $(MAKE) -C game/src clean
	$(MAKE) -C game/src

	@touch db/src/Depend
	$(MAKE) -C db/src dep
	# $(MAKE) -C db/src clean
	$(MAKE) -C db/src

	@echo "--------------------------------------"
	@echo "Full Build End"
	@echo "--------------------------------------"

clean:
	$(MAKE) -C liblua clean
	$(MAKE) -C libsql clean
	$(MAKE) -C libgame/src clean
	$(MAKE) -C libpoly clean
	$(MAKE) -C libthecore/src clean
	$(MAKE) -C game/src clean
	$(MAKE) -C db/src clean
	
clean2:
	$(MAKE) -C game/src clean
	$(MAKE) -C db/src clean
