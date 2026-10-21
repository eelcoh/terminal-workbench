// Test the actual notification extension with Node's native TypeScript loader.
import { createServer } from 'node:net';
import { mkdtemp, rm, writeFile } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import assert from 'node:assert/strict';
import extension from '../integrations/pi/workbench.ts';
const directory=await mkdtemp(join(tmpdir(),'wb-pi-observer-'));
const control=join(directory,'terminal.ctl');const packets=[];const handlers=new Map();
const server=createServer(socket => {
    let bytes=Buffer.alloc(0);
    socket.on('data',part => {bytes=Buffer.concat([bytes,part]);if(bytes.length>=35){packets.push(bytes);socket.end('OK\n');}});
});
const wait=async condition => {for(let i=0;i<100;i++){if(condition())return;await new Promise(resolve=>setTimeout(resolve,5));}assert.fail('observer did not deliver expected notification');};
try {
    process.env.WORKBENCH_SESSION_SOCKET=join(directory,'terminal.sock');
    process.env.WORKBENCH_AGENT_PID=String(process.pid);process.env.WORKBENCH_AGENT_GROUP='17';process.env.WORKBENCH_AGENT_RUN='1234567890abcdef1234567890abcdef';
    extension({on:(name,callback)=>handlers.set(name,callback)});
    // Factory registration cannot require a socket: create the endpoint afterward.
    await new Promise(resolve=>server.listen(control,resolve));
    assert.deepEqual([...handlers.keys()],['agent_start','agent_settled','session_shutdown']);
    let previous=0n;
    for(const [event,state] of [['agent_start',1],['agent_settled',3],['session_shutdown',0]]){
        const count=packets.length;assert.equal(handlers.get(event)({privatePrompt:'not transmitted'}),undefined);
        await wait(()=>packets.length===count+1);const packet=packets.at(-1);
        assert.equal(packet.length,35);assert.equal(packet.subarray(0,5).toString(),'STATE');assert.equal(packet[5],7);assert.equal(packet[6],state);
        assert.equal(packet.readInt32BE(7),17);assert.equal(packet.subarray(11,27).toString('hex'),process.env.WORKBENCH_AGENT_RUN);
        assert(packet.readBigUInt64BE(27)>previous);previous=packet.readBigUInt64BE(27);
        assert(!packet.includes(Buffer.from('not transmitted')));
    }
    process.env.WORKBENCH_AGENT_PID=String(process.pid+1);handlers.get('agent_start')();
    await new Promise(resolve=>setTimeout(resolve,30));assert.equal(packets.length,3,'descendant cannot reuse reporting identity');
    process.env.WORKBENCH_AGENT_PID=String(process.pid);process.env.WORKBENCH_AGENT_RUN='invalid';handlers.get('agent_start')();assert.equal(packets.length,3);
    process.env.WORKBENCH_AGENT_RUN='1234567890abcdef1234567890abcdef';
    process.env.WORKBENCH_SESSION_SOCKET=join(directory,'regular.sock');await writeFile(join(directory,'regular.ctl'),'preserved');handlers.get('agent_start')();
    await new Promise(resolve=>setTimeout(resolve,30));assert.equal(packets.length,3,'unsafe endpoint cannot receive status');
    console.log('PASS: actual Pi observer notifications, monotonic scoped binary reports, no prompt payloads, child guard, unsafe endpoints and no factory I/O');
} finally {
    await new Promise(resolve=>server.close(resolve));await rm(directory,{recursive:true,force:true});
}
