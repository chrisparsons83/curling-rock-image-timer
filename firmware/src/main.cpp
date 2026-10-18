#include <Arduino.h>

#include "split_format.h"

void setup() {
    Serial.begin(115200);
}

void loop() {
    char split[12];
    curling::format_split(3705000, split, sizeof(split));
    Serial.printf("curling timer alive, sample split %s s\n", split);
    delay(1000);
}
