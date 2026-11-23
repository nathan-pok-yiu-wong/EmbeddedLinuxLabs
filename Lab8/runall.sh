#!/bin/bash


if [ $# -lt 1 ]
then
        echo "Usage : ./runall.sh 1|clean|submit"
        echo "        ./runall.sh 2 <13 bit chord>"
        echo "        ./runall.sh 3|4|5|6 <path to keyboard>"
        echo "        ./runall.sh additional [arguments for your program]"
        exit
fi

if [ $# -lt 2 ]
then
  case "$1" in 2|3|4|5|6)
        echo "You forgot either the keyboard path or part 2 chord!"
        echo "Usage : ./runall.sh 1|clean|submit"
        echo "        ./runall.sh 2 <13 bit chord>"
        echo "        ./runall.sh 3|4|5|6 <path to keyboard>"
        echo "        ./runall.sh additional [arguments for your program]"
        exit
  esac
fi

case "$1" in

1)  echo "-- Setting up Part 1 --"
    (cd partAdditional && make unload)
    cd part1 && make clean && make
    echo "-- Running Part 1, press <Ctrl-C> to quit --"
    exec ./part1
    ;;
2)  echo "-- Setting up Part 2 --"
    (cd partAdditional && make unload)
    cd part2 && make clean && make
    echo "-- Running Part 2, press <Ctrl-C> to quit --"
    exec ./part2 "$2"
    ;;
3)  echo "-- Setting up Part 3 --"
    (cd partAdditional && make unload)
    cd part3 && make clean && make
    echo "-- Running Part 3, press <Ctrl-C> to quit --"
    exec ./part3 "$2"
    ;;
4)  echo "-- Setting up Part 4 --"
    (cd partAdditional && make unload)
    cd part4 && make clean && make
    rmmod -f video
    insmod /home/root/Linux_Libraries/drivers/video.ko
    echo "-- Running Part 4, press <Ctrl-C> to quit --"
    exec ./part4 "$2"
    ;;
5)  echo "-- Setting up Part 5 --"
    (cd partAdditional && make unload)
    cd part5 && make clean && make
    rmmod -f video
    rmmod -f stopwatch
    rmmod -f KEY
    rmmod -f LEDR
    insmod ../stopwatch.ko
    insmod /home/root/Linux_Libraries/drivers/video.ko
    insmod /home/root/Linux_Libraries/drivers/LEDR.ko
    insmod /home/root/Linux_Libraries/drivers/KEY.ko
    echo "---   Running Part 5, press <Ctrl-C> to quit   ---"
    echo "---   Press KEY0 to start/stop recording       ---"
    echo "---   Press KEY1 to play back recording        ---"
    exec ./part5 "$2"
    ;;
6)  echo "-- Setting up Part 6 --"
    (cd partAdditional && make unload)
    cd part6 && make clean && make
    rmmod -f video
    rmmod -f stopwatch
    rmmod -f KEY
    rmmod -f LEDR
    rmmod -f audio
    insmod ../stopwatch.ko
    insmod /home/root/Linux_Libraries/drivers/video.ko
    insmod /home/root/Linux_Libraries/drivers/LEDR.ko
    insmod /home/root/Linux_Libraries/drivers/KEY.ko
    insmod /home/root/Linux_Libraries/drivers/audio.ko
    echo "---   Running Part 6, press <Ctrl-C> to quit   ---"
    echo "---   Press KEY0 to start/stop recording       ---"
    echo "---   Press KEY1 to play back recording        ---"
    exec ./part6 "$2"
    ;;
additional) echo "-- Setting up Additional Part --"
    if ! ls partAdditional/*.c > /dev/null 2>&1; then
        echo "partAdditional has no .c files. Put your additional part code there first (see README)."
        exit
    fi
    rmmod -f video
    rmmod -f stopwatch
    rmmod -f KEY
    rmmod -f LEDR
    rmmod -f audio
    (cd partAdditional && make unload)
    cd partAdditional && make clean && make && make load || exit
    [ -e /dev/stopwatch ] || insmod ../stopwatch.ko
    insmod /home/root/Linux_Libraries/drivers/video.ko
    insmod /home/root/Linux_Libraries/drivers/LEDR.ko
    insmod /home/root/Linux_Libraries/drivers/KEY.ko
    insmod /home/root/Linux_Libraries/drivers/audio.ko
    echo ""
    echo "---   To uninstall drivers: ./runall.sh clean     ---"
    PROG=$(make -s name)
    if [ -n "$PROG" ]; then
        echo "---   Running $PROG, press <Ctrl-C> to quit   ---"
        exec ./$PROG "${@:2}"
    fi
    ;;
"clean") echo "---  Cleaning Up!  ---"
   rmmod -f video
   rmmod -f stopwatch
   rmmod -f KEY
   rmmod -f LEDR
   rmmod -f audio
   (cd partAdditional && make unload)
   cd part1 && make clean
   cd ../part2 && make clean
   cd ../part3 && make clean
   cd ../part4 && make clean
   cd ../part5 && make clean
   cd ../part6 && make clean
   cd ../partAdditional && make clean
   ;;
"submit") echo "--- Creating submission archive.  Please upload to Quercus! ---"
   rmmod -f video
   rmmod -f stopwatch
   rmmod -f KEY
   rmmod -f LEDR
   rmmod -f audio
   (cd partAdditional && make unload)
   cd part1 && make clean
   cd ../part2 && make clean
   cd ../part3 && make clean
   cd ../part4 && make clean
   cd ../part5 && make clean
   cd ../part6 && make clean
   cd ../partAdditional && make clean
   cd ..
   tar -cjvf e8_$(date "+%Y.%m.%d-%H.%M.%S").tar.bz2 include part1 part2 part3 part4 part5 part6 partAdditional README runall.sh stopwatch.ko physical.c stopwatch_wrappers.c
   echo "---   Created submission archive.  Please upload to Quercus!   ---"
   ;;

*) echo "Usage : ./runall.sh 1|clean|submit"
   echo "        ./runall.sh 2 <13 bit chord>"
   echo "        ./runall.sh 3|4|5|6 <path to keyboard>"
   echo "        ./runall.sh additional [arguments for your program]"
   ;;

esac

