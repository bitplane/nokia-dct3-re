-- Passive bus census around the qualified physical native-ROM6 UI run.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
local machine=manager.machine
local npe3=machine.system.name=='npe3ref' or machine.system.name=='npe3r6' or machine.system.name=='npe3afs' or machine.system.name=='npe3ad0' or machine.system.name=='npe3cp9' or machine.system.name=='npe3zp9' or machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5'
dofile(directory..(npe3 and 'rom6_reset_policy_observe.lua' or 'rom6_native_ui.lua'))
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local stream=assert(io.open('rom6_radio_writes.tsv','wb'))
local stats,handles={},{}
local writes,overflow=0,false
local write_cap=2097152
if machine.system.name=='npe3ap5' then
    local seconds=tonumber(os.getenv('NPE3_NATIVE_SECONDS') or '24')
    assert(seconds and seconds>=24 and seconds<=120)
    write_cap=write_cap*math.ceil(seconds/24)
end
local captured_output_overlay=false
local function observe(direction,address,value,mask)
    local now=machine.time:as_double()
    local key=string.format('%s:%04x',direction,address)
    local item=stats[key]
    if not item then
        item={direction=direction,address=address,count=0,first=now,last=now,
            values={},value_count=0,values_capped=false,pcs={},pc_count=0,pcs_capped=false}
        stats[key]=item
    end
    item.count=item.count+1; item.last=now
    if not item.values[value] then
        if item.value_count<16 then item.values[value]=true; item.value_count=item.value_count+1
        else item.values_capped=true end
    end
    -- Bus-time CPU state is context, not a claim about the issuing pipeline slot.
    local pc=cpu.state.GENPC.value
    if not item.pcs[pc] then
        if item.pc_count<32 then item.pcs[pc]=true; item.pc_count=item.pc_count+1
        else item.pcs_capped=true end
    end
    if direction=='W' then
        if writes>=write_cap then overflow=true; return end
        writes=writes+1
        assert(stream:write(string.format('%.9f\t%06x\t%04x\t%04x\t%04x\n',now,pc,address,value,mask)))
        if address==0x31 and not captured_output_overlay then
            captured_output_overlay=true
            -- This upload may later be overwritten by another overlay/data.
            local file=assert(io.open('rom6_radio_first_output.be16','wb'))
            for word=0,0x1fff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(word))) end
            file:close()
            machine.debugger:command('dasm rom6_radio_first_output.asm,a00,200,1,:dsp_c54x:cpu')
            machine:logerror(string.format('rom6_radio_overlay: event=first_p31 t=%.9f cpu_pc=%06x words=8192 observer_writes=0\n',now,pc))
        end
    end
end
handles[#handles+1]=cpu.spaces.io:install_read_tap(0,0xffff,'rom6_radio_reads',function(a,v,m) observe('R',a,v,m) end)
handles[#handles+1]=cpu.spaces.io:install_write_tap(0,0xffff,'rom6_radio_writes',function(a,v,m) observe('W',a,v,m) end)
local function words(values)
    local result={}
    for value in pairs(values) do result[#result+1]=value end
    table.sort(result)
    for index,value in ipairs(result) do result[index]=string.format('%04x',value) end
    return table.concat(result,',')
end
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    stream:close()
    local keys={}
    for key in pairs(stats) do keys[#keys+1]=key end
    table.sort(keys)
    for _,key in ipairs(keys) do
        local item=stats[key]
        machine:logerror(string.format('rom6_radio_port: direction=%s address=%04x count=%d first=%.9f last=%.9f values=%s values_capped=%d cpu_pcs=%s pcs_capped=%d\n',
            item.direction,item.address,item.count,item.first,item.last,words(item.values),item.values_capped and 1 or 0,
            words(item.pcs),item.pcs_capped and 1 or 0))
    end
    local file=assert(io.open('rom6_radio_program.be16','wb'))
    for address=0,0xffff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(address))) end
    file:close()
    machine.debugger:command('dasm rom6_radio_program.asm,0,10000,1,:dsp_c54x:cpu')
    machine:logerror(string.format('rom6_radio_capture: writes=%d overflow=%d observer_writes=0\n',writes,overflow and 1 or 0))
end)
_G.rom6_native_radio=handles
