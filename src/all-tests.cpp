#include "engine/core.h"

int main(int argc, const char * argv[])
{
    doctest::Context dt_context;
    dt_context.applyCommandLine(argc-1, argv+1);
    return dt_context.run();
}
