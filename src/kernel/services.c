#include "kernel.h"

/* ═══════════════════════════════════════════════════════════════════
   SYSTEM  TIMELINE
   ═══════════════════════════════════════════════════════════════════ */

static timeline_event_t tl_events[TIMELINE_MAX];
static int tl_count = 0;

void timeline_init(void) {
    kmemset(tl_events, 0, sizeof(tl_events));
    tl_count = 0;
}

void timeline_record(const char *category, const char *msg) {
    int idx = tl_count % TIMELINE_MAX;
    tl_events[idx].timestamp = timer_seconds();
    kstrncpy(tl_events[idx].message,  msg,      TIMELINE_MSG - 1);
    kstrncpy(tl_events[idx].category, category, 31);
    tl_count++;
}

void timeline_show(int last_n) {
    int total = tl_count < TIMELINE_MAX ? tl_count : TIMELINE_MAX;
    int start = (last_n > 0 && last_n < total) ? total - last_n : 0;
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  TIME(s)  CATEGORY   EVENT");
    term_writeln("  ──────────────────────────────────────────────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    for (int i = start; i < total; i++) {
        int idx = i % TIMELINE_MAX;
        uint8_t cat_color = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK);
        if (kstrcmp(tl_events[idx].category, "user") == 0)
            cat_color = VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK);
        else if (kstrcmp(tl_events[idx].category, "lang") == 0)
            cat_color = VGA_COLOR(VGA_LIGHT_MAGENTA, VGA_BLACK);
        else if (kstrcmp(tl_events[idx].category, "debug") == 0)
            cat_color = VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK);
        term_printf("  %-8u ", tl_events[idx].timestamp);
        term_setcolor(cat_color);
        term_printf("%-10s ", tl_events[idx].category);
        term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
        term_writeln(tl_events[idx].message);
    }
    term_printf("  Total events recorded: %d\n", tl_count);
}

void timeline_show_range(uint32_t from, uint32_t to) {
    int total = tl_count < TIMELINE_MAX ? tl_count : TIMELINE_MAX;
    for (int i = 0; i < total; i++) {
        int idx = i % TIMELINE_MAX;
        if (tl_events[idx].timestamp >= from && tl_events[idx].timestamp <= to) {
            term_printf("  [%u] %-10s %s\n",
                tl_events[idx].timestamp,
                tl_events[idx].category,
                tl_events[idx].message);
        }
    }
}

int timeline_count(void) { return tl_count; }

/* ═══════════════════════════════════════════════════════════════════
   ACTIVITY  WORKSPACE
   ═══════════════════════════════════════════════════════════════════ */

static activity_t activities[MAX_ACTIVITIES];
static int act_count = 0;
static char current_activity[ACT_NAME_LEN] = "default";

void activity_init(void) {
    kmemset(activities, 0, sizeof(activities));
    act_count = 0;
    /* Built-in default activity */
    activity_create("default", "General workspace");
    activity_create("system",  "System administration");
}

void activity_create(const char *name, const char *desc) {
    /* Check duplicate */
    for (int i = 0; i < act_count; i++)
        if (kstrcmp(activities[i].name, name) == 0) return;
    if (act_count >= MAX_ACTIVITIES) return;
    int idx = act_count++;
    kstrncpy(activities[idx].name,        name, ACT_NAME_LEN - 1);
    kstrncpy(activities[idx].description, desc, 127);
    ksnprintf(activities[idx].root_path, VFS_MAX_PATH, "/activities/%s", name);
    activities[idx].used = true;
    /* Create directory structure */
    vfs_mkdir(activities[idx].root_path);
    char sub[VFS_MAX_PATH];
    ksnprintf(sub, VFS_MAX_PATH, "%s/code",     activities[idx].root_path); vfs_mkdir(sub);
    ksnprintf(sub, VFS_MAX_PATH, "%s/data",     activities[idx].root_path); vfs_mkdir(sub);
    ksnprintf(sub, VFS_MAX_PATH, "%s/notes",    activities[idx].root_path); vfs_mkdir(sub);
    ksnprintf(sub, VFS_MAX_PATH, "%s/docs",     activities[idx].root_path); vfs_mkdir(sub);
    ksnprintf(sub, VFS_MAX_PATH, "%s/snapshots",activities[idx].root_path); vfs_mkdir(sub);
    char buf[128];
    ksnprintf(buf, sizeof(buf), "Activity created: %s", name);
    timeline_record("user", buf);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    term_printf("  Activity '%s' created at %s\n", name, activities[idx].root_path);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
}

