#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>

#define MAX_WIDTH  512
#define MAX_HEIGHT 256
#define MAX_PIPES  16
#define TURN_DELAY 3
#define BORDER_PADDING 2

typedef struct {
    int x, y;
} Point;

typedef struct {
    Point pos, from;
    int dir, prev_dir;
    int steps_since_turn;
    int first_move;
    int color_id;
} Pipe;

int term_width, term_height;
const char* screen[MAX_HEIGHT][MAX_WIDTH];
int color_map[MAX_HEIGHT][MAX_WIDTH];

char *colors[] = {
    "\033[31m", // red
    "\033[32m", // green
    "\033[33m", // yellow
    "\033[34m", // blue
    "\033[35m", // magenta
    "\033[36m", // cyan
    "\033[37m", // white
    "\033[90m"  // gray
};
const int color_count = sizeof(colors) / sizeof(colors[0]);
char *reset = "\033[0m";

int rainbow_mode = 0;
int flicker_mode = 0;
int base_color_index = 0;
int delay_ms = 100;
int pipe_count = 1;
int gui_mode = 0;

void print_help() {
    printf("Usage: pipes [options]\n\n");
    printf("Options:\n");
    printf("  --cli             Run in CLI (terminal) mode\n");
    printf("  --color [name]    Set pipe color: red, green, yellow, blue, magenta, cyan, white, gray, rainbow\n");
    printf("  --flicker         Enable flickering rainbow mode\n");
    printf("  --speed [ms]      Delay in milliseconds between frames (default: 100)\n");
    printf("  --hm [number]     How many pipes to generate (default: 1, max: 16)\n");
    printf("  -h                Show this help message\n");
    exit(0);
}

void print_no_args_msg() {
    printf("No mode selected. Run with -h for help.\n");
    exit(0);
}

void get_terminal_size() {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    term_width = w.ws_col;
    term_height = w.ws_row;
}

void clear_screen() {
    printf("\033[2J\033[H");
}

void draw_screen() {
    clear_screen();
    for (int y = 0; y < term_height; ++y) {
        for (int x = 0; x < term_width; ++x) {
            if (screen[y][x]) {
                int color = flicker_mode ? rand() % color_count : color_map[y][x];
                printf("%s%s", colors[color], screen[y][x]);
            } else {
                printf(" ");
            }
        }
        printf("\n");
    }
    printf("%s", reset);
    fflush(stdout);
}

void reset_screen() {
    for (int y = 0; y < MAX_HEIGHT; ++y)
        for (int x = 0; x < MAX_WIDTH; ++x) {
            screen[y][x] = NULL;
            color_map[y][x] = -1;
        }
}

int screen_filled() {
    for (int y = 0; y < term_height; ++y)
        for (int x = 0; x < term_width; ++x)
            if (screen[y][x] == NULL)
                return 0;
    return 1;
}

void sleep_ms(int ms) {
    usleep(ms * 1000);
}

Point get_random_start(int *dir) {
    Point p;
    *dir = rand() % 4;

    int min_x = BORDER_PADDING;
    int max_x = term_width - BORDER_PADDING;
    int min_y = BORDER_PADDING;
    int max_y = term_height - BORDER_PADDING;

    p.x = rand() % (max_x - min_x) + min_x;
    p.y = rand() % (max_y - min_y) + min_y;
    return p;
}

const char* get_pipe_char(int from_dir, int to_dir) {
    if (from_dir == to_dir) {
        return (to_dir == 0 || to_dir == 2) ? "─" : "│";
    }

    if (from_dir == 0 && to_dir == 1) return "┐";
    if (from_dir == 0 && to_dir == 3) return "┘";
    if (from_dir == 1 && to_dir == 0) return "┌";
    if (from_dir == 1 && to_dir == 2) return "┐";
    if (from_dir == 2 && to_dir == 1) return "┌";
    if (from_dir == 2 && to_dir == 3) return "└";
    if (from_dir == 3 && to_dir == 0) return "└";
    if (from_dir == 3 && to_dir == 2) return "┘";

    return "*";
}

