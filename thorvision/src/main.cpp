#include "app.h"


int main(int argc, char *argv[])
{
    const App app(argc, argv);
    return app.exec();
}