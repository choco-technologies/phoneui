#include "dmod.h"

/**
 * @brief phoneui_view - the views of phoneui (screens/<screen>/index.html), as a package.
 *
 * The module is its views: one bundle (phoneui.dmv, its fonts) for each kind of screen, chosen when it is installed
 * (dmf-get --screen <id>). It does nothing when run.
 */
int main(int argc, char* argv[])
{
    Dmod_Printf("phoneui_view: the views of phoneui (phoneui.dmv, its fonts)\n");
    return 0;
}
