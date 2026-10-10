/* Workbench's experimental Unix transport for the pinned Ghostling viewer.
 * Rendering and VT encoding remain upstream's responsibility. */
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

/* Each pane owns transport state. Binding is confined to the UI thread. */
typedef struct {
    bool takeover, takeover_unsupported, replaying, failed, snapshot_attempt, snapshot_seen, snapshot_done, attached_elsewhere;
    char session_path[sizeof(((struct sockaddr_un *)0)->sun_path)];
    uint8_t *snapshot;
    size_t snapshot_size, snapshot_used;
    double attach_started;
    uint16_t cols, rows, wanted_cols, wanted_rows;
    uint32_t cell_width, cell_height;
    uint8_t send_buffer[1024 * 1024], receive_buffer[65541];
    size_t send_length, receive_length;
    uint64_t output_revision;
    int agent_fd;
    uint8_t agent, attention, agent_buffer[7];
    bool agent_fallback, agent_v2_received;
    char agent_control[sizeof(((struct sockaddr_un *)0)->sun_path)];
    size_t agent_used;
} RemoteSession;
static bool remote_restore_terminal(GhosttyTerminal *terminal, const uint8_t *bytes, size_t length, void *effects);
static RemoteSession single_remote = { .replaying = true };
static RemoteSession *active_remote = &single_remote;
#define remote_replaying (active_remote->replaying)
#define remote_failed (active_remote->failed)
#define remote_cols (active_remote->cols)
#define remote_rows (active_remote->rows)
#define desired_cols (active_remote->wanted_cols)
#define desired_rows (active_remote->wanted_rows)
#define remote_cell_width (active_remote->cell_width)
#define remote_cell_height (active_remote->cell_height)
#define send_queue (active_remote->send_buffer)
#define receive_queue (active_remote->receive_buffer)
#define send_used (active_remote->send_length)
#define receive_used (active_remote->receive_length)

static void remote_fail(int fd, const char *reason)
{
    fprintf(stderr, "session connection: %s\n", reason);
    free(active_remote->snapshot); active_remote->snapshot = NULL;
    remote_failed = true;
    shutdown(fd, SHUT_RDWR);
}

static void remote_flush(int fd)
{
    while (send_used && !remote_failed) {
        ssize_t n = send(fd, send_queue, send_used, 0);
        if (n > 0) {
            send_used -= (size_t)n;
            memmove(send_queue, send_queue + n, send_used);
        } else if (n < 0 && errno == EINTR) {
            continue;
        } else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        } else {
            remote_fail(fd, "write failed");
        }
    }
}

static void remote_send(int fd, uint8_t kind, const void *data, size_t length)
{
    if (remote_failed) return;
    if (length > 65536 || send_used + length + 5 > sizeof(send_queue)) {
        remote_fail(fd, "input queue capacity reached");
        return;
    }
    uint8_t *header = send_queue + send_used;
    header[0] = kind;
    header[1] = (uint8_t)(length >> 24);
    header[2] = (uint8_t)(length >> 16);
    header[3] = (uint8_t)(length >> 8);
    header[4] = (uint8_t)length;
    if (length) memcpy(header + 5, data, length);
    send_used += length + 5;
    remote_flush(fd);
}

static void remote_resize(int fd, uint16_t cols, uint16_t rows)
{
    desired_cols = cols;
    desired_rows = rows;
    if (remote_replaying) return;
    uint8_t payload[4] = { (uint8_t)(cols >> 8), (uint8_t)cols,
                           (uint8_t)(rows >> 8), (uint8_t)rows };
    remote_send(fd, 17, payload, sizeof(payload));
}

