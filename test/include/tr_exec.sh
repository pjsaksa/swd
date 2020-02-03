#!/bin/bash

if [ ! "$TR" ]
then
    echo "\$TR needs to be defined."
    exit 1
fi

#####

. "$1"

if [ "$2" ]; then
    "$2"
else
    swd_info
fi
