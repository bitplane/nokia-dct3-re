-- Passive SCH frontend snapshots, including only the class-40 transaction.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_receive_entry.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local reads=0
_G.rom6_frontend_raw=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_frontend_raw',function(_,value)
    if cpu.spaces.data:read_u16(0x2f02)~=0x40 then return end
    reads=reads+1
    if reads<=1024 then
        machine:logerror(string.format('rom6_sch_adc: index=%d value=%04x t=%.9f pc=%06x count=%04x mode=%04x observer_writes=0\n',
            reads,value&0xffff,machine.time:as_double(),cpu.state.GENPC.value,
            cpu.spaces.data:read_u16(0x231),cpu.spaces.data:read_u16(0x22e)))
    end
end)
for _,entry in ipairs({{0x743a,'samples',0x10},{0x7937,'filter',0x20},
                      {0x76a7,'training',0x40},{0x7790,'equalizer',0x80}}) do
    _G.rom6_receive_entry_add(entry[1],entry[2],entry[3],'dw@2f02==40')
end
_G.rom6_frontend_stop=emu.add_machine_stop_notifier(function()
    manager.machine.debugger:command('logerror "rom6_receive_frontend_complete: stages=%x expected=ff observer_writes=0\\n",temp5')
    machine:logerror(string.format('rom6_sch_adc_complete: reads=%d cap=1024 observer_writes=0\n',reads))
end)
