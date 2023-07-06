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
	div = 1.0,
	base = 1.0,
	isDraw = false,
	isPrint = false
}
_G.threatData = threatData

local spGetGroundHeight  = Spring.GetGroundHeight

local mapWidth  = Game.mapSizeX
local mapHeight = Game.mapSizeZ

function gadget:Initialize()
	Spring.Echo("Initialize AI DBG")
end

function gadget:RecvSkirmishAIMessage(teamID, dataStr)
--	Spring.Echo("teamID: " .. tostring(teamID) .. " | dataStr: " .. dataStr)
	local commandData  = "ai_thr_data:"
	local commandSize  = "ai_thr_size:"
	local commandDiv   = "ai_thr_div:"
	local commandDraw  = "ai_thr_draw:"
	local commandPrint = "ai_thr_print:"

	if dataStr:sub(1, #commandData) == commandData then
		-- "ai_thr_data:<val1> <val2> <val3> ..."
		local mapStr = dataStr:sub(#commandData + 1)
		threatMap = {}
		for m in mapStr:gmatch("%S+") do
			threatMap[#threatMap + 1] = m + 0.0;
		end
		threatData.map = threatMap
	elseif dataStr:sub(1, #commandSize) == commandSize then
		-- "ai_thr_size:<square_size> <threat_base>"
		local sb = dataStr:sub(commandSize:len() + 1)
		local slash = sb:find(" ", 1, true)
		if not slash then return end
		local ss = tonumber(sb:sub(1, slash - 1))
		local tb = tonumber(sb:sub(slash + 1))
		if not ss or not tb then return end
		threatData.base = tb
		threatData.size = ss
		threatData.width = mapWidth / threatData.size
		threatData.height = mapHeight / threatData.size
	elseif dataStr:sub(1, #commandDiv) == commandDiv then
		-- "ai_thr_div:<divider>"
		threatData.div = tonumber(dataStr:sub(#commandDiv + 1))
	elseif dataStr:sub(1, #commandDraw) == commandDraw then
		-- "ai_thr_draw:"
		threatData.isDraw = not threatData.isDraw
		return threatData.isDraw and "1" or "0"
	elseif dataStr:sub(1, #commandPrint) == commandPrint then
		-- "ai_thr_print:"
		threatData.isPrint = not threatData.isPrint
		return threatData.isPrint and "1" or "0"
	end
end

function gadget:DrawWorldPreUnit()
	if SYNCED and #SYNCED.threatData.map > 0 then
		local threatMap = SYNCED.threatData.map
		local width = SYNCED.threatData.width
		local height = SYNCED.threatData.height
		local size = SYNCED.threatData.size
		local div = SYNCED.threatData.div
		local base = SYNCED.threatData.base
--		Spring.Echo(threatMap[0 * width + 1])
--		Spring.Echo(threatMap[(height - 1) * width + width])

		if SYNCED.threatData.isDraw then
			for x = 1, width do
				px = (x - 1) * size
				for z = 0, height - 1 do
					value = threatMap[z * width + x] - base
					if value > 0 then
						pz = z * size
						value = value / div
						gl.Color(value, 0.0, 0.0, 0.6)
						gl.DrawGroundQuad(px + 1, pz + 1, px + size - 1, pz + size - 1)
					elseif value < 0 then
						pz = z * size
						value = -value / div
						gl.Color(0.0, 0.0, value, 0.6)
						gl.DrawGroundQuad(px + 1, pz + 1, px + size - 1, pz + size - 1)
					end
				end
			end
		end

		if SYNCED.threatData.isPrint then
			local halfSize = size / 2;
--			local cx, cy, cz = Spring.GetCameraDirection()
--			local dir = ((math.atan2(cx, cz) / math.pi) + 1) * 180

			for x = 1, width do
				px = (x - 1) * size + halfSize
				for z = 0, height - 1 do
					value = threatMap[z * width + x]
					if value ~= base then
						pz = z * size + halfSize
						local py = spGetGroundHeight(px, pz)
						if py < 0 then py = 0 end

						gl.PushMatrix()

						gl.Translate(px, py, pz)
						gl.Rotate(-90, 1, 0, 0)
--						gl.Rotate(dir, 0, 0, 1)
						gl.Text(("%.2f"):format(value), 0.0, 0.0, 14, "cno")

						gl.PopMatrix()
					end
				end
			end
		end

		gl.Color(1,1,1,1)
	end
end
