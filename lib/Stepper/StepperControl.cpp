#include "StepperControl.h"

#define FULLSTEP 4
#define STEP_PER_REV 2048

#define MAX_SPEED 600.0f    // Steps per second
#define ACCELERATION 100.0f // Steps per second^2

// Upper bound for the planchette: Y of 'G', plus slack for step rounding
#define MAX_Y 128.9f

// Interstitial points are passed without stopping once both motors are within this many steps
#define BLEND_STEPS 60

// Path simulation time step (seconds) and the most steps a single segment may take
#define SIM_TIMESTEP 0.02f
#define SIM_MAX_STEPS 3000

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

#define HOME_STEPS_1 -1024
#define HOME_STEPS_2 0

// Gearbox backlash for stepper1 and stepper2, in steps (28 steps = ~5 degrees)
const long backlashSteps[2] = {28, 28};

// Backlash tracking for each motor. The gearbox output only follows the motor once the slack
// has been taken up, so every reversal shifts the commanded position by the backlash.
struct MotorState
{
    long ideal;  // Where the output shaft should be, in steps
    int dir;     // Direction of the last move, +1 or -1
    long offset; // Extra steps currently commanded to hold the slack taken up
};
MotorState motors[2] = {{HOME_STEPS_1, 1, 0}, {HOME_STEPS_2, 1, 0}};

// Works out the position to command so a motor's output shaft ends up at the ideal position,
// and updates its backlash state. Slack sits between the command and command + backlash.
static long CommandFor(MotorState &motor, int index, long ideal)
{
    if (ideal != motor.ideal)
    {
        int dir = ideal > motor.ideal ? 1 : -1;
        if (dir != motor.dir)
        {
            motor.offset += dir * backlashSteps[index];
            motor.dir = dir;
        }
        motor.ideal = ideal;
    }
    return ideal + motor.offset;
}

// Commands a motor (0 = stepper1, 1 = stepper2) so its output shaft ends up at the ideal position
static void SetTarget(int index, long ideal)
{
    (index == 0 ? stepper1 : stepper2).moveTo(CommandFor(motors[index], index, ideal));
}

// Ideal step positions for a pair of angles. Steps run opposite to angles, and stepper1 drives theta2.
static void IdealSteps(Angles ang, long steps[2])
{
    steps[0] = -lround(ang.theta2 / stepAngle);
    steps[1] = -lround(ang.theta1 / stepAngle);
}

// How close (steps) both motors must get to an interstitial point before heading for the next
// one. Capped at half the segment so short segments still have an effect.
static float BlendDistance(float distance1, float distance2)
{
    return min((float)BLEND_STEPS, max(fabsf(distance1), fabsf(distance2)) / 2);
}

void InitializeSteppers()
{
    stepper1.setMaxSpeed(MAX_SPEED);
    stepper1.setAcceleration(ACCELERATION);
    stepper1.setCurrentPosition(0);
    stepper1.setSpeed(220);

    stepper2.setMaxSpeed(MAX_SPEED);
    stepper2.setAcceleration(ACCELERATION);
    stepper2.setCurrentPosition(0);
    stepper2.setSpeed(220);           
}

bool PerformHoming()
{
    // Start from the same state as power on so homing can be re-run at any time
    stepper1.enableOutputs();
    stepper2.enableOutputs();
    stepper1.setCurrentPosition(0);
    stepper2.setCurrentPosition(0);

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
    stepper1.setCurrentPosition(HOME_STEPS_1);
    stepper2.setCurrentPosition(HOME_STEPS_2);

    // Both motors finish homing travelling in the positive direction
    motors[0] = {HOME_STEPS_1, 1, 0};
    motors[1] = {HOME_STEPS_2, 1, 0};


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
    SetTarget(0, -stepsForMotor1);
    SetTarget(1, -stepsForMotor2);

    // Block until both steppers reach their designated position
    while ((stepper1.distanceToGo() != 0) || (stepper2.distanceToGo() != 0))
    {
        stepper1.run();
        stepper2.run();
    }
    delay(500);
    SetTarget(0, HOME_STEPS_1);
    SetTarget(1, HOME_STEPS_2);

    while (stepper1.distanceToGo() != 0 || stepper2.distanceToGo() != 0)
    {
        stepper1.run();
        stepper2.run();
    }
    stepper1.disableOutputs();
    stepper2.disableOutputs();
}

