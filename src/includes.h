#ifndef STANDARDINCLUDES_H
#define STANDARDINCLUDES_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#if defined(__linux__) || defined(__unix__) || defined(__APPLE__)
	#include <unistd.h>
#endif

#endif // STANDARDINCLUDES_H