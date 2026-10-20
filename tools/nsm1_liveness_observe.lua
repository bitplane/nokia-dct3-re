-- Own NSM-1 empty-window policy; no injected liveness or policy changes.
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_native_radio_long.lua')
local machine=manager.machine
machine.debugger:command('bpset 2c1cc2,1,{logerror "nsm1_liveness_window: counter=%04x context=%08x cycles=%016x observer_writes=0\\n",w@(r4+2),r4,totalcycles;g}')
machine.debugger:command('bpset 2b4b72,r0==1,{logerror "nsm1_liveness_entry: argument=%08x caller=%08x cycles=%016x observer_writes=0\\n",r0,r14,totalcycles;g}')
machine.debugger:command('bpset 2b4b9c,r5==1,{logerror "nsm1_liveness_config: argument=%08x bytes=%02x%02x cycles=%016x observer_writes=0\\n",r5,b@r13,b@(r13+1),totalcycles;g}')
machine.debugger:command('bpset 2b4bcc,r5==1,{logerror "nsm1_liveness_selected: reason=%08x cycles=%016x observer_writes=0\\n",r4,totalcycles;g}')
machine.debugger:command('bpset 2b4c1e,r5==1,{logerror "nsm1_liveness_exit: reason=%08x cycles=%016x observer_writes=0\\n",r4,totalcycles;g}')