static void remote_agent_open(void)
{
    RemoteSession *remote = active_remote;
    remote->agent = 255; remote->attention = 0; remote->agent_fd = -1; remote->agent_used = 0;
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    strcpy(address.sun_path, remote->agent_control);
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0
        || fcntl(fd, F_SETFD, FD_CLOEXEC) < 0
        || connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0
        || send(fd, remote->agent_fallback ? "WATCH" : "WAT2\n", 5, 0) != 5) { close(fd); return; }
    remote->agent_fd = fd;
}
static void remote_agent_connect(const char *path)
{
    RemoteSession *remote = active_remote;
    remote->agent = 255; remote->attention = 0; remote->agent_fd = -1;
    snprintf(remote->agent_control, sizeof(remote->agent_control), "%s", path);
    char *extension = strrchr(remote->agent_control, '.');
    if (!extension || strcmp(extension, ".sock")) return;
    strcpy(extension, ".ctl");
    remote_agent_open();
}
static void remote_agent_read(void)
{
    RemoteSession *remote = active_remote;
    if (remote->agent_fd < 0) return;
    for (int batch = 0; batch < 8; batch++) {
        size_t total = 5;
        if (remote->agent_used >= 5) {
            const uint8_t *bytes = remote->agent_buffer;
            if (bytes[1] || bytes[2] || bytes[3]
                || !((bytes[0] == 5 && bytes[4] == 1) || (bytes[0] == 6 && bytes[4] == 2))) break;
            total += bytes[4];
        }
        ssize_t n = recv(remote->agent_fd, remote->agent_buffer + remote->agent_used, total - remote->agent_used, 0);
        if (n > 0) {
            remote->agent_used += (size_t)n;
            if (total > 5 && remote->agent_used == total) {
                const uint8_t *bytes = remote->agent_buffer;
                remote->agent = bytes[5] <= 5 ? bytes[5] : 255;
                remote->attention = bytes[0] == 6 && bytes[6] <= 3 ? bytes[6] : 0;
                if (bytes[0] == 6) remote->agent_v2_received = true;
                remote->agent_used = 0;
            }
        } else if (n < 0 && errno == EINTR) continue;
        else if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) return;
        else break;
        if (batch == 7) return;
    }
    close(remote->agent_fd); remote->agent_fd = -1; remote->agent = 255; remote->attention = 0;
    if (!remote->agent_fallback && !remote->agent_v2_received) {
        remote->agent_fallback = true; remote_agent_open();
    }
}
static const char *remote_attention_name(uint8_t state)
{
    const char *names[] = {"Unknown", "Working", "Needs input", "Ready"};
    return state <= 3 ? names[state] : "Unknown";
}
static Color remote_attention_color(uint8_t state)
{
    if (state == 1) return ui.accent;
    if (state == 2) return ui.waiting;
    if (state == 3) return ui.ready;
    return ui.muted;
}

static const char *remote_agent_name(uint8_t agent)
{
    const char *names[] = {"", "Codex", "Claude", "Gemini", "OpenCode", "Aider"};
    return agent <= 5 ? names[agent] : "";
}

static int remote_connect(const char *path, uint16_t cols, uint16_t rows,
                          int cell_width, int cell_height)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    if (!path || strlen(path) >= sizeof(address.sun_path)) {
        fprintf(stderr, "set WORKBENCH_SOCKET to the running session's socket\n");
        return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    memcpy(active_remote->session_path, path, strlen(path) + 1);
    memcpy(address.sun_path, path, strlen(path) + 1);
    char *extension = strrchr(address.sun_path, '.');
    if (extension && !strcmp(extension, ".sock")) {
        strcpy(extension, ".ctl");
        active_remote->snapshot_attempt = true;
    }
    active_remote->attach_started = GetTime();
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        if (active_remote->takeover || !active_remote->snapshot_attempt) {
            fprintf(stderr, "Could not attach to session socket %s: %s\n", path, strerror(errno)); close(fd); return -1;
        }
        active_remote->snapshot_attempt = false;
        strcpy(address.sun_path, path);
        if (connect(fd, (struct sockaddr *)&address, sizeof(address)) < 0) { close(fd); return -1; }
    }
    if (active_remote->snapshot_attempt) {
        char hello[] = "ATS1\nf64f4aca2c29b554d111b36c3d946a9bddd159ff";
        if (active_remote->takeover) hello[2] = 'M';
        if (send(fd, hello, sizeof(hello)-1, 0) != sizeof(hello)-1) { close(fd); return -1; }
    }
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl"); close(fd); return -1;
    }
    signal(SIGPIPE, SIG_IGN);
    remote_cell_width = (uint32_t)cell_width;
    remote_cell_height = (uint32_t)cell_height;
    remote_resize(fd, cols, rows);
    remote_agent_connect(path);
    return fd;
}

