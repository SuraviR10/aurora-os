#include <stdint.h>
#include <stdbool.h>
#include "../auroralang/string.h"
#include "../kernel/kernel.h"
#include "../auroralang/auroralang.h"

// Forward declarations for AI system
void ai_log_event(int type, const char* data);
void ai_generate_code_from_intent(const char* intent, char* code_out, size_t out_size);
void ai_explain_topic(const char *topic);
void analyze_code(const char *code);

void execute_command(char *cmd_line);
#define MAX_CMD_LEN 256
#define MAX_ARGS    10

typedef void (*command_func)(int argc, char *argv[]);
typedef struct {
    char *name;
    char *description;
    command_func func;
} command_t;

#define MAX_ALIASES 50
typedef struct {
    char name[32];
    char value[128];
} alias_t;

alias_t aliases[MAX_ALIASES];
int alias_count = 0;

// Forward declarations
void cmd_help(int argc, char *argv[]);
void cmd_run(int argc, char *argv[]);
void cmd_deploy(int argc, char *argv[]);
void cmd_apps(int argc, char *argv[]);
void cmd_process(int argc, char *argv[]);
void cmd_memory(int argc, char *argv[]);
void cmd_analyze(int argc, char *argv[]);
void cmd_explain(int argc, char *argv[]);
void cmd_clear(int argc, char *argv[]);
void cmd_ls(int argc, char *argv[]);
void cmd_cat(int argc, char *argv[]);
void cmd_touch(int argc, char *argv[]);
void cmd_ps(int argc, char *argv[]);
void cmd_desktop(int argc, char *argv[]);
void cmd_services(int argc, char *argv[]);
void cmd_packages(int argc, char *argv[]);
void cmd_install(int argc, char *argv[]);
void cmd_symspawn(int argc, char *argv[]);
void cmd_lsnodes(int argc, char *argv[]);
void cmd_connect(int argc, char *argv[]);
void cmd_alias(int argc, char *argv[]);
void cmd_do(int argc, char *argv[]);

command_t commands[] = {
    {"help",     "Show available commands",                    cmd_help},
    {"run",      "Execute AuroraLang file",                   cmd_run},
    {"deploy",   "Deploy application to desktop",             cmd_deploy},
    {"apps",     "List installed applications",               cmd_apps},
    {"process",  "Show running programs",                     cmd_process},
    {"memory",   "Show memory usage",                         cmd_memory},
    {"analyze",  "Analyze program structure",                 cmd_analyze},
    {"explain",  "Explain programming concepts",              cmd_explain},
    {"clear",    "Clear screen",                              cmd_clear},
    {"ls",       "List directory contents",                   cmd_ls},
    {"cat",      "Display file contents",                     cmd_cat},
    {"touch",    "Create empty file",                         cmd_touch},
    {"ps",       "Show process list",                         cmd_ps},
    {"desktop",  "Show desktop information",                  cmd_desktop},
    {"services", "Show system services",                      cmd_services},
    {"packages", "List installed packages",                   cmd_packages},
    {"install",  "Install package",                           cmd_install},
    {"symspawn", "Spawn symbiote: symspawn <pid> <name>",     cmd_symspawn},
    {"lsnodes",  "List UI nodes on desktop canvas",           cmd_lsnodes},
    {"connect",  "Connect two UI nodes: connect <id1> <id2>", cmd_connect},
    {"alias",    "Create alias: alias name=\"command\"",       cmd_alias},
    {"do",       "Execute intent: do \"your goal\"",           cmd_do},
    {NULL, NULL, NULL}
};

// ── Command implementations ───────────────────────────────────────────

void cmd_help(int argc, char *argv[]) {
    (void)argc; (void)argv;
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("Available Commands:");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    for (int i = 0; commands[i].name != NULL; i++) {
        term_printf("  %-10s - %s\n", commands[i].name, commands[i].description);
    }
}

void cmd_run(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: run <filename>"); return; }
    term_printf("Running %s\n", argv[1]);
    char buf[1024];
    uint32_t sz = sizeof(buf) - 1;
    if (vfs_read(argv[1], buf, &sz) == 0) {
        buf[sz] = '\0';
        aurora_run_string(buf);
    } else {
        term_setcolor(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));
        term_writeln("Error: Could not read file.");
        term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    }
}

void cmd_deploy(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: deploy <filename>"); return; }
    term_printf("Deploying %s...\n", argv[1]);
}

void cmd_apps(int argc, char *argv[]) {
    (void)argc; (void)argv;
    term_writeln("Installed Applications:");
    term_writeln("  Calculator");
    term_writeln("  Notes");
}

void cmd_process(int argc, char *argv[]) {
    (void)argc; (void)argv;
    term_writeln("PID   Application     Memory");
    term_writeln("1     shell           2KB");
}

void cmd_memory(int argc, char *argv[]) {
    (void)argc; (void)argv;
    uint32_t total, used, free_b;
    mem_stats(&total, &used, &free_b);
    term_printf("Total: %u KB\n", total / 1024);
    term_printf("Used:  %u KB\n", used  / 1024);
    term_printf("Free:  %u KB\n", free_b / 1024);
}

void cmd_analyze(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: analyze <filename>"); return; }
    char buf[1024];
    uint32_t sz = sizeof(buf) - 1;
    if (vfs_read(argv[1], buf, &sz) == 0) {
        buf[sz] = '\0';
        analyze_code(buf);
    } else {
        term_writeln("Error: Could not read file.");
    }
}

void cmd_explain(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: explain <concept>"); return; }
    ai_explain_topic(argv[1]);
}

void cmd_clear(int argc, char *argv[]) {
    (void)argc; (void)argv;
    term_clear();
}

