#include <stdio.h>
#include <wiringPi.h>

#include "common.h"

int main() {
    printf("Hello, World!\n");
    printf("Pins: %d\n", getGpioNum());
    return 0;
}