static int pty_spawn(pid_t *child_out, uint16_t cols, uint16_t rows,
                     int cell_width, int cell_height)
{
    *child_out = -1; /* The session server owns the process. */
    return remote_connect(getenv("WORKBENCH_SOCKET"), cols, rows, cell_width, cell_height);
}

static void pty_write(int fd, const char *bytes, size_t length)
{
    /* Replaying terminal queries must not inject their replies into a live
     * shell a second time. User input also waits for the ready boundary. */
    if (remote_replaying) return;
    while (length) {
        size_t count = length > 65536 ? 65536 : length;
        remote_send(fd, 16, bytes, count);
        bytes += count;
        length -= count;
    }
}

typedef enum { PTY_READ_OK, PTY_READ_EOF, PTY_READ_ERROR } PtyReadResult;

/* Older control servers close unknown methods. Reconnect only before any
 * snapshot response; malformed/partial snapshots never silently fall back. */
static bool remote_legacy_fallback(int fd)
{
    struct sockaddr_un address = { .sun_family = AF_UNIX };
    strcpy(address.sun_path, active_remote->session_path);
    int replacement = socket(AF_UNIX, SOCK_STREAM, 0);
    if (replacement < 0) return false;
    bool ok = connect(replacement, (struct sockaddr *)&address, sizeof(address)) == 0
        && dup2(replacement, fd) >= 0;
    close(replacement);
    if (!ok) return false;
    int flags = fcntl(fd, F_GETFL);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) return false;
    active_remote->snapshot_attempt = false;
    active_remote->receive_length = 0;
    fprintf(stderr, "session attachment: legacy replay fallback\n");
    return true;
}
/* Pre-transfer runtimes may still own live shells. Retry their ordinary
 * snapshot attachment without taking ownership: rejection preserves the old
 * window; closing it first permits reconnect without restarting the shell. */
static bool remote_takeover_fallback(int fd)
{
    RemoteSession *remote=active_remote;
    if (!remote->takeover) return false;
    struct sockaddr_un address={.sun_family=AF_UNIX};
    strcpy(address.sun_path,remote->session_path);
    char *extension=strrchr(address.sun_path,'.');
    if (!extension || strcmp(extension,".sock")) return false;
    strcpy(extension,".ctl");
    int replacement=socket(AF_UNIX,SOCK_STREAM,0);
    if (replacement<0) return false;
    const char hello[]="ATS1\nf64f4aca2c29b554d111b36c3d946a9bddd159ff";
    bool ok=connect(replacement,(struct sockaddr *)&address,sizeof(address))==0
        && send(replacement,hello,sizeof(hello)-1,0)==sizeof(hello)-1
        && dup2(replacement,fd)>=0;
    close(replacement);
    if (!ok) return false;
    int flags=fcntl(fd,F_GETFL);
    if (flags<0 || fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0) return false;
    remote->takeover=false;
    remote->takeover_unsupported=true;
    remote->receive_length=0;
    remote->attach_started=GetTime();
    fprintf(stderr,"Earlier runtime: retrying ordinary attachment without revoking its owner.\n");
    return true;
}

