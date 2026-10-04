-- Enumerate firmware menu presentation through physical keys only.
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
local calculator = os.getenv("NOKIA_DCT3_6250_CALCULATOR") == "1"
local actions = {{16, cell(1, 1), 1}, {16.15, cell(1, 1), 0}}
for n = 1, calculator and 6 or 9 do
    actions[#actions + 1] = {16 + 2*n, cell(1, 3), 1}
    actions[#actions + 1] = {16.15 + 2*n, cell(1, 3), 0}
end
if calculator then
    for _, action in ipairs({
        {30, cell(1, 1)}, {32, cell(2, 1)}, {34, cell(1, 1)},
        {36, cell(1, 1)}, {38, cell(3, 1)}, {40, cell(1, 1)},
        {42, cell(1, 1)},
    }) do
        actions[#actions + 1] = {action[1], action[2], 1}
        actions[#actions + 1] = {action[1] + 0.15, action[2], 0}
    end
end
local next_action, next_capture = 1, 1
emu.register_periodic(function()
    local time = machine.time:as_double()
    local action = actions[next_action]
    if action and time >= action[1] then
        action[2]:set_value(action[3])
        machine:logerror(string.format("6250_app_input: step=%d pressed=%d t=%.6f\n",
            next_action, action[3], time))
        next_action = next_action + 1
    end
    if next_capture <= (calculator and 14 or 10) and time >= 15 + 2*next_capture then
        machine.screens[":screen"]:snapshot("6250_app_" .. next_capture .. ".png")
        next_capture = next_capture + 1
    end
end)
