// Finder entry point for a same-machine preview. Opening a window runs the
// bundled runtime binary; preview management arguments still use Python.
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int failure(const char *message) {
    fprintf(stderr, "Terminal Workbench Preview: %s\n", message);
    pid_t child = fork();
    if (child == 0) {
        execl("/usr/bin/osascript", "osascript", "-e",
            "on run argv\n display alert \"Terminal Workbench could not start\" message (item 1 of argv) as critical\nend run",
            message, (char *)NULL);
        _exit(1);
    }
    if (child > 0) waitpid(child, NULL, 0);
    return 1;
}

int main(int argc, char **argv) {
    char executable[PATH_MAX], resolved[PATH_MAX], root[PATH_MAX], path[PATH_MAX];
    uint32_t size = sizeof(executable);
    if (_NSGetExecutablePath(executable, &size) != 0 || !realpath(executable, resolved))
        return failure("Cannot locate the preview application. Rebuild with just toolkit-stage.");
    char *last = strrchr(resolved, '/');
    if (!last) return failure("Invalid application location.");
    *last = '\0';
    if (snprintf(root, sizeof(root), "%s/../Resources/workbench", resolved) >= (int)sizeof(root))
        return failure("Application path is too long.");
    int extra = 0;
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "-psn_", 5) != 0) ++extra;
    }
    if (extra == 0) {
        char runtime[PATH_MAX], viewer[PATH_MAX];
        if (snprintf(runtime, sizeof(runtime), "%s/libexec/band-session", root) >= (int)sizeof(runtime) ||
            snprintf(viewer, sizeof(viewer), "%s/band-ui", resolved) >= (int)sizeof(viewer))
            return failure("Application path is too long.");
        execl(runtime, runtime, "open", "--payload", root, "--viewer", viewer, "--desktop", (char *)NULL);
        return failure("The preview runtime is missing or not executable. Rebuild with just toolkit-stage.");
    }
    if (snprintf(path, sizeof(path), "%s/python.path", root) >= (int)sizeof(path))
        return failure("Application path is too long.");
    FILE *file = fopen(path, "r");
    if (!file) return failure("The preview is incomplete. Rebuild with just toolkit-stage.");
    char python[PATH_MAX];
    int valid = fgets(python, sizeof(python), file) != NULL;
    if (valid) {
        char *newline = strchr(python, '\n');
        valid = newline != NULL && fgetc(file) == EOF && python[0] == '/';
        if (newline) *newline = '\0';
    }
    fclose(file);
    if (!valid) return failure("The recorded Python path is invalid. Rebuild with just toolkit-stage.");
    char script[PATH_MAX], runtime[PATH_MAX], viewer[PATH_MAX];
    if (snprintf(script, sizeof(script), "%s/scripts/toolkit-ui.py", root) >= (int)sizeof(script) ||
        snprintf(runtime, sizeof(runtime), "%s/libexec/band-session", root) >= (int)sizeof(runtime) ||
        snprintf(viewer, sizeof(viewer), "%s/band-ui", resolved) >= (int)sizeof(viewer))
        return failure("Application path is too long.");
    char **arguments = calloc((size_t)argc + 10, sizeof(char *));
    if (!arguments) return failure("Cannot allocate launcher arguments.");
    int count = 0;
    arguments[count++] = python; arguments[count++] = "-B"; arguments[count++] = script;
    arguments[count++] = runtime; arguments[count++] = viewer;
    arguments[count++] = "--saved"; arguments[count++] = "--installed"; arguments[count++] = "--desktop";
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "-psn_", 5) != 0) arguments[count++] = argv[i];
    }
    setenv("PYTHONDONTWRITEBYTECODE", "1", 1);
    execv(python, arguments);
    free(arguments);
    return failure("Could not run the Python installation used to build this preview. Rebuild with just toolkit-stage. Once Python starts, launch details are in ~/.local/state/terminal-workbench/qt-preview/launch.log (or your selected state directory).");
}
