#!/bin/bash

P=$PWD  # Get current path

# If user has given -p option then get that path and shift the arguments in order to get TLDs
while getopts ":p:" o; do
    case "${o}" in
        p)
            P="${P}/${OPTARG}"
            shift 2
            ;;
        *)
            echo "error"
            exit 1
            ;;
    esac
done

# For every TLD that we got
for TLD in "${@}"
do
    count=0 # initialize a counter
    for FILE in "$P"/*.out; do  # check every .out file in the path specified
        while read LINE; do # read each line of the file
            SPLITTED_LINE=(${LINE//" "/ })  # split it on space characher
            if [[ ${SPLITTED_LINE[0]} == *.${TLD} ]] ;  # if the first token ends with the desired tld
            then
                (( count += SPLITTED_LINE[1] )) # increment count according to the second token
            fi
        done < $FILE    # until the end of the file
    done
    echo -e 'TLD .'"${TLD}"' \tappears' "${count}" ' \ttimes'   # each time echo the tld and the number of times it appears
done