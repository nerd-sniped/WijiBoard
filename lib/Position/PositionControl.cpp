#include <Arduino.h>
#include "PositionControl.h"
#include <math.h>
#include <algorithm>


// Initialize the lookupTable
std::map<char, Coordinate> lookupTable = {
    {'Q', {-66.5, 91.6}},
    {'W', {70.8, 92}},
    {'E', {-41.4, 125.5}},
    {'R', {-44, 96}},
    {'T', {1.3, 97.8}},
    {'Y', {117.8, 91.7}},
    {'U', {22.5, 98.3}},
    {'I', {53.5, 124.5}},
    {'O', {-112, 82}},
    {'P', {-90, 89}},
    {'A', {-135, 99}},
    {'S', {-19.8, 98}},
    {'D', {-68.5, 121.7}},
    {'F', {-19.5, 127}},
    {'G', {4, 128.4}},
    {'H', {31.3, 127.3}},
    {'J', {72.5, 121}},
    {'K', {93, 116.8}},
    {'L', {114.3, 110.2}},
    {'Z', {122, 75}},
    {'X', {95.9, 87.3}},
    {'C', {-94, 116}},
    {'V', {44.5, 97}},
    {'B', {-119, 109}},
    {'N', {-127.4, 75.1}},
    {'M', {124, 100.5}},

    {'+', {-110, 1}},
    {'-', {110, 1}},
    {'*', {110, -36}},
    {',', {-110, -36}},


    {'0', {-122.4, 44.2}},
    {'1', {-101.1, 53.3}},
    {'2', {-71.5, 60.4}},
    {'3', {-41.7, 65.2}},
    {'4', {-13, 66.5}},
    {'5', {16.3, 68}},
    {'6', {45.8, 64}},
    {'7', {75.7, 61.1}},
    {'8', {103.6, 53.8}},
    {'9', {124.1, 45}}
    // Add the rest of the alphabet here
};

Coordinate XYTableLookup(char key) {
    if (lookupTable.find(key) != lookupTable.end()) {
        return lookupTable[key];
    } else {
        // return a default coordinate or handle the error when the key is not found in the lookupTable
        return Coordinate{0, 100}; // example of returning a default coordinate
    }
}

Angles calculateInverseKinematics(Coordinate coord)
{
    const float d = 25.8;    // Length of base link
    const float d2 = 12.9;   // Half length of base link
    const float l1 = 85.0;  // Length of secondary links
    const float l2 = 110.0; // Length of Third links

    float x = coord.x;
    float y = coord.y;
    float xpd = x + d2;
    float xmd = x - d2;
    float angle1, angle2;

    float s = sqrt((pow(xmd, 2)) + (pow(y, 2)));
    Serial.print("S: ");
    Serial.println(s);
    float q = atan2(y, xmd);
    Serial.print("Q: ");   
    Serial.println(q, 6);
    float w1 = acos((pow(l2,2)-pow(s,2)-pow(l1,2))/(-2*l1*s));
    Serial.print("W1: ");
    Serial.println(w1, 6);
    float theta1 = q - w1;
    angle1 = theta1 * RAD_TO_DEG;
    Serial.print("Angle1: ");
    Serial.println(angle1);

    float t = sqrt(pow(xpd, 2) + pow(y, 2));
    Serial.print("T: ");
    Serial.println(t);
    float r = atan2(y, xpd);
    Serial.print("R: ");
    Serial.println(r, 6);
    float w2 = acos((pow(l2,2)-pow(t,2)-pow(l1,2))/(-2*l1*t));
    Serial.print("W2: ");
    Serial.println(w2, 6);
    float theta2 = r + w2;
    angle2 = theta2 * RAD_TO_DEG;
    Serial.print("Angle2: ");
    Serial.println(angle2);

    // Below the motors on the left side (Hello) atan2 wraps around, which would send the arms
    // the long way round. Keep each angle on the same turn as its home angle (0 and 180).
    if (angle1 < -180) angle1 += 360;
    if (angle2 < 0) angle2 += 360;

    return Angles{angle1, angle2};
}

// Inverse of calculateInverseKinematics(): where the planchette sits for a pair of motor angles
Coordinate calculateForwardKinematics(Angles ang)
{
    const float d2 = 12.9;   // Half length of base link
    const float l1 = 85.0;  // Length of secondary links
    const float l2 = 110.0; // Length of Third links

    float theta1 = ang.theta1 * DEG_TO_RAD;
    float theta2 = ang.theta2 * DEG_TO_RAD;

    // Elbow positions at the end of each secondary link
    float e1x = d2 + l1 * cosf(theta1);
    float e1y = l1 * sinf(theta1);
    float e2x = -d2 + l1 * cosf(theta2);
    float e2y = l1 * sinf(theta2);

    // Planchette is where the two third links meet, on the far side of the elbows
    float dx = e1x - e2x;
    float dy = e1y - e2y;
    float dist = sqrtf(dx * dx + dy * dy);
    float h = sqrtf(l2 * l2 - dist * dist / 4);

    return Coordinate{(e1x + e2x) / 2 - h * dy / dist, (e1y + e2y) / 2 + h * dx / dist};
}

