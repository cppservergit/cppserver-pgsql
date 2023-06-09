# cppserver-pgsql

CPPServer is a No-Code engine for creating JSON microservices based on EPOLL and Modern C++.
This Repo is the base C++ project for CPPServer w/PostgreSQL native API support.

![api-definition](https://github.com/cppservergit/cppserver-pgsql/assets/126841556/c366e193-44ce-489f-93d6-61fabdab9fc6)

To learn about CPPServer please check the [documentation Repo](https://github.com/cppservergit/cppserver-docs) for an overview, QuickStart and tutorials, this README contains information of the native/docker build process only, assumes familiarity with CPPServer general concepts.

This project compiles on Linux (AMD64/ARM64) only, depends on specific Linux APIs (epoll), depends on PostgreSQL native client library (libpq).

## Requirements - tested on Ubuntu 22.04 LTS
```
sudo apt update
sudo apt install g++-12 libpq-dev libcurl4-openssl-dev make -y
```

Note: You can use GCC-13 too if you have it installed, for Ubuntu 23.04 and greater you can use "apt install g++" instead of "g++-12". In any case you will have to edit Makefile to change the compiler name.

## Build

```
git clone https://github.com/cppservergit/cppserver-pgsql
cd cppserver-pgsql
make
```

Expected output:
```
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -c src/env.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -c src/logger.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -c src/config.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -c src/audit.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -c src/email.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -c src/httputils.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -I/usr/include/postgresql -c src/sql.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -I/usr/include/postgresql -c src/login.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -I/usr/include/postgresql -c src/session.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -I/usr/include/postgresql -DCPP_BUILD_DATE=20230706 -c src/mse.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init -I/usr/include/postgresql -DCPP_BUILD_DATE=20230706 -c src/main.cpp
g++-12 -Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init env.o logger.o config.o audit.o email.o httputils.o sql.o login.o session.o mse.o main.o -lpq -lcurl -o "cppserver"
cp cppserver image
cp config.json image
chmod 777 image/cppserver
```

An executable binary will be created in the current directory and it will be copied to the /image folder alongside with config.json (microservices definitions) for docker image building purposes.

## Clean build

Use the "clean" target to delete all object files and rebuild the whole project, if you change some specific file, like audit.cpp, just run "make" to rebuild only what's necessary.

```
make clean
make
```

## Docker

Note: this step requires docker pre-installed on your system.
The project contains a folder named "image" with the latest binary (after running make), config.json and a dockerfile of course, to build a docker image execute:
```
cd image
sudo docker build . -t cppserver/pgsql:1.2.0
```

You can build your own docker image if you changed the source code and recompiled the binary, but if this is not the case, we recommend you use our prebuilt official images on [dockerhub](https://hub.docker.com/repository/docker/cppserver/pgsql/general).
The config.json file is included by default into the docker image, but when running on Kubernetes you can deploy it as a ConfigMap and ignore the one included with the image, it's up to you.


## Project's folder structure

```
cppserver-pgsql
├── Makefile
├── README.md
├── config.json
├── image
│   ├── config.json
│   ├── cppserver
│   └── dockerfile
└── src
    ├── audit.cpp
    ├── audit.h
    ├── config.cpp
    ├── config.h
    ├── email.cpp
    ├── email.h
    ├── env.cpp
    ├── env.h
    ├── httputils.cpp
    ├── httputils.h
    ├── logger.cpp
    ├── logger.h
    ├── login.cpp
    ├── login.h
    ├── main.cpp
    ├── mse.cpp
    ├── mse.h
    ├── session.cpp
    ├── session.h
    ├── sql.cpp
    └── sql.h
```

## Makefile

As mentioned before, you may need to change the CC variable that points to the compiler, if your docker image is going to run on known hardware, you may want to add a "-mtune=XXX" option to the CC_OPTS variable in order to ask for hardware-specific optimizations, if you are not sure about the hardware, you may consider removing the "-march=native" flag from CC_OPTS variable. Whatever changes you do, please do not alter or remove the DATE variable. Shown below is the first section of the Makefile where the variables are defined.
```
SHELL=bash
DATE=$(shell printf '%(%Y%m%d)T')
CC=g++-12
CC_OPTS=-Wno-unused-parameter -Wpedantic -Wall -Wextra -O3 -std=c++23 -pthread -flto=6 -fno-extern-tls-init
CC_LIBS=-lpq -lcurl
CC_OBJS=env.o logger.o config.o audit.o email.o httputils.o sql.o login.o session.o mse.o main.o
```

## dockerfile

The CPPServer's docker image uses the official Ubuntu 22.04 LTS as the base image, a few environment variables are defined, but not all of the required variables, some were left out because those will are defined in the deployment artifact (YAML files) using secrets, mainly for database connection strings when using Kubernetes or a serverless Cloud service like Azure Container Apps, please visit CPPServer's [QuickStart tutorial](https://github.com/cppservergit/cppserver-docs/blob/main/quickstart.md) to learn all about CPPServer configuration options.

```
FROM ubuntu:latest
LABEL maintainer="cppserver@martincordova.com"
RUN apt update
RUN apt install -y --no-install-recommends libpq5 libcurl4
RUN apt clean
RUN mkdir /opt/cppserver
RUN mkdir /etc/cppserver
ADD config.json /etc/cppserver
ADD cppserver /opt/cppserver
ENV CPP_HTTP_LOG=0
ENV CPP_POOL_SIZE=4
ENV CPP_PORT=8080
ENV CPP_LOGIN_LOG=0
EXPOSE 8080
WORKDIR /opt/cppserver
ENTRYPOINT ["./cppserver"]
```

## Reliability

CPPServer has been tested with up to 20000 concurrent users bombarding requests on Ubuntu Server 22.04 both on VMs and baremetal, standalone and running inside Kubernetes (MicroK8s), processing millions of microservice requests. Such tests have also been executed using a binary compiled with instrumentation (-fsanitize, valgrind) to detect memory leaks and other memory-related vulnerabilities and none was found in the current release.

## Additional components

CPPServer has two more related projects, cppjob to execute scheduled tasks, and LoginServer to provide login service (via DBMS or LDAP) and security session creation, although CPPServer includes the capability for login and session, this is limited (no LDAP) and it's better manager in a separate component like LoginServer, it may be the case that these two projects are not yet released as open source repositories, if so they will be open source in short time, nevertheless their docker images are available on GitHub and fully documented:

* [cppserver/pgsql-job](https://hub.docker.com/r/cppserver/pgsql-job)
* [cppserver/pgsql-login](https://hub.docker.com/r/cppserver/pgsql-job)

In a production environment, CPPServer will be running as a container on Kubernetes or a Cloud service like Azure's Container App, and it will be deployed with these two components mentioned above:

![Pod deployment model](https://github.com/cppservergit/cppserver-pgsql/assets/126841556/45b023cd-fd1a-49c0-b82f-2429ac4ba6db)

Both projects were built on the codebase of CPPServer, LoginServer is almost the same but with additional LDAP dependencies and focused on Login and Security Session creation.

