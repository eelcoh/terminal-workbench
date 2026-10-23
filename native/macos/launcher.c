// Finder entry point of Band.app: run the bundled runtime, which opens a
// window on the saved sessions. Closing the window detaches; shells remain.
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int failure(const char *message) {
    fprintf(stderr, "Band: %s\n", message);
    pid_t child = fork();
    if (child == 0) {
        execl("/usr/bin/osascript", "osascript", "-e",
            "on run argv\n display alert \"Band could not start\" message (item 1 of argv) as critical\nend run",
            message, (char *)NULL);
        _exit(1);
    }
    if (child > 0) waitpid(child, NULL, 0);
    return 1;
}

int main(int argc, char **argv) {
    char executable[PATH_MAX], resolved[PATH_MAX];
    uint32_t size = sizeof(executable);
    if (_NSGetExecutablePath(executable, &size) != 0 || !realpath(executable, resolved))
        return failure("Cannot locate the application.");
    char *last = strrchr(resolved, '/');
    if (!last) return failure("Invalid application location.");
    *last = '\0';
    // Finder may pass a process serial number; nothing else is accepted.
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "-psn_", 5) != 0) {
            fprintf(stderr, "usage: open -a Band\n");
            return 2;
        }
    }
    char payload[PATH_MAX], runtime[PATH_MAX], viewer[PATH_MAX];
    if (snprintf(payload, sizeof(payload), "%s/../Resources/band", resolved) >= (int)sizeof(payload) ||
        snprintf(runtime, sizeof(runtime), "%s/bin/band-session", payload) >= (int)sizeof(runtime) ||
        snprintf(viewer, sizeof(viewer), "%s/band-ui", resolved) >= (int)sizeof(viewer))
        return failure("Application path is too long.");
    execl(runtime, runtime, "open", "--payload", payload, "--viewer", viewer, "--desktop", (char *)NULL);
    return failure("The bundled runtime is missing or not executable. Reinstall Band.");
}
