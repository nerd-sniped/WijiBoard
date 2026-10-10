#include <Arduino.h>
#include "ServerLib.h"
#include "PositionControl.h"
#include "StepperControl.h"
#include <ElegantOTA.h>

// Function prototypes
void KeyPress();
void ProcessSingleCharacter();
void ProcessWordSequence();
void MoveWithinBounds(Coordinate from, Coordinate to);
Coordinate XYTableLookup(char key);
Angles calculateInverseKinematics(Coordinate coord);

#define MAX_SPLITS 3 // Levels of interstitial points to try before giving up on a direct path

bool newRequest = false;
char message = '*';
bool isWordMode = false;
bool wordReturnHome = true; // false = glide letter to letter, home only at the end
char lastWordChar = 0; // Letter the planchette is resting on, 0 = home
String currentWord = "";
int currentLetterIndex = 0;

void setup()
{
    Serial.begin(115200); 
    StartServer(); 
    InitializeSteppers(); 
    Serial.println("Steppers initialized");
    Serial.println("Homing Initialized...");
    while (!PerformHoming())
    {
        Serial.println("Homing...");
    }
    Serial.println("Homing finished");
}

void loop()
{
    KeyPress();
    ElegantOTA.loop();
}

void KeyPress()
{
    if (newRequest)
    {
        if (isWordMode) {
            ProcessWordSequence();
        } else {
            ProcessSingleCharacter();
            newRequest = false; // Only reset for single characters
        }
    }
}

void ProcessSingleCharacter()
{
    Coordinate coord = XYTableLookup(message);
    Serial.print("Single char - X: ");
    Serial.println(coord.x); 
    Serial.print("Y: ");
    Serial.println(coord.y);

    Angles ang = calculateInverseKinematics(coord);
    Serial.println("Moving to: " + String(message));
    Serial.println("Angle1: " + String(ang.theta1));
    Serial.println("Angle2: " + String(ang.theta2));
    
    MoveSteppersTo(ang.theta1, ang.theta2); // Still return home for single characters
}

// Works out the points to travel through to get from one coordinate to another without the
// planchette swinging out of bounds. Where the direct move would overshoot, an interstitial
// point is added halfway, pulled inward by the overshoot, and each half is checked again.
// Returns false if no safe route was found within MAX_SPLITS levels.
bool PlanPath(Coordinate from, Coordinate to, int splits, Coordinate *path, int &count)
{
    float overshoot = PathOvershoot(calculateInverseKinematics(from), calculateInverseKinematics(to));
    if (overshoot <= 0) {
        path[count++] = to;
        return true;
    }
    if (splits == 0) {
        return false;
    }

    Coordinate mid = {(from.x + to.x) / 2, (from.y + to.y) / 2 - overshoot};
    return PlanPath(from, mid, splits - 1, path, count) && PlanPath(mid, to, splits - 1, path, count);
}

// Letter to letter move without returning home, routed to stay within the board
void MoveWithinBounds(Coordinate from, Coordinate to)
{
    Coordinate path[1 << MAX_SPLITS];
    int count = 0;

    if (!PlanPath(from, to, MAX_SPLITS, path, count)) {
        // Home moves are known to be safe, so fall back to going via home
        Serial.println("No safe direct path - going via home");
        ReturnHome();
        path[0] = to;
        count = 1;
    }

    for (int i = 0; i < count; i++) {
        Angles ang = calculateInverseKinematics(path[i]);
        MoveSteppersToWithoutHome(ang.theta1, ang.theta2);
    }
}

void ProcessWordSequence()
{
    if (currentLetterIndex < currentWord.length()) {
        char currentChar = currentWord.charAt(currentLetterIndex);
        
        // Skip spaces and add pause
        if (currentChar == ' ') {
            Serial.println("Space detected - pausing");
            currentLetterIndex++;
            delay(1000); // Pause for space
            return;
        }
        
        // Check if character exists in lookup table
        Coordinate coord = XYTableLookup(currentChar);
        if (coord.x == 0 && coord.y == 100) { // Default coordinate means not found
            Serial.println("Skipping unsupported character: " + String(currentChar));
            currentLetterIndex++;
            return;
        }
        
        Serial.print("Word mode - Letter: " + String(currentChar) + " (");
        Serial.print(currentLetterIndex + 1);
        Serial.print("/");
        Serial.print(currentWord.length());
        Serial.println(")");
        
        Angles ang = calculateInverseKinematics(coord);
        
        if (wordReturnHome) {
            MoveSteppersTo(ang.theta1, ang.theta2);
        } else {
            if (lastWordChar == 0) {
                MoveSteppersToWithoutHome(ang.theta1, ang.theta2);
            } else if (currentChar == lastWordChar) {
                // Repeated letter (e.g. "LL") - go home first so the second one is visible
                ReturnHome();
                MoveSteppersToWithoutHome(ang.theta1, ang.theta2);
            } else {
                MoveWithinBounds(XYTableLookup(lastWordChar), coord);
            }
            delay(300); // Brief pause at position
            lastWordChar = currentChar;
        }

        currentLetterIndex++;
        
        // Check if word is complete
        if (currentLetterIndex >= currentWord.length()) {
            Serial.println("Word sequence completed: " + currentWord);
            
            // Return home only at the end of the complete word
            Serial.println("Returning to home position");
            ReturnHome();
            lastWordChar = 0;

            isWordMode = false;
            currentWord = "";
            currentLetterIndex = 0;
            newRequest = false; // Reset flag when word is complete
        } else {
            // Much shorter delay since we're not returning home
            delay(200); // Brief pause between letters
        }
    } else {
        // Word ended on a skipped character - make sure we are not left sitting on a letter
        if (lastWordChar != 0) {
            ReturnHome();
            lastWordChar = 0;
        }
        isWordMode = false;
        currentWord = "";
        currentLetterIndex = 0;
        newRequest = false;
    }
}