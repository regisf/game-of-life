/* 
 * Game Of Live using OpenMP
 * Copyright Regis FLORET 2014 and later
 * 
 */

#include <string.h>
#include <stdio.h>
#include <sys/time.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <omp.h>
#include <pthread.h>
#include <GL/gl.h>
#include <GL/glu.h>

// Declare constants. Better way than #define due to compiler optimisation
const Uint32 kBoardWidth = 40;
const Uint32 kMaxWidth = 39;// To avoid unuseful computation (kBoardWidth - 1)
const Uint32 kBoardHeight = 30;
const Uint32 kMaxHeight = 29;// To avoid unuseful computation (kBoardHeight - 1)
const Uint32 kTileSize = 20;
static SDL_Color kWhite =  { 0xFF, 0xFF, 0xFF };

#define ACTIVE 1
#define INACTIVE 0

#define Yes 1
#define No 0

/**
 * Button structure
 */
typedef struct Button
{
    SDL_Surface * surf[2];              // Contains both active and inactive surface
    SDL_Rect position;                  // Contains the button position
    char active;                        // Flag to mark the button active or not
} Button;

// Pre-declare the GameContainer structure
typedef struct GameContainer GameContainer;

// Declare the draw function type
typedef void (*DrawBoardFunc)(GameContainer);

// Declare the compute function type
typedef void (*ComputeBoardFunc)(int **, int **);

/**
 * Game structure.
 * We don't want global variables (because it's bad)
 */
struct GameContainer 
{
    SDL_Surface * screen;               // Store screen to avoid SDL_GetVideoSurface on each loop
    Button * startBtn;                  // Start button
    Button * stopBtn;                   // Stop button
    Button * resetBtn;                  // Reset button
    TTF_Font * defaultFont;             // The default font mainly for the FPS. 
    SDL_Surface * whiteSquare;          // Don't need to recreate the white square on each loop.
    DrawBoardFunc drawBoardFunc;
    ComputeBoardFunc computeBoardFunc;
    void (*compute_board_func)(int **, int **);
    int ** board;                       // The real board
    int ** temp;                        // The temporary board for computation
    int quit;                           // Flag to know if the game continue (1) or not (0)
    int playing;                        // Flah to know if the game is running (1) or not (0)
    int useOpenGL;
};


/** Create the button 
 * @param defaultFont The preloaded font
 * @param caption The button caption
 * @return The button
 */
static Button * button_create(TTF_Font * defaultFont, const char * caption)
{
    SDL_Surface * drawed = NULL;
    SDL_Rect middle;
    
    // Allocate the button structure
    Button * btn = (Button*)malloc(sizeof(Button));
    
    // Create the text caption
    drawed = TTF_RenderText_Blended(defaultFont, caption, kWhite);
    
    // Inactive button
    btn->surf[INACTIVE] = SDL_CreateRGBSurface(SDL_SWSURFACE, drawed->clip_rect.w + 10, drawed->clip_rect.h + 10, 32, 0, 0, 0, 0);
    SDL_FillRect(btn->surf[INACTIVE], NULL, 0xaeaeae);
    middle.x = (btn->surf[INACTIVE]->clip_rect.w - drawed->clip_rect.w)/2;
    middle.y = (btn->surf[INACTIVE]->clip_rect.h - drawed->clip_rect.h)/2;
    SDL_BlitSurface(drawed, NULL, btn->surf[INACTIVE], &middle);

    // Active button
    btn->surf[ACTIVE] = SDL_CreateRGBSurface(SDL_SWSURFACE, drawed->clip_rect.w + 10, drawed->clip_rect.h + 10, 32, 0, 0, 0, 0);
    SDL_FillRect(btn->surf[ACTIVE], NULL, 0x0000FF);
    middle.x = (btn->surf[ACTIVE]->clip_rect.w - drawed->clip_rect.w)/2;
    middle.y = (btn->surf[ACTIVE]->clip_rect.h - drawed->clip_rect.h)/2;
    SDL_BlitSurface(drawed, NULL, btn->surf[ACTIVE], &middle);
    
    // Release the text caption.
    SDL_FreeSurface(drawed);
   
    return btn;
}

