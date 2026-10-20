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
machine.debugger:command('wpset 10a82c,4,w,1,{logerror "nsk3_retained_buffer_write: pointer=%08x pc=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,totalcycles;g}')
machine.debugger:command('bpset 208a48,r0==7d3 || r0==7d4,{logerror "nsk3_queue_construct: input=%04x argument=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,r14,totalcycles;g}')
machine.debugger:command('wpset 10a404,4,w,1,{logerror "nsk3_server_queue_write: data=%08x pc=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,totalcycles;g}')
machine.debugger:command('bpset 287f70,1,{logerror "nsk3_service_mode: mode=%02x queue=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,d@10a404,r14,totalcycles;g}')
machine.debugger:command('bpset 287da4,1,{logerror "nsk3_server_dynamic_construct: input=%04x object=%08x field2=%02x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,r2,r14,totalcycles;g}')
machine.debugger:command('bpset 249472,1,{logerror "nsk3_completion_identity: task=%02x cycles=%016x observer_writes=0\\n",b@100022,totalcycles;g}')
machine.debugger:command('bpset 24709c,1,{logerror "nsk3_completion_construct: input=%04x owner=%08x result=%02x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,b@r1,r14,totalcycles;g}')
machine.debugger:command('bpset 24753e,1,{logerror "nsk3_completion_wait: cycles=%016x observer_writes=0\\n",totalcycles;g}')
machine.debugger:command('bpset 247542,1,{logerror "nsk3_completion_receive: object=%08x header=%08x cycles=%016x observer_writes=0\\n",r0,d@r0,totalcycles;g}')
machine.debugger:command('bpset 266a2c,r0==11 && r1>=100000 && r1<300000 && w@r1==5e6,{logerror "nsk3_completion_post: pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",r1,d@r1,r14,totalcycles;g}')
machine.debugger:command('wpset 10adf1,1,w,1,{logerror "nsk3_context_flag_write: data=%08x pc=%08x caller=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,r14,totalcycles;g}')
machine.debugger:command('bpset 21d676,1,{logerror "nsk3_controller_threshold: owner=%08x value=%04x threshold=%04x cycles=%016x observer_writes=0\\n",r4,r1,r0,totalcycles;g}')
machine.debugger:command('wpset 10ae00,2,w,1,{logerror "nsk3_controller_threshold_write: data=%08x pc=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,totalcycles;g}')
machine.debugger:command('bpset 21d714,1,{logerror "nsk3_mode_trigger: status=%04x owner=%08x caller=%08x cycles=%016x observer_writes=0\\n",r5,r4,r14,totalcycles;g}')
machine.debugger:command('bpset 21ab96,1,{logerror "nsk3_controller_helper_return: status=%04x caller=%08x cycles=%016x observer_writes=0\\n",r5,d@(r13+10),totalcycles;g}')
for _,address in ipairs({'21c234','21c23e','21c3f4','21c3fe','21ca4c'}) do
    machine.debugger:command('bpset '..address..',1,{logerror "nsk3_mode_callsite: pc=%08x status=%04x cycles=%016x observer_writes=0\\n",pc,r5,totalcycles;g}')
