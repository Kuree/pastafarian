#!/usr/bin/env bash
set -x

# download the latest github release of detector
curl -s https://api.github.com/repos/Kuree/pastafarian/releases/latest \
| grep "release.*zip" \
| cut -d : -f 2,3 \
| tr -d \" \
| wget -i -

# unzip it and remove the zip file
unzip release.zip
rm release.zip

# download slang
wget https://github.com/Kuree/binaries/raw/master/slang
chmod +x slang
