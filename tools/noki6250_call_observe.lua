-- External network call fixture plus physical Send/End; no firmware writes.
local source = debug.getinfo(1, "S").source:sub(2)
local directory = assert(source:match("^(.*[/])"))
dofile(directory .. "noki6250_runtime_observe.lua")
local machine = manager.machine
local function cell(column, row)
    for _, field in pairs(machine.ioport.ports[":COL." .. column].fields) do
        if field.mask == (1 << row) then return field end
    end
    error("missing physical matrix cell")
end
local actions = {
    {20, cell(0, 2), 1}, {20.2, cell(0, 2), 0},
    {24, cell(0, 3), 1}, {24.2, cell(0, 3), 0}
}
local captures = {17, 21, 26, 32}
local next_action, next_capture = 1, 1
emu.register_periodic(function()
    local time = machine.time:as_double()
    local action = actions[next_action]
    if action and time >= action[1] then
        action[2]:set_value(action[3])
        machine:logerror(string.format("6250_call_input: step=%d pressed=%d t=%.6f\n",
            next_action, action[3], time))
        next_action = next_action + 1
    end
    if captures[next_capture] and time >= captures[next_capture] then
        machine.screens[":screen"]:snapshot("6250_call_" .. next_capture .. ".png")
        next_capture = next_capture + 1
    end
end)
