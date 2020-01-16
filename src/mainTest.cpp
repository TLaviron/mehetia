
#include <iostream>
#include "mainApplication.h"

int main() {

    try {
        MainApplication app;
        app.run();
        app.cleanup();
    } catch (const std::exception& e){
        std::cerr << e.what() << "\n";
    }

    return 0;
}
