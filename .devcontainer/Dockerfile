FROM centos:7
LABEL maintainer="Luke Kreczko <kreczko@cern.ch>"

RUN yum update -y -q \
 && yum install -y epel-release centos-release-scl \
 && yum install --nogpg -y gcc-c++ rpm-build which git python-srpm-macros devtoolset-8 cmake3 \
 && rm -fr /var/cache/yum

COPY xrootd-stable.repo /etc/yum.repos.d/xrootd-stable.repo
RUN yum update -y -q \
 && yum install -y --nogpg xrootd-devel xrootd-server-devel java-1.8.0-openjdk-headless java-1.8.0-openjdk-devel \
 && rm -fr /var/cache/yum

ARG HADOOP_VERSION=hadoop-3.3.1
ADD tmp/${HADOOP_VERSION} /opt/hadoop

RUN yum update -y -q \
 && yum install -y --nogpg openssl-devel zlib glibc-devel \
 && rm -fr /var/cache/yum

ENV HDFS_INCLUDE_DIR /opt/hadoop/include
ENV HDFS_LIB_DIR /opt/hadoop/lib/native
ENV JVM_LIB_DIR /usr/lib/jvm/jre

RUN groupadd --gid 1000 xrootd \
 && useradd --uid 1000 --gid xrootd --shell /bin/bash --create-home xrootd
USER xrootd

ENTRYPOINT scl enable devtoolset-8 bash