void activity_open(const char *name) {
    for (int i = 0; i < act_count; i++) {
        if (kstrcmp(activities[i].name, name) == 0) {
            kstrcpy(current_activity, name);
            char buf[128];
            ksnprintf(buf, sizeof(buf), "Activity opened: %s", name);
            timeline_record("user", buf);
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
            term_printf("  Switched to activity: %s\n", name);
            term_printf("  Root: %s\n", activities[i].root_path);
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
            term_writeln("  Subdirectories: code/  data/  notes/  docs/  snapshots/");
            return;
        }
    }
    term_setcolor(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));
    term_printf("  Activity '%s' not found. Use: activity create %s\n", name, name);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
}

void activity_list(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  NAME              DESCRIPTION                    PATH");
    term_writeln("  ──────────────────────────────────────────────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    for (int i = 0; i < act_count; i++) {
        bool active = kstrcmp(activities[i].name, current_activity) == 0;
        if (active) term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
        term_printf("  %-17s %-30s %s%s\n",
            activities[i].name,
            activities[i].description,
            activities[i].root_path,
            active ? "  ◄ active" : "");
        if (active) term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    }
}

const char *activity_current(void) { return current_activity; }

/* ═══════════════════════════════════════════════════════════════════
   WORKSPACE  SNAPSHOTS
   ═══════════════════════════════════════════════════════════════════ */

static snapshot_t snapshots[MAX_SNAPSHOTS];

void snapshot_save(const char *name) {
    /* Find or create slot */
    int idx = -1;
    for (int i = 0; i < MAX_SNAPSHOTS; i++) {
        if (!snapshots[i].used) { idx = i; break; }
        if (kstrcmp(snapshots[i].name, name) == 0) { idx = i; break; }
    }
    if (idx < 0) { term_writeln("  Snapshot limit reached."); return; }
    kstrncpy(snapshots[idx].name, name, SNAP_NAME_LEN - 1);
    snapshots[idx].timestamp = timer_seconds();
    kstrcpy(snapshots[idx].active_activity, current_activity);
    snapshots[idx].used = true;
    /* Save snapshot metadata to VFS */
    char path[VFS_MAX_PATH], content[512];
    ksnprintf(path, VFS_MAX_PATH, "/snapshots/%s.snap", name);
    ksnprintf(content, sizeof(content),
        "snapshot=%s\ntimestamp=%u\nactivity=%s\n",
        name, snapshots[idx].timestamp, current_activity);
    vfs_create(path);
    vfs_write(path, content, 0);
    vfs_tag(path, "snapshot");
    char buf[128];
    ksnprintf(buf, sizeof(buf), "Snapshot saved: %s", name);
    timeline_record("user", buf);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
    term_printf("  Snapshot '%s' saved (t=%u, activity=%s)\n",
                name, snapshots[idx].timestamp, current_activity);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
}

bool snapshot_restore(const char *name) {
    for (int i = 0; i < MAX_SNAPSHOTS; i++) {
        if (snapshots[i].used && kstrcmp(snapshots[i].name, name) == 0) {
            kstrcpy(current_activity, snapshots[i].active_activity);
            char buf[128];
            ksnprintf(buf, sizeof(buf), "Snapshot restored: %s", name);
            timeline_record("user", buf);
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
            term_printf("  Snapshot '%s' restored.\n", name);
            term_printf("  Activity: %s  |  Saved at: t=%u\n",
                        snapshots[i].active_activity, snapshots[i].timestamp);
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
            return true;
        }
    }
    term_setcolor(VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK));
    term_printf("  Snapshot '%s' not found.\n", name);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    return false;
}