void update_pipe(Pipe *p) {
    if (!p->first_move && p->prev_dir != p->dir &&
        p->from.x >= 0 && p->from.x < term_width &&
        p->from.y >= 0 && p->from.y < term_height &&
        screen[p->from.y][p->from.x] == NULL) {

        const char* ch = get_pipe_char(p->prev_dir, p->dir);
        screen[p->from.y][p->from.x] = ch;
        color_map[p->from.y][p->from.x] = rainbow_mode ? rand() % color_count : p->color_id;
    }

    switch (p->dir) {
        case 0: p->pos.x += 1; break;
        case 1: p->pos.y += 1; break;
        case 2: p->pos.x -= 1; break;
        case 3: p->pos.y -= 1; break;
    }

    if (p->pos.x >= 0 && p->pos.x < term_width &&
        p->pos.y >= 0 && p->pos.y < term_height &&
        screen[p->pos.y][p->pos.x] == NULL) {

        const char* ch = get_pipe_char(p->dir, p->dir);
        screen[p->pos.y][p->pos.x] = ch;
        color_map[p->pos.y][p->pos.x] = rainbow_mode ? rand() % color_count : p->color_id;
    }

    p->from = p->pos;
    p->prev_dir = p->dir;
    p->first_move = 0;

    if (p->steps_since_turn >= TURN_DELAY) {
        int t = rand() % 6;
        if (t == 0) { p->dir = (p->dir + 1) % 4; p->steps_since_turn = 0; }
        else if (t == 1) { p->dir = (p->dir + 3) % 4; p->steps_since_turn = 0; }
        else p->steps_since_turn++;
    } else {
        p->steps_since_turn++;
    }

    if (p->pos.x < BORDER_PADDING || p->pos.x >= term_width - BORDER_PADDING ||
        p->pos.y < BORDER_PADDING || p->pos.y >= term_height - BORDER_PADDING) {
        p->pos = get_random_start(&p->dir);
        p->from = p->pos;
        p->prev_dir = p->dir;
        p->steps_since_turn = 0;
        p->first_move = 1;
    }
}

void parse_args(int argc, char *argv[]) {
    if (argc == 1) print_no_args_msg();

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-h")) print_help();
        else if (!strcmp(argv[i], "--cli")) gui_mode = 0;
        else if (!strcmp(argv[i], "--gui")) gui_mode = 1;
        else if (!strcmp(argv[i], "--flicker")) {
            if (gui_mode) {
                printf("Warning: --flicker is not supported in GUI mode. Ignoring.\n");
            } else {
                flicker_mode = 1;
                rainbow_mode = 1;
            }
        }
        else if (!strcmp(argv[i], "--color") && i + 1 < argc) {
            ++i;
            char *c = argv[i];
            if (!strcmp(c, "rainbow")) rainbow_mode = 1;
            else if (!strcmp(c, "red")) base_color_index = 0;
            else if (!strcmp(c, "green")) base_color_index = 1;
            else if (!strcmp(c, "yellow")) base_color_index = 2;
            else if (!strcmp(c, "blue")) base_color_index = 3;
            else if (!strcmp(c, "magenta")) base_color_index = 4;
            else if (!strcmp(c, "cyan")) base_color_index = 5;
            else if (!strcmp(c, "white")) base_color_index = 6;
            else if (!strcmp(c, "gray")) base_color_index = 7;
        }
        else if (!strcmp(argv[i], "--speed") && i + 1 < argc) {
            delay_ms = atoi(argv[++i]);
        }
        else if (!strcmp(argv[i], "--hm") && i + 1 < argc) {
            pipe_count = atoi(argv[++i]);
            if (pipe_count > MAX_PIPES) pipe_count = MAX_PIPES;
        }
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));
    parse_args(argc, argv);

    if (gui_mode) {
        printf("[GUI] 3D mode selected... (to be implemented)\n");
        exit(0);
    }

    get_terminal_size();
    reset_screen();

    Pipe pipes[MAX_PIPES];
    for (int i = 0; i < pipe_count; i++) {
        pipes[i].pos = get_random_start(&pipes[i].dir);
        pipes[i].from = pipes[i].pos;
        pipes[i].prev_dir = pipes[i].dir;
        pipes[i].steps_since_turn = 0;
        pipes[i].first_move = 1;
        pipes[i].color_id = rainbow_mode ? rand() % color_count : base_color_index;
    }

    while (1) {
        for (int i = 0; i < pipe_count; i++) {
            update_pipe(&pipes[i]);
        }

        if (screen_filled()) {
            reset_screen();
            for (int i = 0; i < pipe_count; i++) {
                pipes[i].pos = get_random_start(&pipes[i].dir);
                pipes[i].from = pipes[i].pos;
                pipes[i].prev_dir = pipes[i].dir;
                pipes[i].steps_since_turn = 0;
                pipes[i].first_move = 1;
                pipes[i].color_id = rainbow_mode ? rand() % color_count : base_color_index;
            }
        }

        draw_screen();
        sleep_ms(delay_ms);
    }

    return 0;
}
