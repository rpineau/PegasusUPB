#!/bin/bash

mkdir -p ROOT/tmp/PegasusUPB_X2/
cp "../PegasusUPB.ui" ROOT/tmp/PegasusUPB_X2/
cp "../PegasusAstro.png" ROOT/tmp/PegasusUPB_X2/
cp "../focuserlist PegasusUPB.txt" ROOT/tmp/PegasusUPB_X2/
cp "../build/Release/libPegasusUPB.dylib" ROOT/tmp/PegasusUPB_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.PegasusUPB_X2 --sign "$installer_signature" --scripts Scripts --version 1.0 PegasusUPB_X2.pkg
pkgutil --check-signature ./PegasusUPB_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.PegasusUPB_X2 --scripts Scripts --version 1.0 PegasusUPB_X2.pkg
fi

rm -rf ROOT
