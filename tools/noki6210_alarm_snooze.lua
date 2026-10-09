-- Physical Snooze observation using the same independently provisioned clock.
_G.noki6210_alarm_snooze = true
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_alarm_input.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:bpset(0x4f277a, 'temp7<16',
    'temp7=temp7+1;logerror "6210_alarm_irq: active=%02x cached_mask=%02x cached_status=%02x\\n",r4,b@1742fd,b@1742fc;g')