static PtyReadResult pty_read(int fd, GhosttyTerminal *terminal, void *effects)
{
    remote_agent_read();
    remote_flush(fd);
    if (remote_failed) return PTY_READ_ERROR;
    if (remote_replaying && GetTime() - active_remote->attach_started > 10.0) {
        remote_fail(fd, "attachment timed out"); return PTY_READ_ERROR;
    }
    /* Bound work per frame, including when a large replay is arriving. */
    for (int batch = 0; batch < 64; batch++) {
        if (receive_used >= 5) {
            size_t length = ((size_t)receive_queue[1] << 24)
                          | ((size_t)receive_queue[2] << 16)
                          | ((size_t)receive_queue[3] << 8)
                          | receive_queue[4];
            if (length > 65536) {
                remote_fail(fd, "invalid output frame size"); return PTY_READ_ERROR;
            }
            if (receive_used >= length + 5) {
                uint8_t kind = receive_queue[0];
                const uint8_t *payload = receive_queue + 5;
                RemoteSession *remote = active_remote;
                if (remote->snapshot_attempt) remote->snapshot_seen = true;
                if (kind == 7 && length == 4 && remote->snapshot_attempt && !remote->snapshot && !remote->snapshot_done) {
                    remote->snapshot_size = ((size_t)payload[0]<<24) | ((size_t)payload[1]<<16) | ((size_t)payload[2]<<8) | payload[3];
                    if (!remote->snapshot_size || remote->snapshot_size > 64*1024*1024) {
                        remote_fail(fd, "invalid snapshot size"); return PTY_READ_ERROR;
                    }
                    remote->snapshot = malloc(remote->snapshot_size);
                    if (!remote->snapshot) { remote_fail(fd, "snapshot allocation failed"); return PTY_READ_ERROR; }
                } else if (kind == 8 && length && remote->snapshot && length <= remote->snapshot_size-remote->snapshot_used) {
                    memcpy(remote->snapshot+remote->snapshot_used,payload,length);
                    remote->snapshot_used += length;
                } else if (kind == 9 && !length && remote->snapshot && remote->snapshot_used == remote->snapshot_size) {
                    if (!remote_restore_terminal(terminal,remote->snapshot,remote->snapshot_size,effects)) {
                        remote_fail(fd, "invalid terminal snapshot"); return PTY_READ_ERROR;
                    }
                    free(remote->snapshot); remote->snapshot = NULL; remote->snapshot_done = true;
                    ghostty_terminal_get(*terminal,GHOSTTY_TERMINAL_DATA_COLS,&remote->cols);
                    ghostty_terminal_get(*terminal,GHOSTTY_TERMINAL_DATA_ROWS,&remote->rows);
                    remote->output_revision++;
                } else if (remote->snapshot_attempt && !remote->snapshot_done && kind != 4) {
                    remote_fail(fd, "incomplete snapshot attachment"); return PTY_READ_ERROR;
                } else if (kind == 1) {
                    ghostty_terminal_vt_write(*terminal, payload, length);
                    if (!remote_replaying) active_remote->output_revision++;
                } else if (kind == 2 && length == 4) {
                    remote_cols = ((uint16_t)payload[0] << 8) | payload[1];
                    remote_rows = ((uint16_t)payload[2] << 8) | payload[3];
                    ghostty_terminal_resize(*terminal, remote_cols, remote_rows,
                                            remote_cell_width, remote_cell_height);
                } else if (kind == 3 && length == 0) {
                    remote_replaying = false;
                    remote_resize(fd, desired_cols, desired_rows);
                } else if (kind == 10 && !length) {
                    remote->attached_elsewhere = true;
                    remote_fail(fd,"terminal moved to another window");
                    return PTY_READ_ERROR;
                } else if (kind == 4) {
                    static const char busy[] = "a window is already attached";
                    remote->attached_elsewhere = length == sizeof(busy)-1 && !memcmp(payload,busy,length);
                    fprintf(stderr, "session rejected attachment: %.*s\n", (int)length, payload);
                    if (remote->attached_elsewhere)
                        fprintf(stderr, "This terminal is open in another window. Use Move here to transfer the live terminal to this window.\n");
                    remote_fail(fd,"attachment rejected");
                    return PTY_READ_ERROR;
                } else {
                    remote_fail(fd, "invalid output frame"); return PTY_READ_ERROR;
                }
                receive_used -= length + 5;
                memmove(receive_queue, receive_queue + length + 5, receive_used);
                continue;
            }
        }
        ssize_t n = recv(fd, receive_queue + receive_used,
                         sizeof(receive_queue) - receive_used, 0);
        if (n > 0) { receive_used += (size_t)n; continue; }
        if (n == 0) {
            if (active_remote->snapshot_attempt && !active_remote->snapshot_seen && !receive_used
                && (remote_takeover_fallback(fd) || remote_legacy_fallback(fd))) continue;
            remote_fail(fd, "session stream closed"); return PTY_READ_EOF;
        }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) return PTY_READ_OK;
        if (errno == ECONNRESET && active_remote->snapshot_attempt && !active_remote->snapshot_seen
            && !receive_used && (remote_takeover_fallback(fd) || remote_legacy_fallback(fd))) continue;
        remote_fail(fd, "session read failed"); return PTY_READ_ERROR;
    }
    return PTY_READ_OK;
}