/** Release all surface and the button
 * @param btn The allocated button structure
 */
static void button_dispose(Button * btn)
{
    SDL_FreeSurface(btn->surf[ACTIVE]);
    SDL_FreeSurface(btn->surf[INACTIVE]);
    free(btn);
}

/** Convenient function to set the button position
 * @param btn The allocated button structure
 * @param x The horizontal position
 * @param y The vertical position
 */
static void button_set_position(Button * btn, int x, int y) 
{
    btn->position.x = x;
    btn->position.y = y;
}

/** Convenient function to set the button active or inactive
 * @param btn The allocated button structure
 * @param act The flag 0 for inactive, 1 for active 
 */
static void button_set_active(Button * btn, char act)
{
    btn->active = act;
}

/** Test if a button is clicked
 * @param btn The allocated button structure
 * @param x The vertical 
 */
static char button_is_clicked(Button * btn, int x, int y)
{
    return (x > btn->position.x && x < btn->position.x + btn->surf[btn->active]->clip_rect.w && 
             y > btn->position.y && y < btn->position.y + btn->surf[btn->active]->clip_rect.h);
}

/** Draw the button on the screen
 * @param btn The button to draw
 * @param dest The screen
 */
static void button_draw(Button * btn, SDL_Surface * dest)
{
    SDL_BlitSurface(btn->surf[btn->active], NULL, dest, &(btn->position));
}

/** Compute the board without multi-threading (traditional way)
 * @param board An array  on which the computation is done
 * @param temp An array on which the result are set
 * @param height The vertical indice of the board
 */
static void board_compute_thread(int ** board, int ** temp, const int height)
{
    register Uint32 count = 0;
    register Uint32 width = 0;

    for(width=0; width < kBoardWidth; width++)
    {
        // Count how many cell are alive around
        count = 0;

        if (height)
        {
            // Top left
            if (width)
            {
                count += board[height-1][width-1];
            }

            // Top center
            count += board[height-1][width];

            // Top right
            if (width < kMaxWidth)
            {
                count += board[height-1][width+1];
            }
        }

        // center left
        if (width)
        {
            count += board[height][width-1];
        }

        // center right
        if (width < kMaxWidth)
        {
            count += board[height][width+1];
        }

        if (height < kMaxHeight)
        {
            // bottom left
            if (width)
            {
                count += board[height+1][width-1];
            }

            // bottom center
            count += board[height+1][width];

            // bottom right
            if (width < kMaxWidth)
            {
                count += board[height+1][width+1];
            }
        }

        // If the cell is dead and there's 3 neighbour alive. The cell become alive
        if (board[height][width] == 0 && count == 3)
        {
            temp[height][width] = 1;
        }
        // If the cell is alive and it have 2 or 3 cell alive around, it stay alive
        else if (board[height][width] == 1 && (count == 2 || count == 3))
        {
            temp[height][width] = 1;
        }
        // Otherwise, it's dead.
        else
        {
            temp[height][width] = 0;
        }
    }
}

/**
 * Do the computation
 * @param board
 * @param temp
 */
static void board_compute(int ** board, int ** temp)
{
    register Uint32 i,j;
    
    for(i=0; i < kBoardHeight; i++)
    {
        board_compute_thread(board, temp, i);
    }
    
    // Copy the temp board to the new
    for(i=0; i < kBoardHeight ; i++)
    {
        for(j=0; j < kBoardWidth ; j++)
        {
            board[i][j] = temp[i][j];
        }
    }
}

/**
 * Do the computation with OpenMP
 * @param board
 * @param temp
 */
static void board_compute_openmp(int ** board, int ** temp)
{
    register Uint32 i,j;
    
    // COmptute the board with the maximum possible core
    #pragma omp parallel for private(i) shared(board, temp)
    for(i=0; i < kBoardHeight; i++)
    {
        board_compute_thread(board, temp, i);
    }
    
    // Copy the temp board to the new
    for(i=0; i < kBoardHeight ; i++)
    {
        #pragma omp parallel for private(j) shared(board,temp)
        for(j=0; j < kBoardWidth ; j++)
        {
            board[i][j] = temp[i][j];
        }
    }
    // Ensure all threads are finished
    #pragma omp barrier
}

