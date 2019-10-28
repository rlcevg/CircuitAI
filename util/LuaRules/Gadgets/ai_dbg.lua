function gadget:GetInfo() return {
	name    = "AI DBG",
	desc    = "AI DBG description",
	author  = "rlcevg",
	date    = "something, 2019",
	license = "GNU GPL, v2 or later",
	layer   = -1337,
	enabled = true
} end

local threatData = {
	map = {},
	width = 0,
	height = 0,
	size = 0,  -- tile size
	isDrawing = false
}
_G.threatData = threatData

local mapWidth = Game.mapSizeX
local mapHeight = Game.mapSizeZ

function gadget:Initialize()
	Spring.Echo("Initialize AI DBG")
	threatData.isDrawing = false
end

function gadget:RecvSkirmishAIMessage(teamID, dataStr)
--	Spring.Echo("teamID: " .. tostring(teamID) .. " | dataStr: " .. dataStr)
	local command1 = "ai_threat:"
	local command2 = "ai_draw:"
	
	if dataStr:sub(1, #command1) == command1 then
		-- "ai_threat:<val1> <val2> <val3> ..."
		local mapStr = dataStr:sub(#command1 + 1)
		threatMap = {}
		for m in mapStr:gmatch("%S+") do
			threatMap[#threatMap + 1] = m + 0.0;
		end
		threatData.map = threatMap
	elseif dataStr:sub(1, #command2) == command2 then
		-- "ai_draw:<square_size>"
		threatData.size = tonumber(dataStr:sub(#command2 + 1))
		threatData.width = mapWidth / threatData.size
		threatData.height = mapHeight / threatData.size
		threatData.isDrawing = not threatData.isDrawing
	end
end

function gadget:DrawWorldPreUnit()
	if SYNCED and SYNCED.threatData.isDrawing and #SYNCED.threatData.map > 0 then
		local threatMap = SYNCED.threatData.map
		local width = SYNCED.threatData.width
		local height = SYNCED.threatData.height
		local size = SYNCED.threatData.size
--		Spring.Echo(threatMap[0 * width + 1])
--		Spring.Echo(threatMap[(height - 1) * width + width])
		for x = 1, width do
			px = (x - 1) * size
			for z = 0, height - 1 do
				pz = z * size
				gl.Color(threatMap[z * width + x], 0.0, 0.0, 0.6)
--				gl.Color(1.0, 0.0, 0.0, threatMap[z * width + x])
				gl.DrawGroundQuad(px, pz, px + size, pz + size)
			end
		end
		gl.Color(1,1,1,1)
	end
end
