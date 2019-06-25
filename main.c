//
//  main.c
//  CPong
//
//  Created by 神崎航 on 2017/12/17.
//  Copyright © 2017年 神崎航. All rights reserved.
//

#include <stdio.h>
#include <stdbool.h>
#include "SDL2/SDL.h"

#include "globals.h"
#include "helper_funcs.h"

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

typedef struct _aiattrs {
    int frame_count;
    SDL_Point prediction;
} AIAttrs;

typedef struct _paddle {
    SDL_Rect rect;
    float speed;
    int direction;
    int score;
} Paddle;

typedef struct _ball {
    SDL_Rect rect;
    float direction_x;
    float direction_y;
    bool start;
    SDL_Color color;
    int speed;
} Ball;

Paddle player;
Paddle ai;
AIAttrs aiattrs;
Ball ball[NB_BALLS];
bool game_paused = false;
bool ai_lost = false;
unsigned int game_start;

float calc_speed(float current_speed, int direction)
{
    return f_clamp(current_speed + (PADDLE_ACCEL * abs(direction)
                                 - (PADDLE_DECEL * !abs(direction))), 0.0, 1.0);
}

void calc_paddle_movement(Paddle *paddle)
{
    paddle->speed = calc_speed(paddle->speed, paddle->direction);
    paddle->rect.y = f_clamp(paddle->rect.y + paddle->direction * paddle->speed * PADDLE_SPEED, TOP, BTM);
}

float away_from_paddle(Paddle *paddle)
{
    if(paddle->rect.x >= SCREEN_HALF)
        return -1;
    else
        return 1;
}

float away_from_ball(Ball ball)
{
    if(ball.rect.x >= SCREEN_HALF)
        return -1;
    else
        return 1;
}

void calc_ball_paddle_collision(Paddle *paddle, Ball *ball)
{
    bool bounce = false;
    float percent = remap(ball->rect.y + (ball->rect.h /2),
                    paddle->rect.y,
                    paddle->rect.y + paddle->rect.h,
                    -1.0, 1.0);

    while(SDL_HasIntersection(&ball->rect, &paddle->rect)) {
        bounce = true;
   
        ball->rect.x += away_from_paddle(paddle);
    }
    if(bounce) {
        if(fabsf(percent) > 1.0) {
            ball->direction_x = -ball->direction_x;
            ball->direction_y = -ball->direction_y;
        }
        else {
            ball->direction_x = -ball->direction_x;
            ball->direction_y = -percent + (paddle->speed * paddle->direction * SMACK_FORCE_MULT);
        }
    }
}

void calc_ball_collision(Ball *ball)
{
	bool bounce = false;
    if(ball->rect.x - (ball->rect.w  / 2) <= 0 || ball->rect.x + (ball->rect.w  / 2) >= SCREEN_WIDTH  ) {
        ball->direction_x = -ball->direction_x;
    }
    if(ball->rect.y - ball->rect.h <= 0 || 
			ball->rect.y + ball->rect.h >= SCREEN_HEIGHT)
    {
        ball->direction_y = -ball->direction_y;
    }
#ifdef CONG_EMPIRICAL  
    if(ball->rect.x >= SCREEN_WIDTH) {
        player.score++;
        if (player.score == 10) {
			ai_lost = true;
			game_paused = true;
		}
	}
#endif
	
  //calc_ball_paddle_collision(&ai, ball);
   //calc_ball_paddle_collision(&player,ball);
}

void calc_ball_intercollision() {
   bool bounce = false;
   // Balls between balls collision
   for(int i = 0; i < NB_BALLS; i++) {
	   for(int j = 0; j < NB_BALLS; j++) {
			if (&ball[j] == &ball[i]) continue;
			while (SDL_HasIntersection(&(ball[j].rect), &(ball[i].rect))) {
				bounce = true;
				ball[j].rect.x += away_from_ball(ball[i]);
			}
			if (bounce) {
				ball[j].direction_x = -ball[j].direction_x;
				ball[j].direction_y = -ball[j].direction_y;
				ball[i].direction_x = -ball[i].direction_x;
				ball[i].direction_y = -ball[i].direction_y;
			}
			bounce = false;
		}
	}
}

