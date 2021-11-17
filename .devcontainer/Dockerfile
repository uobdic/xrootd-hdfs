FROM centos:7
LABEL maintainer="Luke Kreczko <kreczko@cern.ch>"

COPY xrootd-stable.repo /etc/yum.repos.d/xrootd-stable.repo
RUN yum update -y -q \
 && yum install -y epel-release centos-release-scl \
 && yum install --nogpg -y \
    cmake3 \
    devtoolset-8 \
    gcc-c++ \
    git \
    glibc-devel \
    java-1.8.0-openjdk-devel \
    java-1.8.0-openjdk-headless \
    openssl-devel \
    python-srpm-macros \
    rpm-build \
    wget \
    which \
    xrootd-devel \
    xrootd-server-devel \
    zlib \
 && rm -fr /var/cache/yum

# TODO: Download, extract and install HADOOP
ARG HADOOP_VERSION=hadoop-3.3.1
ADD tmp/${HADOOP_VERSION} /opt/hadoop

ENV HDFS_INCLUDE_DIR /opt/hadoop/include
ENV HDFS_LIB_DIR /opt/hadoop/lib/native
ENV JVM_LIB_DIR /usr/lib/jvm/jre

RUN groupadd --gid 1000 xrootd \
 && useradd --uid 1000 --gid xrootd --shell /bin/bash --create-home xrootd
USER xrootd

ENTRYPOINT scl enable devtoolset-8 bash