/**
 * Clear the board and the temp board (respond to a click on "Reset Button")
 * @param game
 */
static void board_reset(GameContainer * game)
{
    register Uint32 i,j;
    
    for (i = 0; i < kBoardHeight; i++) 
    {
        for (j = 0; j < kBoardWidth; j++ )
        {
            game->board[i][j] = 0;
            game->temp[i][j] = 0;
        }
    }
}

/**
 * Initialize the game board.
 * @param game
 */
static void initialize_game(GameContainer * game)
{
    register Uint32 i;
     
    
    // Store the video surface. We don't have to call this routine each time
    game->screen = SDL_GetVideoSurface();
    
    // Clean up default vars
    game->useOpenGL = No;
    game->playing = No;
    game->quit = No;
    
    // Get the font
    game->defaultFont = TTF_OpenFont("FreeSans.ttf", 16);
    
    // Create Start button
    game->startBtn = button_create(game->defaultFont, "Start");
    button_set_position(game->startBtn, 5, 5);
    button_set_active(game->startBtn, 1);
    
    // Create Stop button
    game->stopBtn  = button_create(game->defaultFont, "Stop");
    button_set_position(game->stopBtn, game->startBtn->surf[INACTIVE]->clip_rect.w + 10, 5);
    button_set_active(game->stopBtn, 0);
    
    // Create Reset button
    game->resetBtn = button_create(game->defaultFont, "Reset");        
    button_set_position(game->resetBtn, game->startBtn->surf[INACTIVE]->clip_rect.w + game->stopBtn->surf[INACTIVE]->clip_rect.w + 15, 5);
    button_set_active(game->resetBtn, 1);
        
    // Create the two boards. 
    game->board  = malloc(sizeof(int*) * kBoardHeight );
    game->temp   = malloc(sizeof(int*) * kBoardHeight );
    for(i = 0; i < kBoardHeight; i++)
    {
        game->board[i] = (int*)malloc(sizeof(int) * kBoardWidth);
        game->temp[i] = (int*)malloc(sizeof(int) * kBoardWidth);
    }
    
    // Create the white square. Don't need to create it on each loop
    game->whiteSquare = SDL_CreateRGBSurface(SDL_SWSURFACE, kTileSize, kTileSize, 32, 0, 0, 0, 0);
    SDL_FillRect(game->whiteSquare, NULL, 0xFFFFFF);
    
    // Clear both board
    board_reset(game);
}

/**
 * Be nice with the system on exiting (Old MacOS classic habit)
 * @param game
 */
static void dispose_game(GameContainer * game)
{
    Uint32 i;
    for(i = 0; i < kBoardHeight; i++)
    {
        free(game->board[i]);
        free(game->temp[i]);
    }
    SDL_FreeSurface(game->whiteSquare);
    TTF_CloseFont(game->defaultFont);
    button_dispose(game->startBtn);
    button_dispose(game->stopBtn);
    button_dispose(game->resetBtn);
}

/**
 * Draw the board in usual way
 * @param game
 */
static void draw_board(GameContainer game)
{
    register Uint32 i,j;
    
    for (i = 0; i < kBoardHeight; i++) 
    {
        for (j = 0; j < kBoardWidth; j++ )
        {
            SDL_Rect square = { 0, 0, kTileSize, kTileSize };
            if ( ! game.board[i][j])
                continue;
            square.y = i * kTileSize;
            square.x = j * kTileSize;
            SDL_BlitSurface(game.whiteSquare, NULL, game.screen, &square);
        }
    }
}

/**
 * Draw the board with OpenMP. As the "sprite" may not be displayed at the same place,
 * yes it's possible. 
 * @param game
 */