void calc_ball_movement(Ball *ball)
{
    ball->rect.x += (ball->speed * ball->direction_x);
    ball->rect.y += (ball->speed * ball->direction_y);
}

void ai_predict_ball(Ball *ball)
{
    if(aiattrs.frame_count > AI_PREDICT_STEP) {
        aiattrs.frame_count = 0;
        
        aiattrs.prediction.x = ai.rect.x;
        aiattrs.prediction.y = ball->rect.y;
    }
    aiattrs.frame_count++;
}

int ai_decision(Ball *ball)
{
    SDL_Point ball_center = get_rect_center(&ball->rect);
    
    if(ball_center.x >= AI_MIN_REACT && ball_center.x <= AI_MAX_REACT && ball->direction_x > 0) {
        ai_predict_ball(ball);
        if(aiattrs.prediction.y <= ai.rect.y + AI_PADDLE_UPPER_BOUND)
            return -1;
        else if(aiattrs.prediction.y >= ai.rect.y + AI_PADDLE_LOWER_BOUND)
            return 1;
        else
            return 0;
    }
    return 0;
}

bool handle_input()
{
    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT) {
            return false;
        }
        if(event.type == SDL_KEYDOWN) {
            if(event.key.keysym.sym == SDLK_UP)
                ball[0].direction_y--;
            else if(event.key.keysym.sym == SDLK_DOWN)
                ball[0].direction_y++;
			else if(event.key.keysym.sym == SDLK_RIGHT)
                ball[0].direction_x++;
			else if(event.key.keysym.sym == SDLK_LEFT)
                ball[0].direction_x--;
            else if(event.key.keysym.sym == SDLK_SPACE) {
				ball[0].direction_x = 0;
                ball[0].direction_y = 0;
			}
            else if(event.key.keysym.sym == SDLK_ESCAPE)
                return false;
            else if(event.key.keysym.sym == SDLK_p)
                game_paused = !game_paused;
        }
        else if(event.type == SDL_KEYUP) {
            if(event.key.keysym.sym == SDLK_UP || event.key.keysym.sym == SDLK_DOWN)
                ball[0].direction_x--;
                ball[0].direction_y--;
        }
    }
    return true;
}

void update()
{
    if(game_paused)
        return;
    
    /* Game logic */
    /*for(int i = 0; i < NB_BALLS; i++) {
		ai.direction = ai_decision(&ball[i]);
		calc_paddle_movement(&ai);
	}
    calc_paddle_movement(&player);*/
    
    for(int i = 0; i < NB_BALLS; i++) {
		calc_ball_intercollision(&ball[i]);
		calc_ball_movement(&ball[i]);
		calc_ball_collision(&ball[i]);
	}
}

/* 
 * Thx Ryozuki
 * https://stackoverflow.com/questions/28346989/drawing-and-filling-a-circle 
 * */
void draw_circle(SDL_Renderer*r, int cx, int cy, int radius, SDL_Color color)
{
    SDL_SetRenderDrawColor(r, color.r, color.g, color.b, color.a);
    for (int w = 0; w < radius * 2; w++)
    {
        for (int h = 0; h < radius * 2; h++)
        {
            int dx = radius - w; // horizontal offset
            int dy = radius - h; // vertical offset
            if ((dx*dx + dy*dy) <= (radius * radius))
            {
                SDL_RenderDrawPoint(r, cx + dx, cy + dy);
            }
        }
    }
}

