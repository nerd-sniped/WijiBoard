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

#define MAX_PATH_POINTS 8     // Most points in a letter to letter route before giving up on it
#define LETTER_CLEARANCE 10.0f // Closest (mm) an interstitial point may sit to the centre of a letter
#define NUDGE_DISTANCE 6.0f    // How far (mm) the planchette dips towards home on a repeated letter

bool newRequest = false;
bool homeRequest = false;
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
    if (homeRequest)
    {
        // Drop whatever was queued and re-run the homing sequence
        homeRequest = false;
        newRequest = false;
        isWordMode = false;
        currentWord = "";
        currentLetterIndex = 0;
        lastWordChar = 0;

        Serial.println("Homing Initialized...");
        PerformHoming();
        Serial.println("Homing finished");
        return;
    }

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

// Moves an interstitial point off any letter it would sit on, so passing through it cannot be
// read as a selection. Never pushes upward, to stay clear of the top of the board.
Coordinate ClearOfLetters(Coordinate point)
{
    for (int pass = 0; pass < 3; pass++) {
        for (auto &entry : lookupTable) {
            float dx = point.x - entry.second.x;
            float dy = point.y - entry.second.y;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist >= LETTER_CLEARANCE) {
                continue;
            }
            if (dist < 0.1f) { // Dead centre - no direction to push in, so go down
                dx = 0;
                dy = -1;
                dist = 1;
            }
            point.x = entry.second.x + dx / dist * LETTER_CLEARANCE;
            point.y = entry.second.y - fabsf(dy) / dist * LETTER_CLEARANCE;
        }
    }
    return point;
}

// Works out the angles to travel through to get from one coordinate to another without the
// planchette swinging out of bounds. While the simulated route overshoots, an interstitial
// point is added halfway along the offending segment, pulled inward by the overshoot.
// Returns false if no safe route was found within MAX_PATH_POINTS.
bool PlanPath(Coordinate from, Coordinate to, Angles *angles, int &count)
{
    Coordinate path[MAX_PATH_POINTS];
    path[0] = to;
    angles[0] = calculateInverseKinematics(to);
    count = 1;

    while (true) {
        int worst;
        float overshoot = PathOvershoot(angles, count, worst);
        if (overshoot <= 0) {
            return true;
        }
        if (count == MAX_PATH_POINTS || isnan(overshoot)) {
            return false;
        }

        Coordinate start = worst == 0 ? from : path[worst - 1];
        Coordinate mid = {(start.x + path[worst].x) / 2, (start.y + path[worst].y) / 2 - overshoot};
        mid = ClearOfLetters(mid);

        // Insert the new point ahead of the segment's end point
        for (int i = count; i > worst; i--) {
            path[i] = path[i - 1];
            angles[i] = angles[i - 1];
        }
        path[worst] = mid;
        angles[worst] = calculateInverseKinematics(mid);
        count++;
    }
}

// Letter to letter move without returning home, routed to stay within the board
void MoveWithinBounds(Coordinate from, Coordinate to)
{
    Angles angles[MAX_PATH_POINTS];
    int count;

    if (!PlanPath(from, to, angles, count)) {
        // Home moves are known to be safe, so fall back to going via home
        Serial.println("No safe direct path - going via home");
        ReturnHome();
        angles[0] = calculateInverseKinematics(to);
        count = 1;
    }

    MoveSteppersAlong(angles, count);
}

// Dips a few mm towards home and back, so a repeated letter reads as a second selection
void NudgeLetter(Coordinate letter)
{
    Coordinate home = calculateForwardKinematics(Angles{0, 180});
    float dx = home.x - letter.x;
    float dy = home.y - letter.y;
    float dist = sqrtf(dx * dx + dy * dy);
    Coordinate dip = {letter.x + dx / dist * NUDGE_DISTANCE, letter.y + dy / dist * NUDGE_DISTANCE};

    MoveWithinBounds(letter, dip);
    MoveWithinBounds(dip, letter);
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
                // Repeated letter (e.g. "LL") - nudge off it and back so the second one is visible
                NudgeLetter(coord);
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