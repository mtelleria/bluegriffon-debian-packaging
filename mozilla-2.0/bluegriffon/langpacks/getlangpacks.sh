#!/bin/sh

VERSION=$1
BG_VERSION=$2

LOCALES="cs de es-ES fi fr he hu it ja ko nl pl sl sr sv-SE zh-CN zh-TW"

R_VERSION=`echo $VERSION | sed -e "s/\./\\\\\./" | sed -e "s/\n//"`
R_BG_VERSION=`echo $BG_VERSION | sed -e "s/\./\\\\\./" | sed -e "s/\n//"`

wget ftp://ftp.mozilla.org/pub/firefox/nightly/latest-mozilla-central/firefox-$VERSION.en-US.langpack.xpi
for i in `echo $LOCALES`
do
    wget ftp://ftp.mozilla.org/pub/firefox/nightly/latest-mozilla-central-l10n/win32/xpi/firefox-$VERSION.$i.langpack.xpi
done

for i in `echo "en-US "$LOCALES`
do
    P=`pwd`
    cd /tmp
    rm -fr $i
    mkdir $i
    cd $i
    unzip $P/firefox-$VERSION.$i.langpack.xpi
    cat install.rdf | sed -e "s/firefox.mozilla.org/bluegriffon.org/" \
                    | sed -e "s/{ec8030f7-c20a-464f-9b0e-13a3a9e97384}/bluegriffon@disruptive-innovations.com/" \
                    | sed -e "s/$R_VERSION/$R_BG_VERSION/" \
                    > foo
    mv foo install.rdf
    cp chrome.manifest foo
    echo "locale bluegriffon "$i" base/locale/bluegriffon/
locale branding "$i" base/locale/branding/
locale fs "$i" extensions/fs/
locale gfd "$i" extensions/gfd/
locale cssproperties "$i" sidebars/cssproperties/
locale domexplorer "$i" sidebars/domexplorer/
locale scripteditor "$i" sidebars/scripteditor/
locale stylesheets "$i" sidebars/stylesheets/
locale tipoftheday "$i" extensions/tipoftheday/
" >> foo
    cat foo | grep -v "manifest" > chrome.manifest
    rm foo
    cd ..
    cp -r $i $P
    cd $P
    rm firefox-$VERSION.$i.langpack.xpi
done

find . -name \*.dtd -exec svn add {} \;
find . -name \*.properties -exec svn add {} \;
