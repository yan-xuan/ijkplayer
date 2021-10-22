#! /usr/bin/env bash
echo start 
echo remove old "release" dir
rm -rf release 

echo find aar file
find . | grep -Ei "\.aar" 2>/dev/null
echo

#_pwd=`pwd`
#echo ${_pwd}
echo create new release dir: `pwd`/release
mkdir release 

echo copy armv7a and arm64 related file
cp ./ijkplayer/ijkplayer-arm64/build/outputs/aar/ijkplayer-arm64-release.aar ./release  
cp ./ijkplayer/ijkplayer-armv7a/build/outputs/aar/ijkplayer-armv7a-release.aar ./release
cp ./ijkplayer/ijkplayer-java/build/outputs/aar/ijkplayer-java-release.aar ./release  

echo 'release' dir file list:

find ./release | grep -Ei "\.aar" 2>/dev/null

echo

exit

