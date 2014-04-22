PROJECT_NAME=gamelive
SDL_FLAGS=`sdl-config --cflags`
SDL_LIBS=`sdl-config --libs` -lSDL_ttf
OTHER_FLAGS=-fopenmp

all:
	gcc main.c -o ${PROJECT_NAME} ${SDL_FLAGS} ${OTHER_FLAGS} ${SDL_LIBS} -O3

debug:
	gcc main.c -o ${PROJECT_NAME} ${SDL_FLAGS} ${OTHER_FLAGS} ${SDL_LIBS} -O0 -g


   
