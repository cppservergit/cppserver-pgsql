SHELL=bash
DATE=$(shell printf '%(%Y%m%d)T')
CC=g++-12
CC_OPTS=-Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init
CC_LIBS=-lpq -lcurl
CC_OBJS=env.o logger.o config.o audit.o email.o httputils.o sql.o login.o session.o mse.o main.o

cppserver: env.o logger.o config.o audit.o email.o httputils.o sql.o login.o session.o mse.o main.o
	$(CC) $(CC_OPTS) $(CC_OBJS) $(CC_LIBS) -o "cppserver"
	cp cppserver image
	cp config.json image
	chmod 777 image/cppserver

main.o: src/main.cpp mse.o
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -DCPP_BUILD_DATE=$(DATE) -c src/main.cpp

mse.o: src/mse.cpp src/mse.h
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -DCPP_BUILD_DATE=$(DATE) -c src/mse.cpp

session.o: src/session.cpp src/session.h
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -c src/session.cpp

login.o: src/login.cpp src/login.h
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -c src/login.cpp

sql.o: src/sql.cpp src/sql.h
	$(CC) $(CC_OPTS) -I/usr/include/postgresql -c src/sql.cpp

httputils.o: src/httputils.cpp src/httputils.h
	$(CC) $(CC_OPTS) -c src/httputils.cpp

email.o: src/email.cpp src/email.h
	$(CC) $(CC_OPTS) -c src/email.cpp

audit.o: src/audit.cpp src/audit.h
	$(CC) $(CC_OPTS) -c src/audit.cpp

config.o: src/config.cpp src/config.h
	$(CC) $(CC_OPTS) -c src/config.cpp

logger.o: src/logger.cpp src/logger.h
	$(CC) $(CC_OPTS) -c src/logger.cpp

env.o: src/env.cpp src/env.h
	$(CC) $(CC_OPTS) -c src/env.cpp

clean:
	rm env.o logger.o sql.o login.o session.o httputils.o mse.o email.o audit.o config.o main.o
