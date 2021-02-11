#!/bin/bash
set -ex

rm -rf build
mkdir build
cd build

echo "head branch: ${GITHUB_HEAD_REF}"
echo "base branch: ${GITHUB_BASE_REF}"
echo "branch: ${GITHUB_REF}"

BRANCH_NAME="${GITHUB_REF#refs/heads/}"
echo "branch name: ${BRANCH_NAME}"

if [ -z "${GITHUB_HEAD_REF}" ]; then
    # TODO: Code coverage
    cmake -DCMAKE_BUILD_TYPE=Debug ../
    make -j2
    ctest --verbose
else
    # TODO: ASAN
    echo "pull request sha: ${GITHUB_SHA}"
    cmake -DCMAKE_BUILD_TYPE=Debug ../
    make -j2
    ctest --verbose
fi
