// Invocation-local, notification-only Pi lifecycle observer. No prompt payloads.
import { createConnection } from "node:net";
import { lstatSync } from "node:fs";
export default function (pi: any) {
    let sequence = 0n;
    function report(state: number) {
        const env = process.env;
        // A descendant Pi cannot reuse its parent's reporting identity.
        if (env.WORKBENCH_AGENT_PID !== String(process.pid)) return;
        const group = Number(env.WORKBENCH_AGENT_GROUP);
        const token = env.WORKBENCH_AGENT_RUN || "";
        const path = (env.WORKBENCH_SESSION_SOCKET || "").replace(/\.sock$/, ".ctl");
        if (!Number.isInteger(group) || group <= 0 || group > 2147483647 || !/^[0-9a-f]{32}$/.test(token) || !path.endsWith(".ctl")) return;
        try {
            const file = lstatSync(path);
            if (!file.isSocket() || (process.getuid && file.uid !== process.getuid())) return;
            const packet = Buffer.alloc(35);packet.write("STATE");packet[5] = 7;packet[6] = state;
            packet.writeInt32BE(group,7);Buffer.from(token,"hex").copy(packet,11);
            const now = process.hrtime.bigint();sequence = now > sequence ? now : sequence + 1n;
            packet.writeBigUInt64BE(sequence,27);
            const socket = createConnection(path);
            socket.setTimeout(400,() => socket.destroy());socket.on("error",() => socket.destroy());
            socket.on("connect",() => socket.end(packet));socket.on("data",() => socket.destroy());
            socket.unref();
        } catch { /* Reporting failure cannot alter Pi's behavior. */ }
    }
    pi.on("agent_start",() => { report(1); });
    // agent_end can precede retries/continuations. Settled is final idle.
    pi.on("agent_settled",() => { report(3); });
    pi.on("session_shutdown",() => { report(0); });
}
