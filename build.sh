#!/usr/bin/sh

#脚本当前路径
scriptDir=$(cd $(dirname $0); pwd)
echo ${scriptDir};


# 编译 basenode
cd $scriptDir
if [ ! -d build ]; then
    mkdir build
else 
    echo build exist
fi

cd $scriptDir/build

cmake -S $scriptDir/cmake -B $scriptDir/build

make -j8


# 拉子库
# git submodule update --remote --recursive
