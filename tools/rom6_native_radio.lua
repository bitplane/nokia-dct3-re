-- Passive bus census around the qualified physical native-ROM6 UI run.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
local machine=manager.machine
local npe3=machine.system.name=='npe3ref' or machine.system.name=='npe3r6' or machine.system.name=='npe3afs' or machine.system.name=='npe3ad0' or machine.system.name=='npe3cp9' or machine.system.name=='npe3zp9' or machine.system.name=='npe3bp9' or machine.system.name=='npe3sp9' or machine.system.name=='npe3tp5' or machine.system.name=='npe3ap5'
dofile(directory..(npe3 and 'rom6_reset_policy_observe.lua' or 'rom6_native_ui.lua'))
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
-- State entries cached once: cpu.state[...] costs a container lookup per call.
local cpu_state=setmetatable({},{__index=function(t,k) local e=cpu.state[k]; rawset(t,k,e); return e end})
local handles={}
local write_cap=2097152
if machine.system.name=='npe3ap5' then
    local seconds=tonumber(os.getenv('NPE3_NATIVE_SECONDS') or '24')
    assert(seconds and seconds>=24 and seconds<=120)
    write_cap=write_cap*math.ceil(seconds/24)
end
-- The census of every I/O access (per-port rom6_radio_port summaries, the
-- rom6_radio_writes.tsv write stream and the rom6_radio_capture footer) is
-- kept by the DSP device: a Lua tap on all ports cost a fifth of a run.
-- Writing the stream cap into its save item enables it.
local backend=assert(machine.devices[':dsp_c54x'])
emu.item(assert(backend.items['0/m_io_census_cap'])):write(0,write_cap)
local function retire(handle)
    local co=coroutine.create(function() emu.wait(0.001); handle:remove() end)
    assert(coroutine.resume(co))
end
local captured_output_overlay=false
handles[#handles+1]=cpu.spaces.io:install_write_tap(0x31,0x31,'rom6_radio_first_output',function()
    if captured_output_overlay then return end
    captured_output_overlay=true
    retire(handles[1])
    local pc=cpu_state.GENPC.value
    -- This upload may later be overwritten by another overlay/data.
    local file=assert(io.open('rom6_radio_first_output.be16','wb'))
    for word=0,0x1fff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(word))) end
    file:close()
    machine.debugger:command('dasm rom6_radio_first_output.asm,a00,200,1,:dsp_c54x:cpu')
    machine:logerror(string.format('rom6_radio_overlay: event=first_p31 t=%.9f cpu_pc=%06x words=8192 observer_writes=0\n',machine.time:as_double(),pc))
end)
handles[#handles+1]=emu.add_machine_stop_notifier(function()
    local file=assert(io.open('rom6_radio_program.be16','wb'))
    for address=0,0xffff do file:write(string.pack('>I2',cpu.spaces.program:read_u16(address))) end
    file:close()
    machine.debugger:command('dasm rom6_radio_program.asm,0,10000,1,:dsp_c54x:cpu')
end)
_G.rom6_native_radio=handles
