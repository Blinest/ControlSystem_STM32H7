#ifndef __KINEMATIC_H
#define __KINEMATIC_H
#include <stdint.h>
#include "CR.h"
/**********************************************************
***	编写作者：blinest

***	qq：1071378062
**********************************************************/

void calculate_L(float R[], float theta[], float phi, float deltaL[]);
double tendonCompensation(int seg, float angle_deg);
#endif
