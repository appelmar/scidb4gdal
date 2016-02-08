#!/bin/bash
# This file sets up the platform for automated documentation builds with travis ci.

# 1. install dependencies (no LaTeX)
sudo apt-get update -qq
sudo apt-get install -y --no-install-recommends -y doxygen graphviz


# 2. Make documentation and push to gh-pages branch
rm -Rf doc/html
mkdir -p doc/html
git clone -b gh-pages --single-branch "git@github.com:mappl/scidb4gdal.git" doc/html

rm -Rf doc/html/*
cd src && doxygen Doxyfile


cd doc/html
git add .
git config user.name "Travis CI Documentation Builder"
git config user.email "marius.appel@uni-muenster.de"
git commit -m "Automated documentation build[ci skip]"
git push origin gh-pages
cd -