# Master_Mind_Device_Driver

In this repo a character device driver module that plays the board game "Master Mind" is implemented.

MAKE MODULE
make -C /lib/modules/3.13.0-32-generic/build M=$PWD modules

CONNECTING MODULE
// default guess limit, 10
insmod ./mastermind.ko mmind_number="4283"
// set a guess limit, 25
insmod ./mastermind.ko mmind_number="4283" mastermind_guess_limit="25"

MAKE NOD
mknod -m 666 /dev/mastermind c 250 0

WRITE
echo "1234" > /dev/mastermind

READ
cat /dev/mastermind

DISCONNECTING MODULE
rmmod mastermind
rm -r /dev/mastermind*

CLEAR MAKE
make -C /lib/modules/3.13.0-32-generic/build M=$PWD clean

BUILD TESTS
gcc remaining_guess_test.c -o remaining_guess_test
gcc endgame_test.c -o endgame_test
gcc newgame_test.c -o newgame_test

RUN TESTS
./remaining_guess_test
./endgame_test
./newgame_test [mmind_number]
