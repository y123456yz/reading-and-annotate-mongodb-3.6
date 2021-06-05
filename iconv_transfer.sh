#!/bin/sh  
#=====================  
# configure 
#=====================  

function fun_dir(){
for child in `ls $1`
do
    if [[ -f $1/$child ]]
    then
        echo "convert $1/$child"
        iconv -f utf-8 -t gb2312 $1/$child > $2/$child
        if (( $? != 0 ))
            then
                cp $1/$child $2/$child
                echo "cp $1/$child $2/$child"
            fi
    elif [[ -d $1/$child ]]
    then
        mkdir $2/$child
        fun_dir $1/$child $2/$child
    fi
done
}

if (( $# != 2 ))
    then
    echo "parameter error"
    exit
    fi
 
fun_dir ${1%/} ${2%/}

exit

