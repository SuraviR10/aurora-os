#include "kernel.h"
#include "../auroralang/string.h"

/* ═══════════════════════════════════════════════════════════════════
   SEMANTIC  VIRTUAL  FILE  SYSTEM
   – Standard path-based operations
   – Per-file tags, activity association, description
   – Semantic search: find files related to a query
   – Time-Travel: Versioning for all file writes
   ═══════════════════════════════════════════════════════════════════ */

// For now, a simple pool of blocks and versions
static vfs_data_block_t data_block_pool[VFS_MAX_NODES];
static vfs_version_t version_pool[VFS_MAX_NODES * VFS_MAX_VERSIONS_PER_FILE];
static int version_pool_count = 0;

static vfs_node_t nodes[VFS_MAX_NODES];
static int node_count = 0;

/* ── Helpers ───────────────────────────────────────────────────────── */
static int alloc_node(void) {
    for (int i = 0; i < VFS_MAX_NODES; i++)
        if (!nodes[i].used) { kmemset(&nodes[i], 0, sizeof(vfs_node_t)); nodes[i].used = true; return i; }
    return -1;
}

static int find_node(const char *path) {
    for (int i = 0; i < VFS_MAX_NODES; i++)
        if (nodes[i].used && kstrcmp(nodes[i].path, path) == 0) return i;
    return -1;
}

static __attribute__((unused)) void build_path(const char *parent, const char *name, char *out) {
    if (kstrcmp(parent, "/") == 0)
        ksnprintf(out, VFS_MAX_PATH, "/%s", name);
    else
        ksnprintf(out, VFS_MAX_PATH, "%s/%s", parent, name);
}

/* ── Init ──────────────────────────────────────────────────────────── */
void vfs_init(void) {
    kmemset(nodes, 0, sizeof(nodes));
    /* Root */
    int r = alloc_node();
    kstrcpy(nodes[r].path, "/");
    kstrcpy(nodes[r].name, "/");
    nodes[r].is_dir = true;
    nodes[r].created = timer_seconds();
    nodes[r].parent  = -1;
    node_count = 1;

    /* Standard directories */
    vfs_mkdir("/home");
    vfs_mkdir("/home/user");
    vfs_mkdir("/system");
    vfs_mkdir("/system/bin");
    vfs_mkdir("/system/lib");
    vfs_mkdir("/activities");
    vfs_mkdir("/projects");
    vfs_mkdir("/snapshots");
    vfs_mkdir("/tmp");
    vfs_mkdir("/docs");

    /* Seed a few demo files */
    vfs_create("/home/user/welcome.txt");
    vfs_write("/home/user/welcome.txt",
        "Welcome to AuroraOS!\n"
        "Type 'help' in the shell to see all commands.\n"
        "Try: activity create MyProject\n"
        "     timeline\n"
        "     aurora run /home/user/demo.al\n", 0);
    vfs_tag("/home/user/welcome.txt", "docs");
    vfs_tag("/home/user/welcome.txt", "welcome");

    vfs_create("/home/user/demo.al");
    vfs_write("/home/user/demo.al",
        "# AuroraLang demo program\n"
        "let name = \"AuroraOS\"\n"
        "let version = 2\n"
        "print \"Hello from \" + name + \" v\" + str(version)\n"
        "loop i from 1 to 5 {\n"
        "    print \"  Step \" + str(i)\n"
        "}\n"
        "let result = add(10, 32)\n"
        "print \"10 + 32 = \" + str(result)\n"
        "function add(a, b) {\n"
        "    return a + b\n"
        "}\n", 0);
    vfs_tag("/home/user/demo.al", "code");
    vfs_tag("/home/user/demo.al", "auroralang");
    vfs_tag("/home/user/demo.al", "demo");
}

/* ── mkdir ─────────────────────────────────────────────────────────── */
int vfs_mkdir(const char *path) {
    if (find_node(path) >= 0) return 0; /* already exists */
    int idx = alloc_node();
    if (idx < 0) return -1;
    kstrcpy(nodes[idx].path, path);
    /* Extract name */
    const char *slash = path + kstrlen(path);
    while (slash > path && *slash != '/') slash--;
    kstrcpy(nodes[idx].name, slash + 1);
    nodes[idx].is_dir   = true;
    nodes[idx].created  = timer_seconds();
    nodes[idx].modified = timer_seconds();
    node_count++;
    return idx;
}

/* ── create file ───────────────────────────────────────────────────── */
int vfs_create(const char *path) {
    if (find_node(path) >= 0) return find_node(path);
    int idx = alloc_node();
    if (idx < 0) return -1;
    kstrcpy(nodes[idx].path, path);
    const char *slash = path + kstrlen(path);
    while (slash > path && *slash != '/') slash--;
    kstrcpy(nodes[idx].name, slash + 1);
    nodes[idx].is_dir   = false;
    nodes[idx].created  = timer_seconds();
    nodes[idx].modified = timer_seconds();
    node_count++;
    return idx;
}

/* ── write ─────────────────────────────────────────────────────────── */
int vfs_write(const char *path, const void *data, uint32_t size) {
    int idx = find_node(path);
    if (idx < 0) idx = vfs_create(path);
    if (idx < 0 || nodes[idx].is_dir) return -1;

    if (!size) size = (uint32_t)kstrlen((const char *)data);
    if (size > VFS_MAX_DATA) size = VFS_MAX_DATA;

    // Time-Travel: Create a new version instead of overwriting
    if (version_pool_count >= (VFS_MAX_NODES * VFS_MAX_VERSIONS_PER_FILE)) return -1; // Pool full

    vfs_version_t* new_version = &version_pool[version_pool_count++];
    vfs_data_block_t* new_block = &data_block_pool[idx]; // Simplified: 1 block per file node

    kmemcpy(new_block->data, data, size);
    new_block->size = size;

    new_version->timestamp = timer_seconds();
    new_version->block = new_block;
    new_version->prev_version = nodes[idx].versions; // Link to old version
    nodes[idx].versions = new_version;
    nodes[idx].modified = timer_seconds();
    nodes[idx].size = size; // Update node size to latest version's size
    return 0;
}

