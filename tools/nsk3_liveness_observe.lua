-- Own packet-supervision counter and policy boundary; no heartbeat injection.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_long_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
machine.debugger:command('bpset 2ab64a,1,{logerror "nsk3_liveness_window: counter=%04x pointer=%08x cycles=%016x observer_writes=0\\n",w@(r5+2),r5,totalcycles;g}')
machine.debugger:command('bpset 2a2a4a,r0==1,{logerror "nsk3_liveness_policy: argument=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
machine.debugger:command('bpset 2823c4,r1>=100000 && r1<10ff00,{logerror "nsk3_packet_forward: target=%02x pointer=%08x header=%08x cycles=%016x observer_writes=0\\n",r0,r1,d@r1,totalcycles;g}')
machine.debugger:command('bpset 266eb0,d@(r13+4)>=100000 && d@(r13+4)<10ff00 && w@(d@(r13+4))==13b8,{logerror "nsk3_packet_receive: task=%02x pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",b@(r5+2),d@(r13+4),d@(d@(r13+4)),d@(r13+28),totalcycles;g}')
machine.debugger:command('bpset 21a2ea,r5>=100000 && r5<10ff00,{logerror "nsk3_packet_consumer: pointer=%08x header=%08x owner=%08x state=%02x phase=%02x cache=%08x cycles=%016x observer_writes=0\\n",r5,d@r5,r6,b@(r6+14),b@(r6+19),d@(r4+28),totalcycles;g}')
local writes=0
machine.debugger:command('bpset 287a38,r0==43f,{logerror "nsk3_043f_construct: caller=%08x cycles=%016x observer_writes=0\\n",r14,totalcycles;g}')
machine.debugger:command('bpset 21a674,1,{logerror "nsk3_043f_consumer: pointer=%08x header=%08x cycles=%016x observer_writes=0\\n",r5,d@r5,totalcycles;g}')
machine.debugger:command('bpset 254476,1,{logerror "nsk3_043f_predecessor: cycles=%016x observer_writes=0\\n",totalcycles;g}')
machine.debugger:command('wpset 106cfc,1,w,1,{logerror "nsk3_owner_state_write: address=00106cfc data=%08x pc=%08x caller=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,r14,totalcycles;g}')
machine.debugger:command('wpset 106d01,1,w,1,{logerror "nsk3_owner_phase_write: address=00106d01 data=%08x pc=%08x caller=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,r14,totalcycles;g}')
machine.debugger:command('bpset 21a32e,b@(r6+14)==3 && b@(r6+19)==1 && d@(r4+28)>=100000 && d@(r4+28)<10ff00,{logerror "nsk3_packet_cache: pointer=%08x source=%08x data=%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x%08x cycles=%016x observer_writes=0\\n",d@(r4+28),r5,d@(d@(r4+28)+6),d@(d@(r4+28)+a),d@(d@(r4+28)+e),d@(d@(r4+28)+12),d@(d@(r4+28)+16),d@(d@(r4+28)+1a),d@(d@(r4+28)+1e),d@(d@(r4+28)+22),d@(d@(r4+28)+26),d@(d@(r4+28)+2a),d@(d@(r4+28)+2e),d@(d@(r4+28)+32),d@(d@(r4+28)+36),d@(d@(r4+28)+3a),d@(d@(r4+28)+3e),d@(d@(r4+28)+42),d@(d@(r4+28)+46),d@(d@(r4+28)+4a),d@(d@(r4+28)+4e),d@(d@(r4+28)+52),d@(d@(r4+28)+56),d@(d@(r4+28)+5a),d@(d@(r4+28)+5e),d@(d@(r4+28)+62),d@(d@(r4+28)+66),d@(d@(r4+28)+6a),d@(d@(r4+28)+6e),d@(d@(r4+28)+72),d@(d@(r4+28)+76),d@(d@(r4+28)+7a),d@(d@(r4+28)+7e),d@(d@(r4+28)+82),d@(d@(r4+28)+86),d@(d@(r4+28)+8a),d@(d@(r4+28)+8e),d@(d@(r4+28)+92),d@(d@(r4+28)+96),d@(d@(r4+28)+9a),d@(d@(r4+28)+9e),d@(d@(r4+28)+a2),totalcycles;g}')
machine.debugger:command('bpset 21a35c,1,{logerror "nsk3_packet_park: state=%02x phase=%02x cache=%08x cycles=%016x observer_writes=0\\n",b@(r6+14),b@(r6+19),d@(r4+28),totalcycles;g}')
_G.nsk3_liveness_counter_tap=memory:install_write_tap(0x10c5a4,0x10c5a7,'nsk3_liveness_counter',function(offset,value,mask)
    if mask & 0xffff==0 then return end
    writes=writes+1
    if writes<=64 then machine:logerror(string.format('nsk3_liveness_counter: value=%04x mask=%08x pc=%08x t=%.9f observer_writes=0\n',value&0xffff,mask,cpu.state['PC'].value,machine.time:as_double())) end
    if cpu.state['PC'].value==0x2ab512 and writes<=64 then
        local pointer=cpu.state['R4'].value
        if pointer>=0x100000 and pointer<=0x10ff00 then
            local length=memory:read_u8(pointer+2)+4
            if pointer+length>0x110000 then return end
            local data={}
            for i=0,length-1 do data[#data+1]=string.format('%02x',memory:read_u8(pointer+i)) end
            machine:logerror(string.format('nsk3_liveness_packet: pointer=%08x data=%s t=%.9f observer_writes=0\n',pointer,table.concat(data),machine.time:as_double()))
        end
    end
end)
