-- Passive receive-port chronology; no samples, interrupts or firmware writes.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_config.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local reads,handles=0,{}
local cap=1024
local positions,programming=0,0
local short_frame_captured=false
handles[#handles+1]=cpu.spaces.io:install_write_tap(0x0e,0x0e,'rom6_receive_reload',function(_,value,mask)
    machine:logerror(string.format('rom6_receive_reload: t=%.9f value=%04x a=%010x b=%010x ar1=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        machine.time:as_double(),value,cpu.state.A.value&0xffffffffff,cpu.state.B.value&0xffffffffff,
        cpu.state.AR1.value,cpu.state.GENPC.value,mask))
    if value~=0x1387 and not short_frame_captured then
        short_frame_captured=true
        local file=assert(io.open('rom6_receive_short_frame_program.be16','wb'))
        for word=0,0x1fff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(word))) end
        file:close()
        machine.debugger:command('dasm rom6_receive_short_frame_program.asm,900,180,1,:dsp_c54x:cpu')
    end
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x1844,0x1844,'rom6_receive_position',function(_,value,mask)
    positions=positions+1
    if positions>16384 then return end
    machine:logerror(string.format('rom6_receive_position: sequence=%d t=%.9f value=%04x ar1=%04x length=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        positions,machine.time:as_double(),value,cpu.state.AR1.value,
        cpu.spaces.data:read_u16(0x036a),cpu.state.GENPC.value,mask))
end)
handles[#handles+1]=cpu.spaces.io:install_write_tap(9,9,'rom6_receive_programming',function(_,value,mask)
    programming=programming+1
    if programming>16384 then return end
    machine:logerror(string.format('rom6_receive_programming: sequence=%d t=%.9f value=%04x ar1=%04x position1844=%04x length=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        programming,machine.time:as_double(),value,cpu.state.AR1.value,
        cpu.spaces.data:read_u16(0x1844),cpu.spaces.data:read_u16(0x036a),cpu.state.GENPC.value,mask))
end)
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x27,0x27,'rom6_receive_prefix',function(_,value,mask)
    reads=reads+1
    if reads>cap then return end
    machine:logerror(string.format('rom6_receive_word: sequence=%d t=%.9f value=%04x mask=%04x cpu_pc=%06x pmst=%04x imr=%04x ifr=%04x observer_writes=0\n',
        reads,machine.time:as_double(),value,mask,cpu.state.GENPC.value,
        cpu.state.PMST.value,cpu.state.IMR.value,cpu.state.IFR.value))
    if reads==1 then
        local coefficients={}
        for word=0x3ebd,0x3ec4 do coefficients[#coefficients+1]=string.format('%04x',cpu.spaces.data:read_u16(word)) end
        machine:logerror('rom6_receive_coefficients: data=3ebd words=8 values='..table.concat(coefficients,',')..' observer_writes=0\n')
        local file=assert(io.open('rom6_receive_first_program.be16','wb'))
        for word=0,0xffff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(word))) end
        file:close()
        machine.debugger:command('dasm rom6_receive_first_program.asm,0,10000,1,:dsp_c54x:cpu')
    end
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_receive_complete: reads=%d captured=%d cap=%d observer_writes=0\n',reads,math.min(reads,cap),cap))
    machine:logerror(string.format('rom6_receive_programming_complete: positions=%d programming=%d cap=16384 observer_writes=0\n',positions,programming))
end)
_G.rom6_receive_observers=handles
