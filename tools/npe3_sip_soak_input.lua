-- Physical keys for the 6210 SIP soak, taken from a command file.
-- NPE3_SOAK_COMMANDS names a file the controller appends lines to; each line
-- is "key NAME SECONDS" (hold an input field) or "snap NAME" (LCD snapshot).
-- Lines run in order, in emulated time, as they arrive. No observers.
local machine=manager.machine
local path=assert(os.getenv('NPE3_SOAK_COMMANDS'))
local function field(name)
    for tag,port in pairs(machine.ioport.ports) do
        if tag:match('^:COL') then
            for key,value in pairs(port.fields) do if key==name then return value end end
        end
    end
    error('no input field '..name)
end
_G.npe3_sip_soak=coroutine.create(function()
    local offset=0
    while emu.wait(0.05) do
        local file=io.open(path,'r')
        if file then
            file:seek('set',offset)
            local text=file:read('a')
            file:close()
            -- Only whole lines; a partly written one waits for the next poll.
            local complete=text:match('^(.*\n)') or ''
            offset=offset+#complete
            for line in complete:gmatch('([^\n]*)\n') do
                local kind,rest=line:match('^(%S+) (.*)$')
                local name,seconds=rest:match('^(.-) ([%d.]+)$')
                if kind=='snap' then name=rest end
                if kind=='key' then
                    local input=field(name)
                    machine:logerror(string.format('npe3_soak_input: key=%s t=%.3f\n',name,machine.time:as_double()))
                    input:set_value(1)
                    if not emu.wait(tonumber(seconds)) then input:set_value(0);return end
                    input:set_value(0)
                    if not emu.wait(0.15) then return end
                elseif kind=='snap' then
                    machine.screens[':screen']:snapshot(name..'.png')
                else
                    error('bad soak command '..line)
                end
            end
        end
    end
end)
assert(coroutine.resume(_G.npe3_sip_soak))
