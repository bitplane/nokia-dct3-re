-- Passive configuration producers/selectors, composed with the qualified tune trace.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_tuning.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local handles={}
local producers,selections=0,0
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x3f0f,0x3f10,'rom6_config_producer',function(address,value,mask)
    producers=producers+1
    if producers>128 then return end
    local pc=cpu.state.GENPC.value
    machine:logerror(string.format('rom6_config_write: sequence=%d t=%.9f address=%04x value=%04x mask=%04x cpu_pc=%06x observer_writes=0\n',
        producers,machine.time:as_double(),address,value,mask,pc))
    if address==0x3f10 and value~=0 then
        -- Published prefix plus this incoming store value, not a RAM poke.
        local file=assert(io.open('rom6_config_uploaded_prefix.be16','wb'))
        for word=0x3e7e,0x3f0f do file:write(string.pack('>I2',cpu.spaces.data:read_u16(word))) end
        file:write(string.pack('>I2',value)); file:close()
        machine:logerror('rom6_config_prefix: base=3e7e words=147 incoming_store=3f10 observer_writes=0\n')
    end
end)
handles[#handles+1]=cpu.spaces.data:install_write_tap(0x1275,0x1275,'rom6_config_selection',function(_,value,mask)
    if value~=0x4a80 then return end
    selections=selections+1
    if selections>4096 then return end
    machine:logerror(string.format('rom6_config_select: sequence=%d t=%.9f value=%04x word3f0f=%04x word3f10=%04x class263b=%04x cpu_pc=%06x mask=%04x observer_writes=0\n',
        selections,machine.time:as_double(),cpu.spaces.data:read_u16(0x1274),
        cpu.spaces.data:read_u16(0x3f0f),cpu.spaces.data:read_u16(0x3f10),
        cpu.spaces.data:read_u16(0x263b),cpu.state.GENPC.value,mask))
end)
machine.debugger:command('temp8=0')
cpu.debug:bpset(0x3d81,'temp8<10000',
    'temp8=temp8+1;logerror "rom6_config_class_entry: sequence=%d key=%010x observer_writes=0\\n",temp8,b;g')
cpu.debug:bpset(0x3d92,'temp8>0 && temp8<10000',
    'logerror "rom6_config_class_result: sequence=%d class263b=%04x observer_writes=0\\n",temp8,dw@263b;g')
cpu.debug:bpset(0x1994,'temp8>0 && temp8<10000',
    'logerror "rom6_config_selector: sequence=%d old_payload=%04x match3efc=%04x class263b=%04x observer_writes=0\\n",temp8,dw@1274,dw@3efc,dw@263b;g')
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_config_complete: producers=%d producer_cap=128 selections=%d selection_cap=4096 observer_writes=0\n',producers,selections))
end)
_G.rom6_config_observers=handles
