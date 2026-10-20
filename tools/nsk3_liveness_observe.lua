-- Own packet-supervision counter and policy boundary; no heartbeat injection.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_long_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
machine.debugger:command('bpset 2ab64a,1,{logerror "nsk3_liveness_window: counter=%04x pointer=%08x cycles=%016x observer_writes=0\\n",w@(r5+2),r5,totalcycles;g}')
machine.debugger:command('bpset 2a2a4a,r0==1,{logerror "nsk3_liveness_policy: argument=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
local writes=0
_G.nsk3_liveness_counter_tap=memory:install_write_tap(0x10c5a4,0x10c5a7,'nsk3_liveness_counter',function(offset,value,mask)
    if mask & 0xffff==0 then return end
    writes=writes+1
    if writes<=64 then machine:logerror(string.format('nsk3_liveness_counter: value=%04x mask=%08x pc=%08x t=%.9f observer_writes=0\n',value&0xffff,mask,cpu.state['PC'].value,machine.time:as_double())) end
end)
