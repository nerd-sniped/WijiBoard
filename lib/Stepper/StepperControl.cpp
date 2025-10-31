#include <AccelStepper.h>

#define FULLSTEP 4
#define STEP_PER_REV 2048

// Define stepper motor control pins
#define motorPin1 4 // IN1 on ULN2003 ==> Blue   on 28BYJ-48
#define motorPin2 7 // IN2 on ULN2004 ==> Pink   on 28BYJ-48
#define motorPin3 5 // IN3 on ULN2003 ==> Yellow on 28BYJ-48
#define motorPin4 6 // IN4 on ULN2003 ==> Orange on 28BYJ-48

#define motorPin5 8
#define motorPin6 9
#define motorPin7 11
#define motorPin8 10

// Initialize AccelStepper objects for two stepper motors
AccelStepper stepper1(AccelStepper::FULL4WIRE, motorPin1, motorPin2, motorPin3, motorPin4);
AccelStepper stepper2(AccelStepper::FULL4WIRE, motorPin5, motorPin7, motorPin6, motorPin8);

// Stepper specifications
const float stepAngle = 0.17578125; // Degrees per step for your motor 360 / 2048

void InitializeSteppers()
{
    stepper1.setMaxSpeed(600);        
    stepper1.setAcceleration(100);    
    stepper1.setCurrentPosition(0);
    stepper1.setSpeed(220);           

    stepper2.setMaxSpeed(600);       
    stepper2.setAcceleration(100);    
    stepper2.setCurrentPosition(0);
    stepper2.setSpeed(220);           
}

bool PerformHoming()
{
    stepper1.moveTo(1024);
    stepper2.moveTo(2048);
    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }

    stepper2.setCurrentPosition(0);

    stepper1.moveTo(-1050);
    stepper2.move(-1300);

    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }

    stepper1.setCurrentPosition(0);

    stepper1.moveTo(550);
    stepper2.moveTo(-530);

    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }
    stepper1.setCurrentPosition(-1024);
    stepper2.setCurrentPosition(0);


    stepper1.disableOutputs();
    stepper2.disableOutputs();


    return true; // Indicate successful homing
}

void MoveSteppersTo(float angle1, float angle2)
{

    // Calculate the number of steps needed for each angle
    short stepsForMotor1 = round(angle2 / stepAngle);
    short stepsForMotor2 = round(angle1 / stepAngle);

    Serial.println("stepsformotor1: " + String(angle2 / stepAngle));
    Serial.println("stepsformotor2: " + String(angle1 / stepAngle));

    stepper1.enableOutputs();
    stepper2.enableOutputs();

    // Command the steppers to move
    stepper1.moveTo(-stepsForMotor1);
    stepper2.moveTo(-stepsForMotor2);

    // Block until both steppers reach their designated position
    while ((stepper1.distanceToGo() != 0) || (stepper2.distanceToGo() != 0))
    {
        stepper1.run();
        stepper2.run();
    }
    delay(500);
    stepper1.moveTo(-1024);
    stepper2.moveTo(0);

    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }
    stepper1.disableOutputs();
    stepper2.disableOutputs();
}

void MoveSteppersToWithoutHome(float angle1, float angle2)
{
    // Use SAME coordinate calculations as MoveSteppersTo()
    short stepsForMotor1 = round(angle2 / stepAngle);
    short stepsForMotor2 = round(angle1 / stepAngle);

    Serial.println("Word mode - stepsformotor1: " + String(angle2 / stepAngle));
    Serial.println("Word mode - stepsformotor2: " + String(angle1 / stepAngle));

    stepper1.enableOutputs();
    stepper2.enableOutputs();

    // Use SAME movement pattern as MoveSteppersTo() but without home return
    stepper1.moveTo(-stepsForMotor1);
    stepper2.moveTo(-stepsForMotor2);

    // Wait for movement to complete
    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }

    // Brief pause at position
    delay(300);
    
    // Keep outputs enabled for next movement
    // DO NOT return home or disable outputs
}

void ReturnHome()
{
    // Enable stepper outputs  
    stepper1.enableOutputs();
    stepper2.enableOutputs();

    // Use EXACT same home position as MoveSteppersTo()
    stepper1.moveTo(-1024);
    stepper2.moveTo(0);

    // Wait for movement to complete
    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }

    delay(500);

    // Disable outputs to save power
    stepper1.disableOutputs();
    stepper2.disableOutputs();
}