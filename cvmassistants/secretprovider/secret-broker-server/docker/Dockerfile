FROM ubuntu:20.04 AS build

RUN apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y \
       wget \
       pkg-config \
       unzip \
       git     \
       make    \
       cmake   \
       gcc     \
       cargo   \
       libssl-dev \
       software-properties-common \
       libcurl4-openssl-dev \
       build-essential \
       libjansson-dev
ARG VERSION=latest
RUN echo $VERSION > /VERSION

COPY ./ /root

#  RA-TLS DCAP libraries:
RUN echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | tee /etc/apt/sources.list.d/intel-sgx.list > /dev/null \
    && wget -O - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | apt-key add -\
    && apt-get update \
    && apt-get install -y \
       libsgx-dcap-quote-verify-dev \
       libsgx-dcap-ql-dev \
       libsgx-uae-service

RUN mkdir -p $HOME/.cargo/ && echo '[source.crates-io] \n registry = "git://mirrors.ustc.edu.cn/crates.io-index"' >> $HOME/.cargo/config

RUN git clone https://github.com/inclavare-containers/rats-tls.git /rats-tls\
    && cd /rats-tls \
    && git checkout 670933a7b0ade22de1c0b73f8551e2fdefa98f1e \
    && sed -i s@add_subdirectory\(sev\)@#add_subdirectory\(sev\)@g src/attesters/CMakeLists.txt \
    && cmake -H. -Bbuild \
    && make -C build install \
    && cp -a /rats-tls/src/include/rats-tls/claim.h /usr/local/include/rats-tls/

RUN cd /root \
    && make all


FROM ubuntu:20.04

RUN apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y \
       libssl-dev \
       libcurl4-openssl-dev \
       software-properties-common \
       wget

#  RA-TLS DCAP libraries
RUN echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | tee /etc/apt/sources.list.d/intel-sgx.list > /dev/null \
    && wget -O - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | apt-key add -\
    && apt-get update \
    && apt-get install -y \
       libsgx-dcap-quote-verify \
       libsgx-dcap-ql \
       libsgx-uae-service

RUN mkdir -p /workplace/app \
    && mkdir -p /usr/local/lib/rats-tls

ARG VERSION=latest
RUN echo $VERSION > /VERSION

COPY --from=build  /root/secret_broker_server /workplace/app
COPY --from=build  /usr/local/lib/rats-tls  /usr/local/lib/rats-tls
ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib/rats-tls

WORKDIR /workplace/app
ENTRYPOINT ["/workplace/app/secret_broker_server"]
CMD []
