#! /usr/bin/env bash
pwd

echo

find . | grep -Ei "\.aar" 2>/dev/null

rm -rf release 

mkdir release 

cp ./ijkplayer/ijkplayer-arm64/build/outputs/aar/ijkplayer-arm64-release.aar ./release  
cp ./ijkplayer/ijkplayer-armv7a/build/outputs/aar/ijkplayer-armv7a-release.aar ./release
cp ./ijkplayer/ijkplayer-java/build/outputs/aar/ijkplayer-java-release.aar ./release  

echo

find ./release | grep -Ei "\.aar" 2>/dev/null

echo

exit