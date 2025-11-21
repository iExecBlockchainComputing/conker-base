FROM ubuntu:24.04 AS build

RUN apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    wget \
    pkg-config \
    unzip \
    build-essential

ARG VERSION=latest
RUN echo $VERSION > /VERSION

COPY . /cvm-agent

RUN wget --no-proxy --progress=bar:force -c https://dl.google.com/go/go1.24.9.linux-amd64.tar.gz -O - | tar -xz -C /usr/local
ENV PATH=$PATH:/usr/local/go/bin

RUN cd /cvm-agent/apploader \
    && go build -ldflags "-w -s -X apploader/version.version=${VERSION}" -o apploader

RUN cd /cvm-agent/cvmassistants/pkitool \
    && go build -ldflags "-w -s -X apploader/version.version=${VERSION}" -o pkitool

## build keyprovider
RUN apt-get install -y \
    git     \
    make    \
    cmake   \
    gcc     \
    cargo   \
    libssl-dev \
    software-properties-common \
    libcurl4-openssl-dev \
    libcbor-dev

# RA-TLS DCAP libraries
# https://download.01.org/intel-sgx/sgx_repo/ubuntu/dists/noble/main/binary-amd64/Packages
RUN echo 'deb [signed-by=/etc/apt/keyrings/intel-sgx-keyring.asc arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu noble main' | tee /etc/apt/sources.list.d/intel-sgx.list \
    && wget https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key -O /etc/apt/keyrings/intel-sgx-keyring.asc \
    && apt-get update \
    && apt-get install -y \
    libsgx-dcap-quote-verify-dev \
    libsgx-dcap-ql-dev \
    libsgx-uae-service \
    libtdx-attest-dev \
    libsgx-dcap-default-qpl-dev

RUN mkdir -p $HOME/.cargo/ && echo '[source.crates-io] \n registry = "git://mirrors.ustc.edu.cn/crates.io-index"' >> $HOME/.cargo/config

## todovm-cal 1005.1
RUN git clone https://github.com/inclavare-containers/rats-tls.git /rats-tls\
    && cd /rats-tls \
    && git checkout cf5e911a480f7120da480f046417a209e222e101 \
    && sed -i s@sgx_quote_3.h@sgx_quote_4.h@g src/tls_wrappers/api/tls_wrapper_verify_certificate_extension.c \
    && sed -i s@if\ 0@if\ 1@g src/tls_wrappers/api/tls_wrapper_verify_certificate_extension.c \
    && cmake -DRATS_TLS_BUILD_MODE="tdx" -DBUILD_SAMPLES=on -H. -Bbuild \
    && make -C build install \
    && cp -a /rats-tls/src/include/rats-tls/claim.h /usr/local/include/rats-tls/

RUN cd /cvm-agent/cvmassistants/keyprovider/key-provider-agent \
    && make all

RUN cd /cvm-agent/cvmassistants/secretprovider/secret-provider-agent \
    && make all

# Final image
FROM ubuntu:24.04

RUN apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    cryptsetup-bin \
    fdisk \
    wget \
    software-properties-common \
    vim \
    libcbor-dev

RUN mkdir -p /workplace/app \
    && mkdir -p /workplace/apploader/conf \
    && mkdir -p /workplace/cvm-agent/cvmassistants/pkitool/conf \
    && mkdir -p /workplace/cvm-agent/cvmassistants/disktool \
    && mkdir -p /workplace/cvm-agent/cvmassistants/network-tool \
    && mkdir -p /workplace/supervisord/apploader

ARG VERSION=latest
RUN echo $VERSION > /VERSION

COPY --from=build /cvm-agent/apploader/apploader /workplace/apploader
COPY --from=build /cvm-agent/apploader/conf  /workplace/apploader/conf/

#get network-tool
COPY --from=build /cvm-agent/cvmassistants/network-tool/network-config.sh /workplace/cvm-agent/cvmassistants/network-tool/

# get pkitool
COPY --from=build  /cvm-agent/cvmassistants/pkitool/pkitool  /workplace/cvm-agent/cvmassistants/pkitool
COPY --from=build /cvm-agent/cvmassistants/pkitool/conf /workplace/cvm-agent/cvmassistants/pkitool/conf

#get disktool
COPY --from=build  /cvm-agent/cvmassistants/disktool/ /workplace/cvm-agent/cvmassistants/disktool

#for support tdx attest
RUN echo "port=4050" > /etc/tdx-attest.conf

#get keyprovider
#for csv cert: /opt/csv/hsk_cek/<chip_id>/hsk_cek.cert
RUN  mkdir -p /workplace/cvm-agent/cvmassistants/keyprovider \
    && mkdir -p /usr/local/lib/rats-tls \
    && mkdir -p /opt/csv/hsk_cek/

# RA-TLS DCAP libraries
# https://download.01.org/intel-sgx/sgx_repo/ubuntu/dists/noble/main/binary-amd64/Packages
RUN echo 'deb [signed-by=/etc/apt/keyrings/intel-sgx-keyring.asc arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu noble main' | tee /etc/apt/sources.list.d/intel-sgx.list \
    && wget https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key -O /etc/apt/keyrings/intel-sgx-keyring.asc \
    && apt-get update \
    && apt-get install -y \
    libsgx-dcap-quote-verify \
    libsgx-dcap-ql \
    libsgx-uae-service \
    libtdx-attest \
    libsgx-dcap-default-qpl

COPY --from=build  /cvm-agent/cvmassistants/keyprovider/key-provider-agent/key_provider_agent /workplace/cvm-agent/cvmassistants/keyprovider
COPY --from=build  /usr/local/lib/rats-tls/  /usr/lib/
COPY --from=build  /usr/local/lib/rats-tls/  /usr/local/lib/rats-tls/

#get secretprovier-agent
RUN  mkdir -p /workplace/cvm-agent/cvmassistants/secretprovider
COPY --from=build  /cvm-agent/cvmassistants/secretprovider/secret-provider-agent/secret_provider_agent /workplace/cvm-agent/cvmassistants/secretprovider

#config supervisord
RUN apt-get update \
    && env DEBIAN_FRONTEND=noninteractive apt-get install -y \
    supervisor \
    curl

#todo make supervisord.conf configurable so that it can change the log path
COPY --from=build /cvm-agent/supervisord/supervisord.conf /etc/supervisor/
COPY --from=build /cvm-agent/apploader/conf/appload-supervisord.ini /workplace/supervisord/apploader

#install firewall
RUN mkdir -p /workplace/cvm-agent/cvmassistants/firewall \
    && mkdir -p /lib/modules \
    && apt install -y iptables ufw unzip

COPY --from=build /cvm-agent/cvmassistants/firewall  /workplace/cvm-agent/cvmassistants/firewall

WORKDIR /workplace/app
ENTRYPOINT ["supervisord", "-c", "/etc/supervisor/supervisord.conf"]