void render()
{
	unsigned int lost_time;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
   // SDL_RenderFillRect(renderer, &player.rect);
   // SDL_RenderFillRect(renderer, &ai.rect);
    
    // We want a real ball (circle)
    //SDL_RenderFillRect(renderer, &ball.rect);
    for(int i = 0; i < NB_BALLS; i++) {
		draw_circle(renderer, ball[i].rect.x, ball[i].rect.y, ball[i].rect.h, ball[i].color);
	}
	
    /* draw scores */
    SDL_Rect rect;
    rect.w = SCORE_W;
    rect.h = SCORE_H;
    SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
    for(int i = 0; i < player.score; i++) {
        rect.x = PLAYER_SCORE_X + (i * (rect.w * 2));
        rect.y = SCORE_Y;
        SDL_RenderFillRect(renderer, &rect);
    }
    for(int i = 0; i < ai.score; i++) {
        rect.x = AI_SCORE_X - (i * (rect.w * 2));
        rect.y = SCORE_Y;
        SDL_RenderFillRect(renderer, &rect);
    }
    
    /* draw vertical lines */
    rect.h = GATE_H;
    rect.w = GATE_W;
    for(int i = 0; i < 60; i++) {
        rect.x = SCREEN_HALF;
        rect.y = i * (GATE_H * 2);
        SDL_RenderFillRect(renderer, &rect);
    }
    
    /* pause grayout */
    if(game_paused) {
        SDL_Rect grayout = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
        
        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 200);
        
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(renderer, &grayout);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

#ifdef CONG_EMPIRICAL         
        if (ai_lost && player.score > 0)  { 
			lost_time = SDL_GetTicks();
			printf ("AI lost game in %d ticks\n", lost_time - game_start);
			// Ok let's pretend it's a new game (but with same rand seed yet;)
			player.score = 0;
			game_start = lost_time; 
		}
#endif
    }
    
    SDL_RenderPresent(renderer);
}

void game_loop()
{
    bool running = true;
    unsigned int last_time = 0;
    unsigned int accumulator = 0;
    unsigned int current_time = game_start = SDL_GetTicks();
    printf ("Game starts at %d\n", current_time);
    
    while(running) {
        running = handle_input();
        
        current_time = SDL_GetTicks();
        unsigned int delta_time = current_time - last_time;
        last_time = current_time;
        
        /* Game Logic */
        accumulator += delta_time;
        while(accumulator > UPDATE_STEP_SIZE) {
            update();
            accumulator -= UPDATE_STEP_SIZE;
        }
        
        /* Render the stuff */
        render();
        
        /* Cap at fixed update */
        SDL_Delay(UPDATE_STEP_SIZE);
    }
}

void init_game()
{
	SDL_Color ball_color ;
	SDL_Color player_ball_color  = {255, 255, 255};
    player.rect.w = PADDLE_W;
    player.rect.h = PADDLE_H;
    //set_rect_center(&player.rect, SCREEN_HALF - PADDLE_GAP, (SCREEN_HEIGHT / 2));
    
    ai.rect.w = PADDLE_W;
    ai.rect.h = PADDLE_H;
    //set_rect_center(&ai.rect, SCREEN_HALF + PADDLE_GAP, (SCREEN_HEIGHT / 2));
    
    srand(SDL_GetTicks());
    // The 0th ball is player
    for(int i = 0; i < NB_BALLS; i++) {
		ball[i].rect.h = i == 0 ? BALL_SIZE : BALL_SIZE -2 ;
		ball[i].rect.w = i == 0 ? BALL_SIZE : BALL_SIZE -2 ;
		ball[i].start = true;
		ball[i].direction_x = 1;
		ball[i].direction_y = 1;
        ball[i].rect.x =  i_clamp (rand() % SCREEN_WIDTH, ball[i].rect.w, SCREEN_WIDTH - ball[i].rect.w);
        ball[i].rect.y =  i_clamp (rand() % SCREEN_HEIGHT,  ball[i].rect.h ,SCREEN_HEIGHT - ball[i].rect.h);
        ball_color.r = rand() % 255;
        ball_color.g = rand() % 255;
        ball_color.b = rand() % 255;
        ball[i].color = (i != 0 ? ball_color : player_ball_color);
        ball[i].speed = i_clamp (rand() % BALL_SPEED , 1, BALL_SPEED);
        printf ("Init ball[%d] %d,%d color (%d,%d,%d)\n", i , ball[i].rect.x, ball[i].rect.y, ball[i].color.r, ball[i].color.g, ball[i].color.b);
	}
    
    aiattrs.frame_count = 0;
}

int main(int argc, const char * argv[])
{
    SDL_Init(SDL_INIT_EVERYTHING);
    
    window = SDL_CreateWindow("Adrien-Circles", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
    
    init_game();
    game_loop();
    
    // Clean exit
    SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	window = NULL;
	renderer = NULL;
	SDL_Quit();
}
