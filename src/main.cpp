#include <Arduino.h>
#include "ServerLib.h"
#include "PositionControl.h"
#include "StepperControl.h"

// Function prototypes
void KeyPress();
void ProcessSingleCharacter();
void ProcessWordSequence();
Coordinate XYTableLookup(char key);
Angles calculateInverseKinematics(Coordinate coord);

bool newRequest = false;
char message = '*';
bool isWordMode = false;
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
        
        // Use direct movement for word sequences (no home return)
        MoveSteppersToWithoutHome(ang.theta1, ang.theta2);
        
        currentLetterIndex++;
        
        // Check if word is complete
        if (currentLetterIndex >= currentWord.length()) {
            Serial.println("Word sequence completed: " + currentWord);
            
            // Return home only at the end of the complete word
            Serial.println("Returning to home position");
            ReturnHome();
            
            isWordMode = false;
            currentWord = "";
            currentLetterIndex = 0;
            newRequest = false; // Reset flag when word is complete
        } else {
            // Much shorter delay since we're not returning home
            delay(200); // Brief pause between letters
        }
    } else {
        // Safety check - if we somehow get here, reset everything
        isWordMode = false;
        currentWord = "";
        currentLetterIndex = 0;
        newRequest = false;
    }
}