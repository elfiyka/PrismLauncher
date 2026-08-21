#!/bin/bash
set -e

if ! git remote get-url upstream > /dev/null 2>&1; then
    git remote add upstream https://github.com/PrismLauncher/PrismLauncher
fi

git fetch upstream
git checkout develop
git rebase upstream/develop
git submodule update --init --recursive
