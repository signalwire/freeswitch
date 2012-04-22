#!/bin/bash

## files we might want to ignore
git ls-files -o --exclude-standard | sed -e "s:.:\/\\0:"

## files we are accidentally ignoring
git ls-files -i --exclude-standard | sed -e "s:.:\!\/\\0:"

