#!/bin/bash

# DO NOT MODIFY COMMAND LINE USED TO RUN CMAKE. NOT COMPATIBLE WITH OUR BUILD POLICY.
# This is a helper script to make CMake a little more convenient to work with on our system.

rootdir=`pwd`

if [ "$1" = "" ]
then
  echo -e "******************************************************************************************"
  echo -e "*"
  echo -e "*\tclean"
  echo -e "*\t\tcleans the build folders"
  echo -e "*"
  echo -e "*\tdebug [num] [v]"
  echo -e "*\t\tcreates a debug build"
  echo -e "*\t\t[num]\tan optional argument specifying the number of cores to pass to make -j flag"
  echo -e "*\t\t[v]\tan optional argument specifying verbose mode (v or -v)"
  echo -e "*"
  echo -e "*\trelease [num] [v]"
  echo -e "*\t\tcreates a release build"
  echo -e "*\t\t[num]\tan optional argument specifying the number of cores to pass to make -j flag"
  echo -e "*\t\t[v]\tan optional argument specifying verbose mode (v or -v)"
  echo -e "*"
  echo -e "******************************************************************************************"
  exit 1
fi

if [[ "$1" == *"clean"* ]]
then
  echo "Cleaning CMake files"
  rm -r $rootdir/debug/ 2> /dev/null
  rm -r $rootdir/dist/ 2> /dev/null
  rm -r $rootdir/release/ 2> /dev/null
  rm -r $rootdir/unit_tests/ 2> /dev/null
  exit 0
fi

num_cores=1
if [[ "$2" =~ ^[0-9]+$ ]]
then
  echo "Using ${2} cores"
  num_cores=${2}
fi

v="-DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF"
if [ "$3" = "v" ] || [ "$3" = "-v" ]
then
  echo "Turning on VERBOSE mode"
  v="-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON"
fi

c=""
versionfile="/server/core/etc/version"
if [ -f "$versionfile" ] ; then
  zodiacversion=`cat $versionfile | head -n 1 | awk -F ':' {'print $2'} | awk -F '.' {'print $1'}` ;
  if [ $zodiacversion -ge 39 ] ; then
    c="-DCMAKE_C_COMPILER=/opt/rh/devtoolset-6/root/usr/bin/gcc -DCMAKE_CXX_COMPILER=/opt/rh/devtoolset-6/root/usr/bin/g++"
  fi
fi

if [[ "$1" = *"debug"* ]]
then
  echo "Creating debug build"
  mkdir -p $rootdir/debug/
  cd $rootdir/debug;
  cmake ${v} ${c} -DCMAKE_BUILD_TYPE=Debug $rootdir/;
  if [ 0 -ne $? ] ; then
    echo "CMake Build Type Setup Failed."
    exit 2
  fi
  cmake --build . -- -j ${num_cores}
  if [ 0 -ne $? ] ; then
    echo "CMake Debug Build Failed."
    exit 3
  fi
elif [[ "$1" = *"release"* ]]
then
  echo "Creating release build"
  mkdir -p $rootdir/release/
  cd $rootdir/release;
  cmake ${v} ${c} -DCMAKE_BUILD_TYPE=Release $rootdir/;
  if [ 0 -ne $? ] ; then
    echo "CMake Build Type Setup Failed."
    exit 4
  fi
  cmake --build . -- -j ${num_cores}
  if [ 0 -ne $? ] ; then
    echo "CMake Release Build Failed."
    exit 5
  fi
fi

exit 0

