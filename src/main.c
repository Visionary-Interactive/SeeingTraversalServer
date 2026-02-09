#include "includes.h"
#include "traversalServer.h"

int main(int argc, char *argv[]) {
	TraversalServer_Init();
	TraversalServer_CreateServer("UDP", SERVER_PORT);

    srand(1);
    clock_t myClock = clock();
    clock_t now = clock();
    int elapsed_ms = 0;
    int ev;
    while (true)
    {
        now = clock();
        if ((int)(now - myClock) > 25)
        {
            myClock = now;
            while ((ev = TraversalServer_HandleEvents()) != 0)
            {
			}
            TraversalServer_SendPackets();
        }
    }
}
