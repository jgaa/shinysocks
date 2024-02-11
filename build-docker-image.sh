#!/bin/bash

## General build script for a CMake project to
## build a container-image from the deliverables

project=shinysocks
image_repro=jgaafromnorth

docker_run_args=""
tag=latest
strip=false
debug=false
push=false
clean=false
logbt=false
build_deb=false
run_tests=ON
cmake_build_type=RelWithDebInfo
image_tag=$project
build_image="${project}bld:latest"
scriptname=`basename "$0"`
version=v`grep " set(NEXTAPP_VERSION" CMakeLists.txt | xargs | cut -f 2 -d ' ' | cut -f1 -d')'`

if [ -z ${BUILD_DIR+x} ]; then
    BUILD_DIR="${HOME}/${project}-build-image"
else
    BUILD_DIR="${BUILD_DIR}/${project}-docker-build"
fi

usage() {
  echo "Usage: ${scriptname} [options]"
  echo "Builds ${project} to a container image"
  echo "Options:"
  echo "  --debug       Compile with debugging enabled"
  echo "  --strip       Strip the binary (makes backtraces less useful)"
  echo "  --clean       Perform a full, new build."
  echo "  --logbt       Use logbt to get a stack-trace if the app segfaults"
  echo "                /proc/sys/kernel/core_pattern must be '/tmp/logbt-coredumps/core.%p.%E'"
  echo "  --push        Push the image to a docker registry"
  echo "  --tag tagname Tag to '--push' to. Defaults to 'latest'"
  echo "  --version ver Version to tag. Defaults to '${version}'"
  echo "  --scripted    Assume that the command is run from a script"
  echo "  --help        Show help and exit."
  echo "  --skip-tests  Skip running the unit-tests as part of the build"
  echo
  echo "Environment variables"
  echo "  BUILD_DIR     Directory to build with CMake. Default: '${BUILD_DIR}'"
  echo "  TARGET        Target image. Defaults to ${project}:<tagname>"
  echo "  REGISTRY      Registry to '--push' to. Defaults to '${image_repro}'"
  echo "  SOURCE_DIR    Directory to the source code. Defaults to the current dir"
  echo
}

# Make a build-container that contains the compiler and dev-libraries
# for the build. We don't want all that in the destination container.
build_bldimage() {
    pushd docker
    echo Buiding build-image
    docker build -f Dockerfile.build -t ${build_image} . || die
    popd
}

die() {
    echo "$*" 1>&2
    exit 1;
}

while [ $# -gt 0 ];  do
    case "$1" in
        --debug)
            shift
            cmake_build_type=Debug
            ;;

        --strip)
            shift
            strip=true
            ;;

        --push)
            shift
            push=true
            ;;

        --skip-tests)
            shift
            run_tests=OFF
            ;;
            
        --clean)
            shift
            clean=true
            ;;
            
        --logbt)
            shift
            logbt=true
            ;;

        --tag)
            shift
            tag=$1
            shift
            ;;

        --version)
            shift
            version=$1
            shift
            ;;

        --scripted)
            shift
            docker_run_args="-t"
            ;;

        --help)
            usage
            exit 0
            ;;

        -h)
            usage
            exit 0
            ;;

        *)
            echo "ERROR: Unknown parameter: $1"
            echo
            usage
            exit 1
            ;;
    esac
done


if [ -z ${TARGET+x} ]; then
    TARGET=${project}:${tag}
fi

if [ -z ${REGISTRY+x} ]; then
    REGISTRY=${image_repro}
fi

if [ -z ${SOURCE_DIR+x} ]; then
    SOURCE_DIR=`pwd`
fi

echo "Starting the build process in dir: ${BUILD_DIR}"

if [ "$clean" = true ] ; then
  if [[ ! "$(docker images -q ${build_image} 2> /dev/null)" == "" ]]; then
    echo "Removing build-image: ${build_image}"
    docker rmi ${build_image}
  fi
  if [ -d "${BUILD_DIR}" ]; then
    echo "Cleaning the build-dir: ${BUILD_DIR}"
    rm -rf "${BUILD_DIR}"
  fi
fi

build_bldimage

if [ ! -d "${BUILD_DIR}" ]; then
    mkdir -p ${BUILD_DIR}
fi

pushd ${BUILD_DIR}

artifacts_dir="${BUILD_DIR}/artifacts"


echo rm -rf $artifacts_dir
mkdir -p ${artifacts_dir}/lib
mkdir -p ${artifacts_dir}/bin
mkdir -p ${artifacts_dir}/deb

if [ ! -d "build" ]; then
    mkdir build
fi

echo "==================================================="
echo "Building ${project} libraries and binaries"
echo "Artifacts to: ${artifacts_dir}"
echo "==================================================="

docker run                                                           \
    --rm ${docker_run_args}                                          \
    -u $UID                                                          \
    --name "${project}-build"                                        \
    -e DO_STRIP=${strip}                                             \
    -e BUILD_DIR=/build                                              \
    -e BUILD_TYPE="${cmake_build_type}"                              \
    -v ${SOURCE_DIR}:/src                                            \
    -v ${BUILD_DIR}/build:/build                                     \
    -v ${artifacts_dir}:/artifacts                                   \
    ${build_image}                                                   \
    /src/docker/build-${project}.sh                                  \
    || die


target_image="${REGISTRY}/${TARGET}"
echo "==================================================="
echo "Making target: ${target_image}"
echo "==================================================="

if [ "$logbt" = true ] ; then
  cp -v "${SOURCE_DIR}/docker/logbt.sh" ${artifacts_dir}
  cp -v "${SOURCE_DIR}/docker/startup.sh" ${artifacts_dir}
  cp -v "${SOURCE_DIR}/docker/Dockerfile.${project}.bt" ${artifacts_dir}/Dockerfile
else
  cp -v "${SOURCE_DIR}/docker/Dockerfile.${project}" ${artifacts_dir}/Dockerfile
fi

pushd ${artifacts_dir}

docker build -t ${target_image} . || die "Failed to make target: ${target_image}"

if [ "$push" = true ] ; then
    docker push ${target_image}

    if [[ -n "${version// /}" ]]; then
        vtag=${REGISTRY}/${project}:${version}
        echo "Tagging and pushing: ${vtag}"
        docker tag ${target_image} ${vtag}
        docker push ${vtag}
    fi
fi

popd # ${artifacts_dir}
popd # ${BUILD_DIR}