void cmd_ls(int argc, char *argv[]) {
    const char *path = (argc > 1) ? argv[1] : "/";
    term_printf("Contents of %s:\n", path);
    vfs_ls(path);
}

void cmd_cat(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: cat <filename>"); return; }
    char buf[1024];
    uint32_t sz = sizeof(buf) - 1;
    if (vfs_read(argv[1], buf, &sz) == 0) {
        buf[sz] = '\0';
        term_write(buf);
        term_putchar('\n');
    } else {
        term_writeln("Error reading file.");
    }
}

void cmd_touch(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: touch <filename>"); return; }
    vfs_create(argv[1]);
    term_printf("Created: %s\n", argv[1]);
}

void cmd_ps(int argc, char *argv[]) {
    (void)argc; (void)argv;
    sched_list();
}

void cmd_desktop(int argc, char *argv[]) {
    (void)argc; (void)argv;
    desktop_show_info();
}

void cmd_services(int argc, char *argv[]) {
    (void)argc; (void)argv;
    term_writeln("System Services: running");
    timeline_record("shell", "Services status requested");
}

void cmd_packages(int argc, char *argv[]) {
    (void)argc; (void)argv;
    package_t pkg_buf[MAX_PACKAGES];
    uint32_t count = list_packages(pkg_buf, MAX_PACKAGES);
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("Name                 Version");
    term_writeln("----------------------------");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    for (uint32_t i = 0; i < count; i++)
        term_printf("  %-20s %s\n", pkg_buf[i].name, pkg_buf[i].version);
}

void cmd_install(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: install <package>"); return; }
    term_printf("Installing %s...\n", argv[1]);
    if (download_package(argv[1]))
        term_writeln("Done.");
    else
        term_writeln("Package not found.");
}

void cmd_symspawn(int argc, char *argv[]) {
    if (argc < 3) { term_writeln("Usage: symspawn <host_pid> <name>"); return; }
    uint32_t host_pid = (uint32_t)atoi(argv[1]);
    uint32_t pid = sched_spawn(argv[2], 1, host_pid);
    if (pid > 0)
        term_printf("Spawned '%s' PID=%u\n", argv[2], pid);
    else
        term_writeln("Failed to spawn.");
}

void cmd_lsnodes(int argc, char *argv[]) {
    (void)argc; (void)argv;
    desktop_list_nodes();
}

void cmd_connect(int argc, char *argv[]) {
    if (argc < 3) { term_writeln("Usage: connect <id1> <id2>"); return; }
    desktop_connect_nodes(atoi(argv[1]), atoi(argv[2]));
    term_printf("Connected node %s to %s\n", argv[1], argv[2]);
}

void cmd_alias(int argc, char *argv[]) {
    if (argc < 2) {
        for (int i = 0; i < alias_count; i++)
            term_printf("  %s = \"%s\"\n", aliases[i].name, aliases[i].value);
        return;
    }
    if (alias_count >= MAX_ALIASES) { term_writeln("Alias limit reached."); return; }
    char *eq = strchr(argv[1], '=');
    if (!eq) { term_writeln("Syntax: alias name=\"command\""); return; }
    *eq = '\0';
    strcpy(aliases[alias_count].name,  argv[1]);
    strcpy(aliases[alias_count].value, eq + 1);
    alias_count++;
    term_writeln("Alias created.");
}

void cmd_do(int argc, char *argv[]) {
    if (argc < 2) { term_writeln("Usage: do \"intent\""); return; }
    char code_buf[256];
    ai_generate_code_from_intent(argv[1], code_buf, sizeof(code_buf));
    term_writeln(code_buf);
}

// ── Shell main loop ───────────────────────────────────────────────────

void shell_main(void) {
    char cmd_buffer[MAX_CMD_LEN];
    int  buffer_pos = 0;

    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("\n  Welcome to AuroraOS Shell");
    term_writeln("  Type 'help' for available commands.\n");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));

    term_write("AuroraOS> ");

    while (true) {
        if (!keyboard_has_data()) {
            __asm__ volatile("hlt");
            continue;
        }

        char c = keyboard_getchar();

        if (c == '\n' || c == '\r') {
            term_putchar('\n');
            cmd_buffer[buffer_pos] = '\0';
            if (buffer_pos > 0) {
                execute_command(cmd_buffer);
            }
            buffer_pos = 0;
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
            term_write("AuroraOS> ");
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));

        } else if (c == '\b') {
            if (buffer_pos > 0) {
                buffer_pos--;
                term_putchar('\b');
            }
        } else if (c >= 32 && c <= 126 && buffer_pos < MAX_CMD_LEN - 1) {
            cmd_buffer[buffer_pos++] = c;
            term_putchar(c);
        }
    }
}

// ── Command dispatcher ────────────────────────────────────────────────

void execute_command(char *cmd_line) {
    while (*cmd_line == ' ') cmd_line++;
    if (*cmd_line == '\0') return;

    // Check aliases
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(cmd_line, aliases[i].name) == 0) {
            cmd_line = aliases[i].value;
            break;
        }
    }

    char *argv[MAX_ARGS];
    int   argc = 0;
    char *saveptr;
    char *token = strtok(cmd_line, " ", &saveptr);
    while (token && argc < MAX_ARGS) {
        argv[argc++] = token;
        token = strtok(NULL, " ", &saveptr);
    }
    if (argc == 0) return;

    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            ai_log_event(0, argv[0]);
            commands[i].func(argc, argv);
            return;
        }
    }

    ai_log_event(1, argv[0]);
    term_setcolor(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));
    term_printf("Unknown command: %s  (type 'help')\n", argv[0]);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
}
