#include "Engine.h"
//#include <cstdio>
#include <iostream>


int main(int argc, char* argv[]) {
    //printf("Start of main\n");
    std::cout << "Start of main\n";
    Engine e;
    e.init();
    
    // Load the level from JSON file
    e.loadFile("assets/level1.json");

    e.run();
    e.cleanup();
    return 0;
}