end
machine.debugger:command('bpset 266a2c,r1>=100000 && r1<300000 && w@r1==a22,{logerror "nsk3_0a22_post: target=%02x pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,d@r1,r14,totalcycles;g}')
machine.debugger:command('bpset 21f36a,1,{logerror "nsk3_controller_mode_shared: owner=%08x status=%04x flag=%02x caller=%08x cycles=%016x observer_writes=0\\n",r4,r5,b@(r4+6),r14,totalcycles;g}')
machine.debugger:command('wpset 10ade6,1,w,1,{logerror "nsk3_controller_flag_write: data=%08x pc=%08x caller=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,r14,totalcycles;g}')
machine.debugger:command('bpset 21f358,1,{logerror "nsk3_controller_mode_entry: owner=%08x status=%04x flag=%02x cycles=%016x observer_writes=0\\n",r4,r5,b@(r4+6),totalcycles;g}')
machine.debugger:command('wpset 10ae3e,1,w,1,{logerror "nsk3_request_mode_write: data=%08x pc=%08x caller=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,r14,totalcycles;g}')
machine.debugger:command('bpset 21a850,1,{logerror "nsk3_request_mode_select: flag=%02x state=%02x source=%02x cycles=%016x observer_writes=0\\n",b@10adee,b@10ade4,b@10ae3e,totalcycles;g}')
machine.debugger:command('bpset 209a22,1,{logerror "nsk3_request_mode_copy: object=%08x header=%08x cycles=%016x observer_writes=0\\n",r4,d@r4,totalcycles;g}')
machine.debugger:command('wpset 10a7d9,1,w,1,{logerror "nsk3_controller_mode_write: data=%08x pc=%08x caller=%08x cycles=%016x observer_writes=0\\n",wpdata,pc,r14,totalcycles;g}')
machine.debugger:command('bpset 20e356,1,{logerror "nsk3_mode_decision: status=%04x flag=%08x owner=%08x mode=%02x cycles=%016x observer_writes=0\\n",d@r13,r7,r4,b@(r4+1),totalcycles;g}')
machine.debugger:command('bpset 208a48,r0==7d6,{logerror "nsk3_07d6_construct: caller=%08x argument=%08x cycles=%016x observer_writes=0\\n",r14,r1,totalcycles;g}')
machine.debugger:command('bpset 28818e,1,{logerror "nsk3_service_receive: object=%08x header=%08x cycles=%016x observer_writes=0\\n",r0,d@r0,totalcycles;g}')
machine.debugger:command('bpset 266a2c,r1>=100000 && r1<300000 && w@r1==7d6,{logerror "nsk3_07d6_post: target=%02x pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,d@r1,r14,totalcycles;g}')
machine.debugger:command('bpset 2882d4,1,{logerror "nsk3_service_result: value=%04x caller=%08x cycles=%016x observer_writes=0\\n",r6,d@(r13+10),totalcycles;g}')
machine.debugger:command('bpset 2880b6,1,{logerror "nsk3_07e5_translate: object=%08x header=%08x input=%04x cycles=%016x observer_writes=0\\n",r4,d@r4,r6,totalcycles;g}')
machine.debugger:command('bpset 266a2c,r1>=100000 && r1<300000 && w@r1==7e5,{logerror "nsk3_07e5_post: target=%02x pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,d@r1,r14,totalcycles;g}')
machine.debugger:command('bpset 287a38,r0==3e9,{logerror "nsk3_mode_construct: caller=%08x cycles=%016x observer_writes=0\\n",r14,totalcycles;g}')
machine.debugger:command('bpset 266a2c,r1>=100000 && r1<110000 && w@r1==3e9,{logerror "nsk3_mode_post: target=%02x pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r1,d@r1,r14,totalcycles;g}')
machine.debugger:command('bpset 2144b0,1,{logerror "nsk3_radio_argument2: object=%08x header=%08x cycles=%016x observer_writes=0\\n",r8,d@r8,totalcycles;g}')
machine.debugger:command('bpset 214496,1,{logerror "nsk3_radio_138f: object=%08x header=%08x cycles=%016x observer_writes=0\\n",r8,d@r8,totalcycles;g}')
machine.debugger:command('bpset 2144a0,1,{logerror "nsk3_radio_context: owner=%08x first=%08x second=%08x cycles=%016x observer_writes=0\\n",r0,d@(r0+30),d@(r0+20),totalcycles;g}')
machine.debugger:command('bpset 212c04,1,{logerror "nsk3_043c_entry: argument=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
machine.debugger:command('bpset 212c18,1,{logerror "nsk3_043c_predicate: record0605=%08x cached=%04x argument=%08x cycles=%016x observer_writes=0\\n",r0,w@10feaa,r11,totalcycles;g}')
machine.debugger:command('bpset 212c8e,1,{logerror "nsk3_043c_candidate_a: r4=%08x r6=%08x cycles=%016x observer_writes=0\\n",r4,r6,totalcycles;g}')
machine.debugger:command('bpset 214018,1,{logerror "nsk3_043c_candidate_b: r4=%08x cycles=%016x observer_writes=0\\n",r4,totalcycles;g}')
machine.debugger:command('bpset 237bea,r0==2c6750,{logerror "nsk3_043c_send: caller=%08x pointer=%08x cycles=%016x observer_writes=0\\n",r14,r0,totalcycles;g}')
machine.debugger:command('bpset 21c86c,1,{logerror "nsk3_controller_wait: owner=%08x selector=%04x status=%04x cycles=%016x observer_writes=0\\n",r4,w@r7,r5,totalcycles;g}')
machine.debugger:command('bpset 21f394,1,{logerror "nsk3_controller_result: value=%04x cycles=%016x observer_writes=0\\n",r0,totalcycles;g}')
machine.debugger:command('bpset 21ca46,1,{logerror "nsk3_controller_dispatch: status=%04x cycles=%016x observer_writes=0\\n",r5,totalcycles;g}')
machine.debugger:command('bpset 21cf96,1,{logerror "nsk3_controller_tail: value=%04x status=%04x cycles=%016x observer_writes=0\\n",r0,r5,totalcycles;g}')
machine.debugger:command('bpset 21a782,1,{logerror "nsk3_controller_construct: input=%04x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
machine.debugger:command('bpset 21a8d2,1,{logerror "nsk3_controller_receive: pointer=%08x header=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,d@r0,d@(r13+10),totalcycles;g}')
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