static void draw_board_openmp(GameContainer game)
{
    register Uint32 i,j;
    int ** board = game.board;
    SDL_Surface * surf = game.screen;
    SDL_Rect square = { 0, 0, kTileSize, kTileSize };
    
    for (i = 0; i < kBoardHeight; i++) 
    {
        #pragma omp parallel for private(j) shared(board,surf,square)
        for (j = 0; j < kBoardWidth; j++ )
        {
            if ( ! board[i][j])
                continue;
            square.y = i * kTileSize;
            square.x = j * kTileSize;
            SDL_BlitSurface(game.whiteSquare, NULL, surf, &square);
        }
    }
    #pragma omp barrier
}

typedef struct ThreadArg
{
    GameContainer * game;
    Uint32 i;
} ThreadArg;

static void draw_thread(ThreadArg * arg)
{
    register Uint32 j;
    SDL_Rect square = { 0, 0, kTileSize, kTileSize };

    for (j = 0; j < kBoardWidth; j++)
    {
        if ( ! arg->game->board[arg->i][j]) 
        {
            continue;
        }
        square.y = arg->i * kTileSize;
        square.x = j * kTileSize;
        SDL_BlitSurface(arg->game->whiteSquare, NULL, arg->game->screen, &square);
    }
}

static void draw_board_multithread(GameContainer game)
{
    register Uint32 i;
    
    for (i = 0; i < kBoardHeight; i++) 
    {
    }    
}


/**
 * Draw the FPS on the screen. We lost are 10 fps to do that but it's useful I think :)
 * @param screen
 * @param defaultFont
 * @param fps
 */
static void draw_fps(SDL_Surface * screen, TTF_Font * defaultFont, Uint32 fps)
{
    SDL_Surface * surf;
    char str[50];
    SDL_Rect pos = { 0, 10, 0, 0 };
    
    sprintf(str, "%u fps", fps);
    surf = TTF_RenderText_Blended(defaultFont, str, kWhite);
    pos.x = kBoardWidth * kTileSize - surf->clip_rect.w  - 10;
    SDL_BlitSurface(surf, NULL, screen, &pos);
    SDL_FreeSurface(surf);
}

static void draw_time(SDL_Surface * screen, TTF_Font * font,const char * title, const suseconds_t  time, const int posY)
{
    SDL_Surface * surf;
    char str[50];
    SDL_Rect pos = { 0, posY, 0, 0 };
    
    sprintf(str, "%s %d microsec", title, (long int)time);
    surf = TTF_RenderText_Blended(font, str, kWhite);
    pos.x = kBoardWidth * kTileSize - surf->clip_rect.w  - 10;
    SDL_BlitSurface(surf, NULL, screen, &pos);
    SDL_FreeSurface(surf);
}

/**
 * Get Microseconds because SDL_GetTicks have not enought granulosity
 */
static suseconds_t get_usec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_usec;
}

/**
 * Process the main loop
 * @param game
 */
