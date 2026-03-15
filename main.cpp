#include <iostream>
#include "application/application.h"

int main(){

    std::shared_ptr<flare::app::Application> app = std::make_shared<flare::app::Application>();
    try { 
        app->run();
    }
    catch(const std::exception& e) { 
        std::cout << e.what() << std::endl;
    }
    return 0;
}