-- Minimal driver for the ROM6 product lab calls: no observers, only inputs.
-- ROM6_LAB_EVENTS is "time|field|seconds;..." in emulated time; a field of
-- "@name" takes the snapshot name.png instead. ROM6_LAB_ROUNDTRIP_AT adds a
-- save, a NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS recorded replay and a load, after which DSP I/O
-- must repeat exactly (c54x_rom4_codec_restore.lua). Loading cancels every
-- pending emu.wait, so the restored branch schedules the inputs still due.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
local machine=manager.machine
assert(({nsm2ap5=true,nhm3ap5=true,nsb6ap5=true,nsm3ap5=true})[machine.system.name])
local events={}
for time,name,seconds in assert(os.getenv('ROM6_LAB_EVENTS')):gmatch('([%d.]+)|([^|;]+)|([%d.]+)') do
    events[#events+1]={assert(tonumber(time)),name,assert(tonumber(seconds))}
end
assert(#events>0)
local function field(name)
    for tag,port in pairs(machine.ioport.ports) do
        if tag:match('^:COL') or tag==':LAB_BTS' then
            for key,value in pairs(port.fields) do if key==name then return value end end
        end
    end
    error('no input field '..name)
end
local function inputs()
    local task=coroutine.create(function()
        for _,event in ipairs(events) do
            local now=machine.time:as_double()
            if event[1]>=now then
                if not emu.wait(event[1]-now) then return end
                if event[2]:sub(1,1)=='@' then
                    machine.screens[':screen']:snapshot(event[2]:sub(2)..'.png')
                else
                    local input=field(event[2])
                    machine:logerror(string.format('rom6_call_input: key=%s t=%.3f\n',event[2],machine.time:as_double()))
                    input:set_value(1)
                    if not emu.wait(event[3]) then input:set_value(0);return end
                    input:set_value(0)
                end
            end
        end
    end)
    assert(coroutine.resume(task))
    return task
end
local at=tonumber(os.getenv('ROM6_LAB_ROUNDTRIP_AT') or '')
if at then
    -- Set up the replay recorder before any input, as it owns load notifiers.
    local replay=assert(tonumber(os.getenv('NOKIA_DCT3_STATE_ROUNDTRIP_REPLAY_MS')))/1000
    for _,event in ipairs(events) do
        assert(at<event[1]-replay-0.01 or at>event[1]+event[3],'round trip overlaps an input')
    end
    _G.codec_restore_without_exerciser=true
    dofile(directory..'c54x_rom4_codec_restore.lua')
    local dsp=assert(machine.devices[':dsp_c54x:cpu'])
    local mcu=assert(machine.devices[':maincpu']).spaces['program']
    local function structure()
        return string.format('fiq=%02x irq=%02x ctrl=%02x dsp_idle=%d dsp_illegal=%d',
            mcu:read_u8(0x2000a),mcu:read_u8(0x2000b),mcu:read_u8(0x2000c),
            dsp.state['IDLE'].value,dsp.state['ILLEGAL'].value)
    end
    _G.rom6_roundtrip=coroutine.create(function()
        if not emu.wait(at) then return end
        local saved=structure()
        local requested=machine.time:as_double()
        machine:logerror(string.format('state_replay: phase=reference event=begin t=%.6f\n',requested))
        machine:save('rom6_lab_call')
        if not emu.wait(replay) then return end
        machine:logerror(string.format('state_replay: phase=reference event=end t=%.6f\n',machine.time:as_double()))
        machine:load('rom6_lab_call')
        emu.wait(0.01) -- resumed (cancelled) by the load itself
        local restored=structure()
        machine:logerror(string.format('state_roundtrip: result=%s scenario=rom6_lab_call %s requested_at=%.6f t=%.6f\n',
            (restored==saved and dsp.state['ILLEGAL'].value==0) and 'pass' or 'fail',restored,requested,
            machine.time:as_double()))
        _G.rom6_inputs=inputs()
    end)
    assert(coroutine.resume(_G.rom6_roundtrip))
end
_G.rom6_inputs=inputs()
