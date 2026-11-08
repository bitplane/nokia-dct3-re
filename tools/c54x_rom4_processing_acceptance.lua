-- The shared passive census also serves other products without a debugger.
-- Keep execution-entry acceptance explicit rather than changing those callers.
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'c54x_rom4_processing_observe.lua')
dofile(directory..'c54x_rom4_processing_execute.lua')
