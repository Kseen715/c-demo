#include <stdio.h>
#include <wiringPi.h>

#include "common.h"

int main() {
    int i = 0;
    unsigned char gpio_num = 0;

    wiringPiSetup();

    gpio_num = getGpioNum();
    if (0xFF == gpio_num)
        printf("Failed to get the number of GPIO!\n");

    for (i = 0; i < gpio_num; i++)
        pinMode(i, OUTPUT);


    for (;; )
    {
        for (i = 0; i < gpio_num; i++)
            digitalWrite(i, HIGH);	// On
        delay(2000);

        for (i = 0; i < gpio_num; i++)
            digitalWrite(i, LOW);	// Off
        delay(2000);
    }

    return 0;
}