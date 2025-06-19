#include "globals.h"

int utilities::globals::highpass = 0x465686;
int utilities::globals::opusencode = 0x863E90;
int utilities::globals::opusdecode = 0x867BA0;

float utilities::globals::gain = 1.0f;

bool utilities::globals::isOpusHooked = false;
bool utilities::globals::isHighpassHooked = false;

bool utilities::globals::dbcheck = false;