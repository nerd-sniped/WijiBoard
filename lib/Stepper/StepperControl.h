#ifndef Stepper_Control
#define Stepper_Control

#include <AccelStepper.h>

// Declare global stepper objects
extern AccelStepper stepper1;
extern AccelStepper stepper2;

// Function Prototypes
void InitializeSteppers();
bool PerformHoming();
void MoveSteppersTo(float angle1, float angle2);
void MoveSteppersToWithoutHome(float angle1, float angle2);
void ReturnHome();

#endif // STEPPER_HOMING_H