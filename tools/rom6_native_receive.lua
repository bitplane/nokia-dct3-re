-- Passive receive-port chronology; no samples, interrupts or firmware writes.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_config.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local reads,handles=0,{}
local cap=1024
local positions,programming=0,0
local short_frame_captured=false
local capture_writes=0
local counter_reads={}
handles[#handles+1]=cpu.spaces.io:install_read_tap(0x0d,0x0d,'rom6_capture_counter',function(_,value,mask)
    counter_reads[#counter_reads+1]={value=value,mask=mask,pc=cpu.state.GENPC.value,t=machine.time:as_double()}
    if #counter_reads>2 then table.remove(counter_reads,1) end
end)
for _,port in ipairs({0x0a,0x0b,0x11}) do
    handles[#handles+1]=cpu.spaces.io:install_write_tap(port,port,'rom6_capture_'..port,function(_,value,mask)
        capture_writes=capture_writes+1
        if capture_writes>32768 then return end
        machine:logerror(string.format('rom6_capture_write: sequence=%d t=%.9f port=%04x value=%04x ar1=%04x a=%010x b=%010x lead=%04x tail=%04x length=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
            capture_writes,machine.time:as_double(),port,value,cpu.state.AR1.value,
            cpu.state.A.value&0xffffffffff,cpu.state.B.value&0xffffffffff,
            cpu.spaces.data:read_u16(0x3ea7),cpu.spaces.data:read_u16(0x3eb6),
            cpu.spaces.data:read_u16(0x036a),cpu.state.GENPC.value,mask))
        if port==0x0b and cpu.state.GENPC.value==0x15d0 then
            assert(#counter_reads==2,'capture stop without two observed counter reads')
            local first,last=counter_reads[1],counter_reads[2]
            machine:logerror(string.format('rom6_capture_stop_poll: sequence=%d first=%04x first_pc=%06x first_t=%.9f first_mask=%04x last=%04x last_pc=%06x last_t=%.9f last_mask=%04x observer_writes=0\n',
                capture_writes,first.value,first.pc,first.t,first.mask,last.value,last.pc,last.t,last.mask))
        end
    end)
end
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
    machine:logerror(string.format('rom6_capture_complete: writes=%d cap=32768 observer_writes=0\n',capture_writes))
    machine:logerror(string.format('rom6_receive_complete: reads=%d captured=%d cap=%d observer_writes=0\n',reads,math.min(reads,cap),cap))
    machine:logerror(string.format('rom6_receive_programming_complete: positions=%d programming=%d cap=16384 observer_writes=0\n',positions,programming))
end)
_G.rom6_receive_observers=handles
