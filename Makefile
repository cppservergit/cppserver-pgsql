SHELL=bash
DATE=$(shell printf '%(%Y%m%d)T')
CC=g++-12
CC_OPTS=-O3 -std=c++20 -pthread -flto=4 -fno-extern-tls-init -march=native
CC_LIBS=-lpq
CC_OBJS=env.o loki.o logger.o config.o audit.o httputils.o sql.o login.o session.o mse.o main.o

cppserver: env.o loki.o logger.o config.o audit.o httputils.o sql.o login.o session.o mse.o main.o
	$(CC) $(CC_OPTS) $(CC_OBJS) $(CC_LIBS) -o "cppserver"
	cp cppserver image
	cp config.json image

main.o: src/main.cpp mse.o
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -DCPP_BUILD_DATE=$(DATE) -c src/main.cpp

mse.o: src/mse.cpp src/mse.h logger.o httputils.o sql.o login.o session.o config.o audit.o 
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -DCPP_BUILD_DATE=$(DATE) -c src/mse.cpp

session.o: src/session.cpp src/session.h logger.o
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -c src/session.cpp

login.o: src/login.cpp src/login.h logger.o
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -c src/login.cpp

sql.o: src/sql.cpp src/sql.h logger.o
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -c src/sql.cpp

httputils.o: src/httputils.cpp src/httputils.h logger.o
	$(CC) $(CC_OPTS) -c src/httputils.cpp

audit.o: src/audit.cpp src/audit.h config.o
	$(CC) $(CC_OPTS) -c src/audit.cpp

config.o: src/config.cpp src/config.h logger.o
	$(CC) $(CC_OPTS) -c src/config.cpp

logger.o: src/logger.cpp src/logger.h loki.o
	$(CC) $(CC_OPTS) -c src/logger.cpp

loki.o: src/loki.cpp src/loki.h env.o
	$(CC) $(CC_OPTS) -c src/loki.cpp

env.o: src/env.cpp src/env.h
	$(CC) $(CC_OPTS) -c src/env.cpp

clean:
	rm env.o logger.o loki.o sql.o login.o session.o httputils.o mse.o audit.o config.o main.o
