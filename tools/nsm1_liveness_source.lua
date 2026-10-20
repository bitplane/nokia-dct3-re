-- Observe the record reader and raw serial source of the packet-timeout bit.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_liveness_observe.lua')
local machine=manager.machine
machine.debugger:command('bpset 2b8b90,r1==607,{logerror "nsm1_liveness_record: record=%04x pointer=%08x caller=%08x cycles=%016x observer_writes=0\\n",r1,r0,r14,totalcycles;g}')
machine.debugger:command('bpset 2bcace,r0==3ca,{logerror "nsm1_liveness_serial: offset=%04x pointer=%08x length=%04x cycles=%016x observer_writes=0\\n",r0,r1,r2,totalcycles;g}')
machine.debugger:command('bpset 2b4b8a,r5==1,{logerror "nsm1_liveness_read_result: result=%08x byte=%02x cycles=%016x observer_writes=0\\n",r0,b@r13,totalcycles;g}')
