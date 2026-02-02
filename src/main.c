#include "includes.h"
#include "traversalServer.h"

int main(int argc, char *argv[]) {
	TraversalServer_Init();
	TraversalServer_CreateServer("UDP", SERVER_PORT);

    clock_t myClock = clock();
    clock_t now = clock();
    int elapsed_ms = 0;
    int ev;
    while (true)
    {
        now = clock();
        if ((int)(now - myClock) > 20)
        {
            myClock = now;
            ev = TraversalServer_HandleEvents();
            TraversalServer_SendPackets();
        }
    }
}