/* ── read ──────────────────────────────────────────────────────────── */
int vfs_read(const char *path, void *buf, uint32_t *size) {
    int idx = find_node(path);
    if (idx < 0) return -1;
    if (nodes[idx].is_dir || !nodes[idx].versions) return -1; // Not a file or no versions

    vfs_version_t* latest_version = nodes[idx].versions;
    uint32_t n = latest_version->block->size;
    if (size && *size < n) n = *size;
    kmemcpy(buf, latest_version->block->data, n);
    if (size) *size = n;
    return 0;
}

/* ── delete ────────────────────────────────────────────────────────── */
int vfs_delete(const char *path) {
    int idx = find_node(path);
    if (idx < 0) return -1;
    nodes[idx].used = false;
    node_count--;
    return 0;
}

/* ── find ──────────────────────────────────────────────────────────── */
int vfs_find(const char *path) { return find_node(path); }

/* ── ls ────────────────────────────────────────────────────────────── */
void vfs_ls(const char *path) {
    int found = 0;
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used) continue;
        if (kstrcmp(nodes[i].path, path) == 0) continue; /* skip self */
        /* Check if direct child */
        const char *p = nodes[i].path;
        size_t plen = kstrlen(path);
        if (kstrncmp(p, path, plen) != 0) continue;
        const char *rest = p + plen;
        if (*rest == '/' && kstrcmp(path, "/") != 0) rest++;
        else if (kstrcmp(path, "/") == 0 && *rest == '/') rest++;
        else continue;
        if (kstrchr(rest, '/')) continue; /* not direct child */
        /* Print */
        if (nodes[i].is_dir) {
            term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
            term_printf("  [DIR]  %s\n", nodes[i].name);
        } else {
            term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
            term_printf("  [FILE] %s  (%u bytes)", nodes[i].name, nodes[i].size);
            if (nodes[i].tag_count > 0) {
                term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
                term_write("  tags:");
                for (int t = 0; t < nodes[i].tag_count; t++)
                    term_printf(" #%s", nodes[i].tags[t]);
            }
            term_putchar('\n');
        }
        term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
        found++;
    }
    if (!found) term_writeln("  (empty)");
}

/* ── tag ───────────────────────────────────────────────────────────── */
void vfs_tag(const char *path, const char *tag) {
    int idx = find_node(path);
    if (idx < 0) return;
    if (nodes[idx].tag_count >= VFS_MAX_TAGS) return;
    /* Avoid duplicates */
    for (int t = 0; t < nodes[idx].tag_count; t++)
        if (kstrcmp(nodes[idx].tags[t], tag) == 0) return;
    kstrcpy(nodes[idx].tags[nodes[idx].tag_count++], tag);
}

/* ── set activity ──────────────────────────────────────────────────── */
void vfs_set_activity(const char *path, const char *activity) {
    int idx = find_node(path);
    if (idx >= 0) kstrcpy(nodes[idx].activity, activity);
}

/* ── semantic find ─────────────────────────────────────────────────── */
void vfs_semantic_find(const char *query) {
    term_setcolor(VGA_COLOR(VGA_LIGHT_CYAN, VGA_BLACK));
    term_printf("  Semantic search: \"%s\"\n", query);
    term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
    int found = 0;
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used || nodes[i].is_dir) continue;
        bool match = false;
        /* Match against name */
        if (kstrstr(nodes[i].name, query)) match = true;
        /* Match against tags */
        for (int t = 0; t < nodes[i].tag_count && !match; t++)
            if (kstrstr(nodes[i].tags[t], query)) match = true;
        /* Match against activity */
        if (kstrstr(nodes[i].activity, query)) match = true;
        /* Match against description */
        if (kstrstr(nodes[i].description, query)) match = true;
        /* Match against file content (first 256 bytes) */
        if (!match && nodes[i].size > 0) {
            if (nodes[i].versions && nodes[i].versions->block) {
                char tmp[257]; uint32_t n = nodes[i].versions->block->size < 256 ? nodes[i].versions->block->size : 256;
                kmemcpy(tmp, nodes[i].versions->block->data, n); tmp[n] = '\0';
                if (kstrstr(tmp, query)) match = true;
            }
        }
        if (match) {
            term_printf("  %-40s  [%s]", nodes[i].path,
                        nodes[i].activity[0] ? nodes[i].activity : "general");
            if (nodes[i].tag_count > 0) {
                term_setcolor(VGA_COLOR(VGA_LIGHT_GREEN, VGA_BLACK));
                for (int t = 0; t < nodes[i].tag_count; t++)
                    term_printf(" #%s", nodes[i].tags[t]);
                term_setcolor(VGA_COLOR(VGA_LIGHT_GREY, VGA_BLACK));
            }
            term_putchar('\n');
            found++;
        }
    }
    if (!found) term_writeln("  No files found matching that query.");
    else term_printf("  %d file(s) found.\n", found);
}

/* ── activity files ────────────────────────────────────────────────── */
void vfs_activity_files(const char *activity) {
    int found = 0;
    for (int i = 0; i < VFS_MAX_NODES; i++) {
        if (!nodes[i].used || nodes[i].is_dir) continue;
        if (kstrcmp(nodes[i].activity, activity) == 0) {
            term_printf("  %s  (%u bytes)\n", nodes[i].path, nodes[i].size);
            found++;
        }
    }
    if (!found) term_writeln("  No files associated with this activity.");
}
