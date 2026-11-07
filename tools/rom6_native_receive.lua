-- Passive receive-port chronology; no samples, interrupts or firmware writes.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_config.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local reads,handles=0,{}
local cap=1024
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_receive_prefix',function(_,value,mask)
    reads=reads+1
    if reads>cap then return end
    machine:logerror(string.format('rom6_receive_word: sequence=%d t=%.9f value=%04x mask=%04x cpu_pc=%06x pmst=%04x imr=%04x ifr=%04x observer_writes=0\n',
        reads,machine.time:as_double(),value,mask,cpu.state.GENPC.value,
        cpu.state.PMST.value,cpu.state.IMR.value,cpu.state.IFR.value))
    if reads==1 then
        local file=assert(io.open('rom6_receive_first_program.be16','wb'))
        for word=0,0xffff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(word))) end
        file:close()
        machine.debugger:command('dasm rom6_receive_first_program.asm,0,10000,1,:dsp_c54x:cpu')
    end
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_receive_complete: reads=%d captured=%d cap=%d observer_writes=0\n',reads,math.min(reads,cap),cap))
end)
_G.rom6_receive_observers=handles