static void do_main_loop(GameContainer game)
{
    SDL_Event evt;
    char mouseButtonDown = 0;
    unsigned long frame = 0;
    Uint32 ticks = 0, start = 0, result = 0;
    suseconds_t drawTime = 0;
    suseconds_t avgDraw = 0, avgCompute = 0;
    
    memset(&evt, 0, sizeof(SDL_Event));

    start = SDL_GetTicks();
    
    while ( ! game.quit )
    {
        ticks = SDL_GetTicks();
        while (SDL_PollEvent(&evt)) 
        {
            game.quit = evt.type == SDL_QUIT;
            
            if ( ! game.playing)
            {
                switch (evt.type)
                {
                    case SDL_MOUSEBUTTONDOWN:
                        
                        if (evt.button.button == SDL_BUTTON_LEFT)
                        {
                            if (button_is_clicked(game.startBtn, evt.button.x, evt.button.y))
                            {
                                game.playing = Yes;
                                button_set_active(game.resetBtn, 0);
                                button_set_active(game.startBtn, 0);
                                button_set_active(game.stopBtn, 1);
                            }
                            else if (button_is_clicked(game.resetBtn, evt.button.x, evt.button.y))
                            {
                                board_reset(&game);
                            }
                            else 
                            {
                                mouseButtonDown = Yes;
                                game.board[evt.button.y / kTileSize][evt.button.x / kTileSize] = 1;
                            }
                            
                        }
                        break;
                        
                    case SDL_MOUSEBUTTONUP:
                        if ( ! game.playing)
                        {
                                mouseButtonDown = evt.button.button != SDL_BUTTON_LEFT;
                        }
                        break;
                        
                    case SDL_MOUSEMOTION:
                        if (mouseButtonDown)
                        {
                            game.board[evt.motion.y / kTileSize][evt.button.x / kTileSize] = 1;
                        }
                        break;
                }
            } 
            else
            {
                if (evt.type == SDL_MOUSEBUTTONDOWN)
                {
                    game.playing = ! button_is_clicked(game.stopBtn, evt.button.x, evt.button.y);
                    button_set_active(game.resetBtn, !game.playing);
                    button_set_active(game.startBtn, !game.playing);
                    button_set_active(game.stopBtn,   game.playing);
                }
            }
        }
        if ( ! game.useOpenGL)
        {
            SDL_FillRect(game.screen, NULL, 0);
        }
        
        drawTime = get_usec();
        if (game.playing == Yes)
        {
            game.computeBoardFunc(game.board, game.temp);
        } 
        draw_time(game.screen, game.defaultFont, "Compute: ",get_usec() - drawTime, 30);
        
        drawTime = get_usec();
        game.drawBoardFunc(game);
        
        button_draw(game.startBtn, game.screen);
        button_draw(game.stopBtn,  game.screen);
        button_draw(game.resetBtn, game.screen);
        
        result = ticks - start;
        if (result)
        {
            draw_fps(game.screen, game.defaultFont, (++frame * 1000) / result);
        }
        draw_time(game.screen, game.defaultFont, "Draw: ", get_usec() - drawTime, 50);

        if ( ! game.useOpenGL)
        {
            SDL_Flip(game.screen);
        }
        else
        {
            glClear(GL_COLOR_BUFFER_BIT);

            glBegin(GL_TRIANGLES);
                glColor3ub(255,0,0);    glVertex2d(-0.75,-0.75);
                glColor3ub(0,255,0);    glVertex2d(0,0.75);
                glColor3ub(0,0,255);    glVertex2d(0.75,-0.75);
            glEnd();

            glFlush();
            SDL_GL_SwapBuffers();
        }
    }
}

/*
 * Entry point
 */
int main(int argc, char** argv) 
{
    GameContainer game;
    
    if (argc == 1)
    {
        game.computeBoardFunc = board_compute;
        game.drawBoardFunc = draw_board;
        printf("Using single core\n");
    }
    else if (argc == 2)
    {
        if ( ! strcmp(argv[1], "--openmp"))
        {
            game.computeBoardFunc = board_compute_openmp;
            game.drawBoardFunc = draw_board_openmp;
            printf("Using OpenMP with as much core as possible (%d)\n", omp_get_num_procs());
        }
        else if ( ! strcmp(argv[1], "--thread"))
        {
            game.computeBoardFunc = board_compute;
            game.drawBoardFunc = draw_board_multithread;
            printf("Using multithreading\n");
        }
        else if ( ! strcmp(argv[1], "--opengl"))
        {
            game.useOpenGL = Yes;
            game.computeBoardFunc = board_compute;
            game.drawBoardFunc = draw_board;
        }
        else
        {
            fprintf(stderr, "Unknow argument: %s\n", argv[1]);
            return (EXIT_FAILURE);
        }
    }
    else
    {
        fprintf(stderr, "Only --openmp argument may be used for this program\n");
        return (EXIT_FAILURE);        
    }
    
    // Initialize our libs
    if (game.useOpenGL)
    {
        SDL_Init(SDL_OPENGL);
    }
    else
    {
        SDL_Init(SDL_INIT_EVERYTHING);
    }
    TTF_Init();
    
    // Create the video screen
    SDL_SetVideoMode(kBoardWidth * kTileSize, kBoardHeight * kTileSize, 32, 0);
    
    // Create the game
    initialize_game(&game);

    do_main_loop(game);
    
    // Exit gently
    dispose_game(&game);
    TTF_Quit();
    SDL_Quit();
    
    return (EXIT_SUCCESS);
}
