#!/bin/bash
# This file sets up the platform for automated documentation builds with travis ci.

# 1. install dependencies (no LaTeX)
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends -y doxygen graphviz


# 2. Make documentation 
rm -rf doc/html
mkdir -p doc/html
cd src && doxygen Doxyfile

cd ../doc/html
git init
git config user.name "Travis CI Documentation Builder"
git config user.email "marius.appel@uni-muenster.de"
git add .
git commit -m "Automated documentation build[ci skip]"
git push --force --quiet "https://${GH_TOKEN}@${GH_REF}" master:gh-pages



