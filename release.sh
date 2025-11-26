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

PROXY=$1
echo "---> Build Image"
docker build --no-cache --build-arg VERSION=$release_desc --build-arg https_proxy=${PROXY} -t $BASE_NAME:${VERSION} .
