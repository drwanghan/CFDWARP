#!/bin/sh

command -v git >/dev/null 2>&1 || { echo "gitnewversion requires git but it is not installed.  Aborting." >&2; exit 1; }

filecheck=".makefile-header-default"
if [ -f "$filecheck" ]; then
 if [ $# -eq 1 ]; then
  if git show-ref --tags $1 ; then
    echo Version $1 already committed
    exit 
  fi
  if git ls-remote --exit-code --tags origin $1 ; then
    echo Version $1 already committed on origin
    exit 
  fi
  echo 'setting new version in src/version.hh'
  echo '#define VERSION "'"$1"'"' > src/version.hh
  echo 'cleaning warp directory..'
  make cleanall
  make cleanbin
  echo 'copy to origin using git..'
  git add -A .
  git commit -a -m "$1" 
  git tag -d $1 > /dev/null 2>&1
  git tag -a $1 -m "$1"
  git push --tags origin master
  echo '[done]'
 else
  echo "gitnewversion.sh needs one argument: the new version string"
 fi
else
 echo "gitnewversion.sh must be run from the warp main directory."
fi


