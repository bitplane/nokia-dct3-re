-- Physical NPE-3 forwarding lifecycle against the laboratory network.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6210_staged_observe.lua")
local machine = manager.machine
machine.devices[":maincpu"].debug:bpset(0x4fad30, nil,
    'logerror "6210_keypad_decoded: key=%02x\\n",r0;g')
local function press(column, name)
    local key = assert(machine.ioport.ports[":COL." .. column].fields[name])
    machine:logerror("6210_divert_lifecycle_physical: key=" .. name .. "\n")
    key:set_value(1)
    if not emu.wait(0.15) then key:set_value(0); return false end
    key:set_value(0)
    return emu.wait(0.85)
end
local input = coroutine.create(function()
    if not emu.wait(20) then return end
    for index, sequence in ipairs({"*21*5551234#", "*#21#", "#21#", "*#21#"}) do
        machine:logerror("6210_divert_lifecycle_physical: transaction=" .. index .. "\n")
        for character in sequence:gmatch(".") do
            local column = character == "*" and 2 or character == "#" and 4
                or 2 + (tonumber(character) - 1) % 3
            if not press(column, "Keypad " .. character) then return end
        end
        if not press(0, "Send") or not emu.wait(1) then return end
        machine.screens[":screen"]:snapshot("6210_divert_lifecycle_" .. index .. ".png")
        if not press(1, "Right Softkey / C") or not emu.wait(1) then return end
    end
    machine.screens[":screen"]:snapshot("6210_divert_lifecycle_idle.png")
end)
_G.noki6210_divert_lifecycle_input = input
assert(coroutine.resume(input))
