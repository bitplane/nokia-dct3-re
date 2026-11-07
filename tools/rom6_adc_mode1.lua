-- Nonzero native mode-1 product observations; no firmware writes or calls.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_adc_lab.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local filtered,mixed,filter_overflow={},0,0
local function hex(words)
    local result={}
    for _,word in ipairs(words) do result[#result+1]=string.format('%04x',word) end
    return table.concat(result,',')
end
_G.rom6_mode1_filter=cpu.spaces.data:install_write_tap(0xc8,0xd7,'rom6_mode1_filter',function(address,value)
    if mixed>=16 or cpu.spaces.data:read_u16(0x56e)~=1 then return end
    if #filtered<16 then
        filtered[#filtered+1]={address=address,value=value}
    else
        filter_overflow=filter_overflow+1
    end
end)
_G.rom6_mode1_mixer=cpu.spaces.data:install_write_tap(0x518,0x518,'rom6_mode1_mixer',function(_,value)
    if mixed>=16 then return end
    if cpu.spaces.data:read_u16(0x56e)~=1 then filtered={};return end
    local inputs,nonzero={},false
    for _,word in ipairs(filtered) do
        inputs[#inputs+1]=word.value
        if word.value~=0 then nonzero=true end
    end
    if mixed>0 or nonzero then
        mixed=mixed+1
        local previous=cpu.spaces.data:read_u16(0x518)
        local outputs={}
        if previous>=0x80 and previous<0xc8 and (value-previous+0x48)%0x48==2 then
            for index=0,1 do outputs[#outputs+1]=cpu.spaces.data:read_u16(0x80+(previous-0x80+index)%0x48) end
        end
        local addresses={}
        for _,word in ipairs(filtered) do addresses[#addresses+1]=word.address end
        machine:logerror(string.format('rom6_mode1_mixer: block=%d previous=%04x pointer=%04x inputs=%s addresses=%s outputs=%s pc=%06x observer_writes=0\n',
            mixed,previous,value,hex(inputs),hex(addresses),hex(outputs),cpu.state.GENPC.value))
    end
    filtered={}
end)
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp5=0')
-- Debugger integer literals are hexadecimal; keep the 100-phase cap explicit.
cpu.debug:bpset(0x422b,'temp5<0x64 && dw@56e==1 && (dw@2f52|dw@2f53|dw@2f54|dw@2f55)!=0',
    'temp5=temp5+1;logerror "rom6_mode1_product_input: phase=%d values=%04x,%04x,%04x,%04x st1=%04x observer_writes=0\\n",temp5,dw@2f52,dw@2f53,dw@2f54,dw@2f55,st1;g')
for _,address in ipairs({0x41a0,0x41fc}) do
    cpu.debug:bpset(address,'(temp5&1)==1',
        'logerror "rom6_mode1_product_output: phase=%d a=%010x b=%010x ar2=%04x ar3=%04x word2f51=%04x words=%04x,%04x,%04x,%04x st1=%04x observer_writes=0\\n",temp5,a,b,ar2,ar3,dw@2f51,dw@2f52,dw@2f53,dw@2f54,dw@2f55,st1;temp5=temp5+1;g')
end
_G.rom6_mode1_stop=emu.add_machine_stop_notifier(function()
    machine.debugger.visible_cpu=cpu
    machine.debugger:command('logerror "rom6_mode1_complete: phases=%d cap=100 observer_writes=0\\n",temp5')
    machine:logerror(string.format('rom6_mode1_mixer_complete: blocks=%d cap=16 observer_writes=0\n',mixed))
    machine:logerror(string.format('rom6_mode1_filter_complete: overflow=%d observer_writes=0\n',filter_overflow))
end)
