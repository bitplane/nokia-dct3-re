-- Observe own checksum comparison; only the initial external NV fixture differs.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse5_fullbackup_v501_observe.lua')
manager.machine.debugger:command('bpset 3af8f2,1,{logerror "nse5_checksum_compare: computed=%08x stored=%08x observer_writes=0\\n",r0,d@(r5+3c);g}')
manager.machine.debugger:command('bpset 3af904,1,{logerror "nse5_checksum_result: accepted=%08x observer_writes=0\\n",r0;g}')
