#!/usr/bin/sh

#脚本当前路径
scriptDir=$(cd $(dirname $0); pwd)
echo ${scriptDir};


# 选择编译器：优先使用支持 C++20 的 clang，其次是支持 C++20 的 gcc
choose_compiler() {
    supports_cxx20() {
        local cxx="$1"
        local tmp_dir
        tmp_dir=$(mktemp -d)
        echo 'int main() { return 0; }' > "${tmp_dir}/test.cpp"
        "$cxx" -std=c++20 -c "${tmp_dir}/test.cpp" -o "${tmp_dir}/test.o" >/dev/null 2>&1
        local ok=$?
        rm -rf "${tmp_dir}"
        return $ok
    }

    # 1) 优先尝试 clang++
    if command -v clang++ >/dev/null 2>&1; then
        if supports_cxx20 clang++; then
            export CC=clang
            export CXX=clang++
            echo "Using compiler: clang++ (supports C++20)"
            return 0
        fi
    fi

    # 2) 其次尝试 g++
    if command -v g++ >/dev/null 2>&1; then
        if supports_cxx20 g++; then
            export CC=gcc
            export CXX=g++
            echo "Using compiler: g++ (supports C++20)"
            return 0
        fi
    fi

    echo "Error: No compiler with C++20 support found." 1>&2
    echo "Please install a recent clang (recommended) or gcc (>= 10) and re-run build.sh." 1>&2
    exit 1
}

choose_compiler

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
