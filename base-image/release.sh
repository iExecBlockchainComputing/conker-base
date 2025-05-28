#!/bin/bash
set -o errexit
set -x
BASE_NAME=cvm-base
BASEDIR="$( cd "$( dirname "$0"  )" && pwd  )"
VERSION=$(git tag -l --points-at HEAD|awk -F'v' '{print $2}')
buildTime=$(date +%F-%H)
git_commit=$(git log -n 1 --pretty --format=%h)

if [ -z "$VERSION" ];then
    VERSION=latest
fi
release_desc=${VERSION}-${git_commit}-${buildTime}


function build::image() {
    PROXY=$1
	echo "---> Build Image"
    cd $BASEDIR/../
	HOME=`pwd`
    rm -rf $BASEDIR/../../docker-release
    cp -a $BASEDIR/../base-image/k8s $BASEDIR/../../docker-release
    cd $BASEDIR/../../docker-release
	cp -r /usr/share/zoneinfo .

	mkdir tmp
    cp -a $BASEDIR/../* tmp
	docker build --no-cache  --build-arg VERSION=$release_desc --build-arg https_proxy=${PROXY} -t $BASE_NAME:${VERSION} -f Dockerfile.base .
	cd $HOME
}


case $1 in
	buildimage)
		build::image $2
	;;
esac
