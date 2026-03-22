#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../auroralang/string.h"

// System-wide constants
#define PAGE_SIZE 4096
#define HEAP_START 0x1000000 // 16MB
#define HEAP_SIZE  0x1000000 // 16MB

#define VGA_BASE ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

#define MAX_PROCS 64
#define PROC_NAME_LEN 32

#define VFS_MAX_NODES 256
#define VFS_MAX_PATH 128
#define VFS_MAX_TAGS 8
#define VFS_MAX_DATA 4096

#define TIMELINE_MAX 256
#define TIMELINE_MSG 128

#define MAX_ACTIVITIES 16
#define ACT_NAME_LEN 32

#define MAX_SNAPSHOTS 16
#define SNAP_NAME_LEN 32

#define MAX_PACKAGES 20
#define MAX_PACKAGE_NAME 32
#define MAX_PACKAGE_VERSION 16
#define MAX_DEPENDENCIES 5

// Process states
enum {
    PROC_DEAD,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED
};

// Package states
typedef enum {
    PACKAGE_INSTALLED,
    PACKAGE_AVAILABLE,
    PACKAGE_UPDATING,
    PACKAGE_REMOVING
} package_state_t;

// Settings
typedef struct {
    bool developer_mode;
    bool learning_mode;
    bool runtime_tracing;
    bool predictive_alloc;
    bool perf_optimization;
    int  visualization_level;
    char theme[32];
} aurora_settings_t;

// Timeline
typedef struct {
    uint32_t timestamp;
    char category[32];
    char message[TIMELINE_MSG];
} timeline_event_t;

// Activity
typedef struct {
    char name[ACT_NAME_LEN];
    char description[128];
    char root_path[VFS_MAX_PATH];
    bool used;
} activity_t;

// Snapshot
typedef struct {
    char name[SNAP_NAME_LEN];
    uint32_t timestamp;
    char active_activity[ACT_NAME_LEN];
    bool used;
} snapshot_t;

// VFS Types
#define VFS_MAX_VERSIONS_PER_FILE 8

typedef struct vfs_data_block {
    char data[VFS_MAX_DATA];
    uint32_t size;
    uint32_t ref_count;
} vfs_data_block_t;

typedef struct vfs_version {
    uint32_t timestamp;
    vfs_data_block_t* block;
    struct vfs_version* prev_version;
} vfs_version_t;

typedef struct {
    bool used;
    char path[VFS_MAX_PATH];
    char name[64];
    bool is_dir;
    uint32_t created, modified, size;
    int parent, tag_count;
    vfs_version_t* versions;
    char tags[VFS_MAX_TAGS][32];
    char activity[ACT_NAME_LEN], description[128];
} vfs_node_t;

// Kernel Types
typedef struct process {
    uint32_t pid;
    uint32_t host_pid; // For Symbiosis
    char name[PROC_NAME_LEN];
    uint32_t state;
    uint32_t priority;
    uint32_t created_at;
    uint32_t cpu_time;
    uint32_t memory_kb;
    char activity[ACT_NAME_LEN];
    uint32_t esp;
    uint32_t stack_base;
    struct process *next;
} process_t;

typedef struct {
    char name[MAX_PACKAGE_NAME];
    char version[MAX_PACKAGE_VERSION];
    char description[128];
    package_state_t state;
    uint32_t size_kb;
    uint32_t install_time;
    char dependencies[MAX_DEPENDENCIES][MAX_PACKAGE_NAME];
    uint32_t dep_count;
    bool system_package;
} package_t;

// Kernel API
void term_init();
void term_clear();
void term_printf(const char *format, ...);
void term_writeln(const char *s);
void term_write(const char *s);
void term_putchar(char c);
void term_set_cursor(int x, int y);
void term_setcolor(uint8_t color);

uint32_t timer_seconds();
void timeline_record(const char *category, const char *message);

// Hardware I/O
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ( "inb %1, %0" : "=a"(ret) : "Nd"(port) );
    return ret;
}

static inline void io_wait(void) {
    outb(0x80, 0);
}

// Scheduler
void sched_init();
void sched_tick();
uint32_t sched_spawn(const char *name, uint32_t priority, uint32_t host_pid);
void sched_list();
int sched_count();

// VFS
void vfs_init();
int vfs_create(const char *path);
int vfs_write(const char *path, const void *data, uint32_t size);
void vfs_tag(const char *path, const char *tag);
int vfs_read(const char *path, void *buf, uint32_t *size);
int vfs_mkdir(const char *path);
int vfs_delete(const char *path);
void vfs_ls(const char *path);

// Desktop
void desktop_init();
void desktop_show_info();
void desktop_list_nodes();
void desktop_connect_nodes(int id1, int id2);

// Services
void timeline_init();
void activity_init();
void activity_create(const char *name, const char *desc);
void activity_open(const char *name);
const char *activity_current(void);
void settings_init();
void settings_set(const char *key, const char *value);
void mem_stats(uint32_t *total, uint32_t *used, uint32_t *free_bytes);
void packages_init();
uint32_t list_packages(package_t *buffer, uint32_t max_count);
bool download_package(const char *name);

// System Init & Drivers
void mem_init(uint32_t mem_upper_kb);
void idt_init();
void pic_init();
void timer_init(uint32_t hz);
void keyboard_init();
void sti();
void cli();
bool keyboard_has_data(void);
char keyboard_getchar(void);
void irq_install_handler(int irq, void (*handler)(void));

// Runtime
void aurora_runtime_init();
void shell_main();

// Colors
#define VGA_COLOR(fg, bg) ((bg << 4) | fg)
#define VGA_BLACK 0
#define VGA_BLUE 1
#define VGA_GREEN 2
#define VGA_CYAN 3
#define VGA_RED 4
#define VGA_MAGENTA 5
#define VGA_BROWN 6
#define VGA_LIGHT_GREY 7
#define VGA_DARK_GREY 8
#define VGA_LIGHT_BLUE 9
#define VGA_LIGHT_GREEN 10
#define VGA_LIGHT_CYAN 11
#define VGA_LIGHT_RED 12
#define VGA_LIGHT_MAGENTA 13
#define VGA_LIGHT_BROWN 14
#define VGA_WHITE 15

// Graphics API (VGA Mode 13h)
#define SCREEN_WIDTH_GFX 320
#define SCREEN_HEIGHT_GFX 200

void gfx_init(); // Shows splash screen
void gfx_clear_screen(uint8_t color);
void gfx_draw_pixel(int x, int y, uint8_t color);
void gfx_draw_char(int x, int y, char c, uint8_t color);

extern aurora_settings_t g_settings;

// Memory allocation
void *kmalloc(size_t size);
void kfree(void *ptr);

#endif