// Travels through a list of angles and stops on the last one. The points before it are
// interstitial: the motors head for the next point as they get close rather than stopping.
// Outputs stay enabled and there is no return home.
void MoveSteppersAlong(const Angles *path, int count)
{
    stepper1.enableOutputs();
    stepper2.enableOutputs();

    for (int i = 0; i < count; i++)
    {
        long steps[2];
        IdealSteps(path[i], steps);
        SetTarget(0, steps[0]);
        SetTarget(1, steps[1]);

        bool last = i == count - 1;
        float blend = BlendDistance(stepper1.distanceToGo(), stepper2.distanceToGo());
        bool moving = true;
        while (moving)
        {
            bool moving1 = stepper1.run();
            bool moving2 = stepper2.run();
            moving = moving1 || moving2;

            if (!last && labs(stepper1.distanceToGo()) <= blend && labs(stepper2.distanceToGo()) <= blend)
            {
                break;
            }
        }
    }
}

void MoveSteppersToWithoutHome(float angle1, float angle2)
{
    Angles target = {angle1, angle2};
    MoveSteppersAlong(&target, 1);
}

// One time step of a simulated AccelStepper heading for a target: speed up, unless it is time to brake
static void SimulateStep(float &position, float &speed, float target)
{
    float distance = target - position;
    float accel = distance > 0 ? ACCELERATION : -ACCELERATION;
    if (speed * distance > 0 && speed * speed / (2 * ACCELERATION) >= fabsf(distance))
    {
        accel = -accel;
    }

    speed = constrain(speed + accel * SIM_TIMESTEP, -MAX_SPEED, MAX_SPEED);
    if (fabsf(distance) <= fabsf(speed * SIM_TIMESTEP) + 0.01f && fabsf(speed) <= 2 * ACCELERATION * SIM_TIMESTEP)
    {
        position = target;
        speed = 0;
    }
    else
    {
        position += speed * SIM_TIMESTEP;
    }
}

// Simulates MoveSteppersAlong() from where the motors are now, backlash included. Each motor
// ramps independently, so the planchette swings off the straight line between points.
// Returns how far (mm) the path goes past MAX_Y; zero or negative means it stays in bounds.
// worst is set to the index of the point being travelled to when the path was at its highest.
float PathOvershoot(const Angles *path, int count, int &worst)
{
    MotorState state[2] = {motors[0], motors[1]};
    float commanded[2], output[2], target[2];
    float speed[2] = {0, 0};
    for (int m = 0; m < 2; m++)
    {
        commanded[m] = state[m].ideal + state[m].offset;
        output[m] = state[m].ideal;
    }

    float highest = -INFINITY;
    worst = 0;

    for (int i = 0; i < count; i++)
    {
        long steps[2];
        IdealSteps(path[i], steps);
        target[0] = CommandFor(state[0], 0, steps[0]);
        target[1] = CommandFor(state[1], 1, steps[1]);

        bool last = i == count - 1;
        float blend = BlendDistance(target[0] - commanded[0], target[1] - commanded[1]);

        for (int n = 0;; n++)
        {
            float remaining = max(fabsf(target[0] - commanded[0]), fabsf(target[1] - commanded[1]));
            if (last ? (remaining == 0 && speed[0] == 0 && speed[1] == 0) : remaining <= blend)
            {
                break;
            }
            if (n > SIM_MAX_STEPS) return NAN;

            for (int m = 0; m < 2; m++)
            {
                SimulateStep(commanded[m], speed[m], target[m]);
                // The output shaft only moves once the motor has taken up the slack
                output[m] = constrain(output[m], commanded[m], commanded[m] + backlashSteps[m]);
            }

            float y = calculateForwardKinematics(Angles{-output[1] * stepAngle, -output[0] * stepAngle}).y;
            if (isnan(y)) return NAN; // Arms cannot reach - never treat as safe
            if (y > highest)
            {
                highest = y;
                worst = i;
            }
        }
    }
    return highest - MAX_Y;
}

void ReturnHome()
{
    // Enable stepper outputs  
    stepper1.enableOutputs();
    stepper2.enableOutputs();

    // Use EXACT same home position as MoveSteppersTo()
    SetTarget(0, HOME_STEPS_1);
    SetTarget(1, HOME_STEPS_2);

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