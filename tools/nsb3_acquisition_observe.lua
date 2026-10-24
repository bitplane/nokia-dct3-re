-- Own acquisition consumer observation; never supplies samples or success.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb3_startup_observe.lua')
local machine = manager.machine
local cpu = machine.devices[':maincpu']
machine.debugger:command('temp4=0')
cpu.debug:bpset(0x27a4c2, 'temp4<10',
    'temp4=temp4+1;logerror "nsb3_acquisition: result=%08x sample0=%04x sample1=%04x remaining=%02x gain=%08x offset=%08x observer_writes=0\\n",r0,w@r13,w@(r13+2),b@10a0e2,d@10fe54,d@10fe58;g')
