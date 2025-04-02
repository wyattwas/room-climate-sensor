#ifndef STATE_H
#define STATE_H

enum class State
{
    Operation,
    Calibration_Start,
    Calibration_Wait_5_Min,
    Calibration_Wait_500_Ms,
    Calibration_Final_step
};

#endif
