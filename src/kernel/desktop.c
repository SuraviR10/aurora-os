#include <stdint.h>
#include <stdbool.h>
#include "../auroralang/string.h"
#include "kernel.h"

// Simple Desktop GUI for AuroraOS
// Text-based GUI simulation for the OS

#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define DESKTOP_COLOR 0x1F
#define WINDOW_COLOR 0x70
#define TITLEBAR_COLOR 0x2F

static void desktop_print_char(char c, int x, int y, uint32_t color);
static void desktop_print_string(const char *str, int x, int y, uint32_t color);
static void draw_line(int x1, int y1, int x2, int y2, uint32_t color);

typedef struct {
    int id;
    char title[32];
    int x, y, width, height;
    bool visible;
    bool focused;
    uint32_t background_color;
    char content[1024];
    int connections[4]; // Connect to up to 4 other nodes
    int connection_count;
} ui_node_t;

#define MAX_UI_NODES 16
ui_node_t nodes[MAX_UI_NODES];
int node_count = 0;
int focused_node = -1;
char desktop_icons[16][32];

// Desktop state
bool desktop_mode = false;

// Forward declarations
static void set_focused_node(int index);
static void draw_node(ui_node_t *node);
static void draw_line(int x1, int y1, int x2, int y2, uint32_t color);

// Drawing Primitives Implementation
static void desktop_print_char(char c, int x, int y, uint32_t color) {
    term_set_cursor(x, y);
    term_setcolor((uint8_t)color);
    term_putchar(c);
}

static void desktop_print_string(const char *str, int x, int y, uint32_t color) {
    term_set_cursor(x, y);
    term_setcolor((uint8_t)color);
    term_write(str);
}

// Initialize desktop
void desktop_init() {
    desktop_mode = true;

    // Add some default icons
    kstrcpy(desktop_icons[0], "Terminal");
}

// Create a window
static int create_node(const char *title, int x, int y, int width, int height) {
    if (node_count >= MAX_UI_NODES) return -1;

    int index = node_count++;
    nodes[index].id = index + 1; // ID is 1-based
    kstrcpy(nodes[index].title, title);
    nodes[index].x = x;
    nodes[index].y = y;
    nodes[index].width = width;
    nodes[index].height = height;
    nodes[index].visible = true;
    nodes[index].focused = false;
    nodes[index].background_color = WINDOW_COLOR;
    nodes[index].content[0] = '\0';
    nodes[index].connection_count = 0;

    // Focus the new window
    set_focused_node(index);

    return index;
}

// Set focused window
static void set_focused_node(int index) {
    if (focused_node >= 0 && focused_node < node_count) {
        nodes[focused_node].focused = false;
    }
    focused_node = index;
    if (index >= 0 && index < node_count) {
        nodes[index].focused = true;
    }
}

// Draw desktop
void draw_desktop() {
    if (!desktop_mode) return;

    // Clear screen with desktop color
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            desktop_print_char(' ', x, y, DESKTOP_COLOR);
        }
    }

    // Draw desktop title
    desktop_print_string("AuroraOS Fluid Interface", 1, 0, 0x1F);
    desktop_print_string("ESC for shell | 'connect <id1> <id2>'", 40, 0, 0x1F);

    // Draw connections first (so they are in the background)
    for (int i = 0; i < node_count; i++) {
        if (!nodes[i].visible) continue;
        for (int j = 0; j < nodes[i].connection_count; j++) {
            int target_idx = nodes[i].connections[j] - 1;
            if (target_idx >= 0 && target_idx < node_count) {
                int x1 = nodes[i].x + nodes[i].width / 2;
                int y1 = nodes[i].y + nodes[i].height / 2;
                int x2 = nodes[target_idx].x + nodes[target_idx].width / 2;
                int y2 = nodes[target_idx].y + nodes[target_idx].height / 2;
                draw_line(x1, y1, x2, y2, 0x3A);
            }
        }
    }

    // Draw windows
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].visible) {
            draw_node(&nodes[i]);
        }
    }
}

// Draw a window
static void draw_node(ui_node_t *node) {
    uint32_t color = node->background_color;
    if (node->focused) {
        color = (color & 0xF0) | 0x0F; // Brighter for focused
    }

    // Window border
    for (int y = node->y; y < node->y + node->height; y++) {
        for (int x = node->x; x < node->x + node->width; x++) {
            if (y == node->y || y == node->y + node->height - 1 ||
                x == node->x || x == node->x + node->width - 1) {
                desktop_print_char(' ', x, y, TITLEBAR_COLOR);
            } else {
                desktop_print_char(' ', x, y, color);
            }
        }
    }

    // Title bar
    desktop_print_string(node->title, node->x + 2, node->y, TITLEBAR_COLOR);

    // Close button
    desktop_print_char('X', node->x + node->width - 2, node->y, 0x4F);

    // Window content
    int content_y = node->y + 1;
    char *line = node->content;
    while (*line && content_y < node->y + node->height - 1) {
        char *end = line;
        while (*end && *end != '\n' && (end - line) < node->width - 2) {
            end++;
        }

        for (char *c = line; c < end; c++) {
            desktop_print_char(*c, node->x + 1 + (c - line), content_y, color);
        }

        if (*end == '\n') {
            line = end + 1;
        } else {
            line = end;
        }
        content_y++;
    }
}

// Handle desktop input
void handle_desktop_input(char key) {
    if (key == 27) { // ESC
        desktop_mode = false;
        return;
    }

    // Handle window focus and clicks
    // Simplified - in real implementation would handle mouse
}

// Set window content
void set_window_content(int window_id, const char *content) {
    if (window_id >= 0 && window_id < node_count) {
        kstrcpy(nodes[window_id].content, content);
    }
}

// Close window
void close_window(int window_id) {
    if (window_id >= 0 && window_id < node_count) {
        nodes[window_id].visible = false;
        if (focused_node == window_id) {
            focused_node = -1;
        }
    }
}

void desktop_connect_nodes(int id1, int id2) {
    int idx1 = -1, idx2 = -1;
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].id == id1) idx1 = i;
        if (nodes[i].id == id2) idx2 = i;
    }

    if (idx1 != -1 && idx2 != -1 && nodes[idx1].connection_count < 4) {
        nodes[idx1].connections[nodes[idx1].connection_count++] = id2;
        draw_desktop(); // Redraw to show connection
    }
}

// Get desktop mode
bool is_desktop_mode() {
    return desktop_mode;
}

// Simple line drawing for the fluid interface connections
static void draw_line(int x1, int y1, int x2, int y2, uint32_t color) {
    for (int x = (x1 < x2 ? x1 : x2); x <= (x1 > x2 ? x1 : x2); x++) {
        desktop_print_char('-', x, y1, color);
    }
    for (int y = (y1 < y2 ? y1 : y2); y <= (y1 > y2 ? y1 : y2); y++) {
        desktop_print_char('|', x2, y, color);
    }
    desktop_print_char('+', x2, y1, color);
}

void desktop_show_info() {
    create_node("Terminal", 5, 3, 40, 10);
    create_node("Editor", 50, 8, 25, 12);
    draw_desktop();
}

void desktop_list_nodes() {
    for (int i = 0; i < node_count; i++) {
        term_printf("  Node %d: '%s' at (%d, %d)\n", nodes[i].id, nodes[i].title, nodes[i].x, nodes[i].y);
    }
}