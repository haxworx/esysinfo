#include "process.h"
#include "system.h"
#include "ui.h"

int main(void)
{
   eina_init();

   stats_poll();

   eina_shutdown();

   return 0;
}
