#ifndef PositionControl_H
#define PositionControl_H

#include <map>

// Define a struct to hold the X and Y coordinates
struct Coordinate {
    float x;
    float y;
};

struct Angles {
    float theta1;
    float theta2;
};

// Declare the lookupTable
extern std::map<char, Coordinate> lookupTable;

// Declare a function to initialize the lookupTable
Coordinate XYTableLookup(char key);
Angles calculateInverseKinematics(Coordinate coord);
Coordinate calculateForwardKinematics(Angles ang);

#endif // LOOKUPTABLE_H