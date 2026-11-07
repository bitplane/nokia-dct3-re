-- Read-only class/frame scheduler boundary; never retime the independent source.
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/])'))
dofile(directory..'rom6_native_observe.lua')
local machine=manager.machine
local cpu=assert(machine.devices[':dsp_c54x:cpu'])
local events=0
local sch_seen,finished=false,false
local function word(address) return cpu.spaces.data:read_u16(address) end
local function record(kind,address,value)
    if machine.time:as_double()<3.2 then return end
    if finished then return end
    events=events+1
    if events>256 then return end
    machine:logerror(string.format('rom6_frame_schedule: sequence=%d t=%.9f kind=%s address=%04x value=%04x owner=%06x pc=%06x current=%04x,%04x next=%04x,%04x class=%04x offset=%04x,%04x shift=%04x observer_writes=0\n',
        events,machine.time:as_double(),kind,address,value,cpu.state.EXECPC.value,cpu.state.GENPC.value,
        word(0x912),word(0x913),word(0x91c),word(0x91d),word(0x2f02),word(0x247a),word(0x247b),word(0x2615)))
    if kind=='data' and address==0x2f02 then
        if value==0x40 then sch_seen=true
        elseif sch_seen and value==0 then finished=true end
    end
end
_G.rom6_frame_schedule_taps={}
for _,address in ipairs({0x913,0x91d,0x2f02}) do
    _G.rom6_frame_schedule_taps[#_G.rom6_frame_schedule_taps+1]=cpu.spaces.data:install_write_tap(address,address,'rom6_frame_schedule',function(at,value)
        record('data',at,value)
    end)
end
for _,port in ipairs({3,9,0x0d,0x0e,0x0f}) do
    _G.rom6_frame_schedule_taps[#_G.rom6_frame_schedule_taps+1]=cpu.spaces.io:install_write_tap(port,port,'rom6_frame_schedule_port',function(at,value)
        record('port',at,value)
    end)
end
_G.rom6_frame_schedule_stop=emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('rom6_frame_schedule_complete: events=%d cap=256 observer_writes=0\n',events))
end)
