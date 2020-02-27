#!/bin/bash

set -e

#

script_name="$1"
func_name="$2"

[ ! "$func_name" ] && func_name="swd_info"
step_name="$script_name $func_name"

#####

function error() {
    echo "[$step_name]:" "$@" 1>&2
    exit 1
}

#####

[ ! "$TR" ] && error "\$TR undefined"

#####

. "$script_name"

if [ "$(type -t "$func_name")" = function ]
then
    [ "$func_name" != swd_info ] && echo "exec $step_name"

    "$func_name"
else
    if [ "$func_name" = swd_info ]; then
        echo '{}'
    else
        echo "Script '$script_name' is missing function '$func_name'."
        exit 1
    fi
fi
