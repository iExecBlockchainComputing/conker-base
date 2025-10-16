#!/bin/bash
set -o errexit
set -x
BASE_NAME=cvm-base
BASE_DIR="$( cd "$( dirname "$0"  )" && pwd )"
VERSION=$(git tag -l --points-at HEAD|awk -F'v' '{print $2}')

if [ -z "$VERSION" ] ; then
    VERSION=latest
fi

git_commit="$(git rev-parse --short=8 HEAD)"
build_time=$(date +%F-%H-%M-%S)
release_desc="${VERSION}-${git_commit}-${build_time}"

function build::image() {
    PROXY=$1
    echo "---> Build Image"
    # create docker-release next to conker-base clone
    rm -rf $BASE_DIR/../../docker-release
    mkdir -p $BASE_DIR/../../docker-release/tmp
    cp -a $BASE_DIR/Dockerfile $BASE_DIR/../../docker-release
    cp -a $BASE_DIR/supervisord/* $BASE_DIR/../../docker-release

    # move to docker-release
    cd $BASE_DIR/../../docker-release
    cp -a /usr/share/zoneinfo .
    cp -a $BASE_DIR/../* tmp

    docker build --no-cache --build-arg VERSION=$release_desc --build-arg https_proxy=${PROXY} -t $BASE_NAME:${VERSION} .
}

case $1 in
    buildimage)
        build::image $2
    ;;
esac
