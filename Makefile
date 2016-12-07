all:
	gcc -g -std=c99 -o ca cellaut.c glad.c -rdynamic -lglfw3 -lrt -lm -ldl -lX11 -pthread -lXrandr -lXinerama -lXxf86vm -lXcursor -lasound -ljack -lportaudio -lsndfile
sor:
	g++ -g -std=c++11 -o sormod sormod.cpp
