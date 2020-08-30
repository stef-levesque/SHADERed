#!/bin/bash

echo "Bundle binaries into bin/SHADERed.app"

pushd ../../bin

rm -rf osx_tools.app
rm -rf SHADERed.app

cp -rf ../Misc/macOS/osx_tools.app .
cp -f ./SHADERed osx_tools.app/Contents/MacOS/
cp -rf ./data osx_tools.app/Contents/Resources/
cp -rf ./plugins osx_tools.app/Contents/Resources/
cp -rf ./templates osx_tools.app/Contents/Resources/
cp -rf ./themes osx_tools.app/Contents/Resources/
cp -f ./SHADERed.icns osx_tools.app/Contents/Resources/
mv osx_tools.app SHADERed.app

popd

echo "Done"