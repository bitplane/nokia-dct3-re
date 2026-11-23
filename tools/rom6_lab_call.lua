-- Minimal driver for the ROM6 sibling lab calls: no observers, only the
-- inputs. ROM6_LAB_PAGE is the page time; Send follows 4 s later and End
-- 14 s later. ROM6_LAB_CODE=1 first enters 12345 and Menu (OK) at 12 s.
local machine=manager.machine
assert(({nsm2ap5=true,nhm3ap5=true,nsb6ap5=true})[machine.system.name])
local page=assert(tonumber(os.getenv('ROM6_LAB_PAGE')))
local function field(name)
    for tag,port in pairs(machine.ioport.ports) do
        if tag:match('^:COL') or tag==':LAB_BTS' then
            for key,value in pairs(port.fields) do if key==name then return value end end
        end
    end
    error('no input field '..name)
end
local function hold(name,seconds)
    local input=field(name)
    machine:logerror(string.format('rom6_call_input: key=%s t=%.3f\n',name,machine.time:as_double()))
    input:set_value(1)
    local running=emu.wait(seconds)
    input:set_value(0)
    return running
end
local function until_time(t) return emu.wait(t-machine.time:as_double()) end
_G.rom6_lab_call=coroutine.create(function()
    if os.getenv('ROM6_LAB_CODE')=='1' then
        if not until_time(12) then return end
        for _,name in ipairs({'Keypad 1','Keypad 2','Keypad 3','Keypad 4','Keypad 5','Menu'}) do
            if not hold(name,0.15) or not emu.wait(0.35) then return end
        end
    end
    if not until_time(page-0.1) then return end
    machine.screens[':screen']:snapshot('idle.png')
    if not until_time(page) or not hold('Laboratory BTS page',0.05) then return end
    if not until_time(page+3.9) then return end
    machine.screens[':screen']:snapshot('ringing.png')
    if not until_time(page+4) or not hold(assert(os.getenv('ROM6_LAB_SEND')),0.15) then return end
    if not until_time(page+14) or not hold('End',0.15) then return end
end)
assert(coroutine.resume(_G.rom6_lab_call))