void snapshot_list(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  NAME              SAVED AT(s)  ACTIVITY");
    term_writeln("  ──────────────────────────────────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    int found = 0;
    for (int i = 0; i < MAX_SNAPSHOTS; i++) {
        if (!snapshots[i].used) continue;
        term_printf("  %-17s %-12u %s\n",
            snapshots[i].name, snapshots[i].timestamp,
            snapshots[i].active_activity);
        found++;
    }
    if (!found) term_writeln("  No snapshots saved yet.");
}

/* ═══════════════════════════════════════════════════════════════════
   SETTINGS
   ═══════════════════════════════════════════════════════════════════ */

aurora_settings_t g_settings;

void settings_init(void) {
    g_settings.developer_mode     = true;
    g_settings.learning_mode      = true;
    g_settings.runtime_tracing    = false;
    g_settings.predictive_alloc   = true;
    g_settings.perf_optimization  = true;
    g_settings.visualization_level = 2;
    kstrcpy(g_settings.theme, "aurora-dark");
}

void settings_show(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ╔══════════════════════════════════════════════════╗");
    term_writeln("  ║           AuroraOS  System  Settings             ║");
    term_writeln("  ╠══════════════════════════════════════════════════╣");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
#define BOOL_STR(b) ((b) ? "ON " : "OFF")
    term_printf("  ║  developer_mode      : %-24s║\n", BOOL_STR(g_settings.developer_mode));
    term_printf("  ║  learning_mode       : %-24s║\n", BOOL_STR(g_settings.learning_mode));
    term_printf("  ║  runtime_tracing     : %-24s║\n", BOOL_STR(g_settings.runtime_tracing));
    term_printf("  ║  predictive_alloc    : %-24s║\n", BOOL_STR(g_settings.predictive_alloc));
    term_printf("  ║  perf_optimization   : %-24s║\n", BOOL_STR(g_settings.perf_optimization));
    term_printf("  ║  visualization_level : %-24d║\n", g_settings.visualization_level);
    term_printf("  ║  theme               : %-24s║\n", g_settings.theme);
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ╚══════════════════════════════════════════════════╝");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    term_writeln("  Usage: set <key> <value>");
}

void settings_set(const char *key, const char *value) {
    bool on = kstrcmp(value, "on") == 0 || kstrcmp(value, "true") == 0 || kstrcmp(value, "1") == 0;
    if (kstrcmp(key, "developer_mode")     == 0) g_settings.developer_mode     = on;
    else if (kstrcmp(key, "learning_mode") == 0) g_settings.learning_mode      = on;
    else if (kstrcmp(key, "runtime_tracing")== 0) g_settings.runtime_tracing   = on;
    else if (kstrcmp(key, "predictive_alloc")== 0) g_settings.predictive_alloc = on;
    else if (kstrcmp(key, "perf_optimization")== 0) g_settings.perf_optimization = on;
    else if (kstrcmp(key, "visualization_level")== 0) g_settings.visualization_level = katoi(value);
    else if (kstrcmp(key, "theme")         == 0) kstrncpy(g_settings.theme, value, 31);
    else { term_printf("  Unknown setting: %s\n", key); return; }
    char buf[128];
    ksnprintf(buf, sizeof(buf), "Setting changed: %s = %s", key, value);
    timeline_record("user", buf);
    term_printf("  %s = %s\n", key, value);
}

/* ═══════════════════════════════════════════════════════════════════
   SYSTEM  MONITOR  /  DASHBOARD
   ═══════════════════════════════════════════════════════════════════ */

static void draw_bar(uint32_t used, uint32_t total, int width) {
    if (total == 0) return;
    int filled = (total > 0) ? (int)(used / (total / width)) : 0;
    term_write("  [");
    uint8_t bar_color = VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK);
    if (filled > width * 7 / 10) bar_color = VGA_COLOR(VGA_LIGHT_BROWN, VGA_BLACK);
    if (filled > width * 9 / 10) bar_color = VGA_COLOR(VGA_LIGHT_RED, VGA_BLACK);
    term_setcolor(bar_color);
    for (int i = 0; i < width; i++) term_putchar(i < filled ? '#' : '.');
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    term_write("]");
}

void monitor_show_memory(void) {
    uint32_t total, used, free_b;
    mem_stats(&total, &used, &free_b);
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ── Memory ──────────────────────────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    term_printf("  Total : %u MB\n", total / (1024*1024));
    term_printf("  Used  : %u KB  ", used / 1024);
    draw_bar(used, total, 40);
    term_printf("  %u%%\n", used * 100 / (total ? total : 1));
    term_printf("  Free  : %u KB\n", free_b / 1024);
}

void monitor_show_processes(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ── Processes ───────────────────────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    sched_list();
}

void monitor_show_timeline(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ── Timeline (last 20 events) ───────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    timeline_show(20);
}

void monitor_show_dashboard(void) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("\n  ╔══════════════════════════════════════════════════════════════════════════════╗");
    term_writeln("  ║                    AuroraOS  System  Dashboard                              ║");
    term_writeln("  ╚══════════════════════════════════════════════════════════════════════════════╝");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    term_printf("  Uptime: %u seconds  |  Activity: %s  |  Processes: %d\n",
                timer_seconds(), activity_current(), sched_count());
    monitor_show_memory();
    term_putchar('\n');
    monitor_show_processes();
    term_putchar('\n');
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_writeln("  ── Recent Timeline ─────────────────────────────────");
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    timeline_show(8);
}
