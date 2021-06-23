function gadget:GetInfo() return {
	name    = "AI DBG",
	desc    = "AI DBG description",
	author  = "rlcevg, Beherith",
	date    = "something, 2019",
	license = "GNU GPL, v2 or later",
	layer   = -1337,
	enabled = true,
} end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
if (gadgetHandler:IsSyncedCode()) then  -- Synced
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local isDraw = false
local isPrint = false

function gadget:RecvSkirmishAIMessage(teamID, dataStr)
-- 	Spring.Echo("teamID: " .. tostring(teamID) .. " | dataStr: " .. dataStr)
	local prefix = "ai_"
	if dataStr:sub(1, #prefix) ~= prefix then
		return
	end

	SendToUnsynced("AiDbgEvent", teamID, dataStr)

	local cmdThrDraw  = "ai_thr_draw:"
	local cmdThrPrint = "ai_thr_print:"

	if dataStr:sub(1, #cmdThrDraw) == cmdThrDraw then
		isDraw = not isDraw
		return isDraw and "1" or "0"
	elseif dataStr:sub(1, #cmdThrPrint) == cmdThrPrint then
		isPrint = not isPrint
		return isPrint and "1" or "0"
	end
end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
else  -- Unsynced
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

-------- GL4 THINGS ----------
local isGL4Way = false
local circleInstanceVBOthr = nil
local circleShaderThr = nil
local circleInstanceVBOmrk = nil
local circleShaderMrk = nil

local function makeConeVBO(numSegments, height, radius)
	-- make a cone that points up, (y = height), with radius specified
	-- returns the VBO object, and the number of elements in it (usually ==  numvertices)
	-- needs GL.TRIANGLES
	if not height then height = 1 end
	if not radius then radius = 1 end
	local coneVBO = gl.GetVBO(GL.ARRAY_BUFFER,true)
	if coneVBO == nil then return nil end

	local VBOData = {}

	for i = 1, numSegments do
		-- center vertex
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = (i - 1) / numSegments

		--- second cone flat
		VBOData[#VBOData+1] = math.sin(math.pi*2* (i - 0) / numSegments) * radius-- X
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = -1* math.cos(math.pi*2* (i - 0) / numSegments) * radius -- Y
		VBOData[#VBOData+1] =(i - 0) / numSegments

		--- first cone flat
		VBOData[#VBOData+1] = math.sin(math.pi*2* (i - 1) / numSegments) * radius -- X
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = -1* math.cos(math.pi*2* (i - 1) / numSegments) * radius-- Y
		VBOData[#VBOData+1] = (i - 1) / numSegments
	end
	for i = 1, numSegments do
		-- top vertex
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = height
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = (i - 1) / numSegments

		--- first cone flat
		VBOData[#VBOData+1] = math.sin(math.pi*2* (i - 0) / numSegments) * radius -- X
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = -1*math.cos(math.pi*2* (i - 0) / numSegments) * radius -- Y
		VBOData[#VBOData+1] =(i - 0) / numSegments

		--- second cone flat
		VBOData[#VBOData+1] = math.sin(math.pi*2* (i - 1) / numSegments) * radius -- X
		VBOData[#VBOData+1] = 0
		VBOData[#VBOData+1] = -1*math.cos(math.pi*2* (i - 1) / numSegments) * radius -- Y
		VBOData[#VBOData+1] =(i - 1) / numSegments
	end

	coneVBO:Define(#VBOData/4,	{{id = 0, name = "localpos_progress", size = 4}})
	coneVBO:Upload(VBOData)
	return coneVBO, #VBOData/4
end

local function MakeCircleGL4(LuaShader, goodbye)
	local circleShader = nil
	local circleInstanceVBO = nil -- THIS IS THE MOST IMPORTANT THING!
	local circleSegments = 16

	local vsSrc = [[
	#version 420
	#line 10000

	layout (location = 0) in vec4 circlepointposition; // This contains the circle stuff
	layout (location = 1) in vec4 centerposxyz_radius; // this is what is passed for each instance
	layout (location = 2) in vec4 color;

	uniform vec4 circleuniforms; // none yet

	uniform sampler2D heightmapTex;

	out DataVS {
		vec4 worldPos; // pos and radius
		vec4 blendedcolor;
	};

	//__ENGINEUNIFORMBUFFERDEFS__

	#line 11000

	float heightAtWorldPos(vec2 w) { // this gets the world height
		vec2 uvhm =   vec2(clamp(w.x,8.0,mapSize.x-8.0),clamp(w.y,8.0, mapSize.y-8.0))/ mapSize.xy;
		return textureLod(heightmapTex, uvhm, 0.0).x;
	}

	void main() {
		// blend start to end on mod gf%15
		// float timemix = mod(timeInfo.x,10)*(0.1); //timeInfo.x contains gameframe
		vec4 circleWorldPos = centerposxyz_radius;
		circleWorldPos.xz = circlepointposition.xy * circleWorldPos.w +  circleWorldPos.xz;

		// get heightmap
		circleWorldPos.y = max(0.0,heightAtWorldPos(circleWorldPos.xz)) + 2.0; // add 2.0, and make sure its > 0

		// dump to FS
		worldPos = circleWorldPos;
		blendedcolor = color; // just dumping this
		gl_Position = cameraViewProj * vec4(circleWorldPos.xyz, 1.0);
	}
	]]

	local fsSrc =  [[
	#version 330

	#extension GL_ARB_uniform_buffer_object : require
	#extension GL_ARB_shading_language_420pack: require

	#line 20000

	uniform vec4 circleuniforms;

	uniform sampler2D heightmapTex;

	//__ENGINEUNIFORMBUFFERDEFS__

	in DataVS {
		vec4 worldPos; // w = range
		vec4 blendedcolor;
	};

	out vec4 fragColor;

	void main() {
		fragColor.rgba = blendedcolor.rgba;
		//fragColor.a *= sin((worldPos.x+worldPos.z)*0.12	 - timeInfo.z*0.033415); // stippling or whatever you want
	}
	]]

	local engineUniformBufferDefs = LuaShader.GetEngineUniformBufferDefs()
	circleShader =  LuaShader(
	{
		vertex = vsSrc:gsub("//__ENGINEUNIFORMBUFFERDEFS__", engineUniformBufferDefs),
		fragment = fsSrc:gsub("//__ENGINEUNIFORMBUFFERDEFS__", engineUniformBufferDefs),
		--geometry = gsSrc, no geom shader for now
		uniformInt = {
			heightmapTex = 0, -- but not really needed here
		},
		uniformFloat = {
			circleuniforms = {1,1,1,1}, -- for uniform floats, 4 of them
		},
	},
		"AI DBG circle shader GL4" -- the human name
	)
	shaderCompiled = circleShader:Initialize()
	if not shaderCompiled then goodbye("Failed to compile AI DBG circle shader") end
	local circleVBO,numVertices = makeCircleVBO(circleSegments + 1)
	local circleInstanceVBOLayout = {
		{id = 1, name = 'centerposxyz_radius', size = 4}, -- the start pos + radius
		{id = 2, name = 'color', size = 4}, --- color
	}
	circleInstanceVBO = makeInstanceVBOTable(circleInstanceVBOLayout, 128, "AI DBG thr VBO")
	circleInstanceVBO.numVertices = numVertices
	circleInstanceVBO.vertexVBO = circleVBO
	circleInstanceVBO.primitiveType = GL.TRIANGLE_FAN -- ugh forgot this one
	circleInstanceVBO.VAO = makeVAOandAttach(
		circleInstanceVBO.vertexVBO,
		circleInstanceVBO.instanceVBO
	)
	return circleInstanceVBO, circleShader
end

local function MakeConeGL4(LuaShader, goodbye)
	local circleShader = nil
	local circleInstanceVBO = nil -- THIS IS THE MOST IMPORTANT THING!
	local circleSegments = 16
	local coneHeight = 8

	local vsSrc = [[
	#version 420
	#line 10000

	layout (location = 0) in vec4 circlepointposition; // This contains the circle stuff
	layout (location = 1) in vec4 centerposxyz_radius; // this is what is passed for each instance
	layout (location = 2) in vec4 color;

	uniform vec4 circleuniforms; // none yet

	uniform sampler2D heightmapTex;

	out DataVS {
		vec4 worldPos; // pos and radius
		flat vec4 blendedcolor;
	};

	//__ENGINEUNIFORMBUFFERDEFS__

	#line 11000

	vec4 lightDir = normalize(vec4(0.3, -0.3, 0.7, 0));  // vertex to light-source direction in camera space

	float heightAtWorldPos(vec2 w) { // this gets the world height
		vec2 uvhm =   vec2(clamp(w.x,8.0,mapSize.x-8.0),clamp(w.y,8.0, mapSize.y-8.0))/ mapSize.xy;
		return textureLod(heightmapTex, uvhm, 0.0).x;
	}

	void main() {
		// blend start to end on mod gf%15
		// float timemix = mod(timeInfo.x,10)*(0.1); //timeInfo.x contains gameframe
		vec4 circleWorldPos = centerposxyz_radius;
		circleWorldPos.xyz = circlepointposition.xyz * circleWorldPos.w +  circleWorldPos.xyz;

		// get heightmap
		circleWorldPos.y += max(0.0,heightAtWorldPos(circleWorldPos.xz)) + 1.0; // add 1.0, and make sure its > 0

		// shading
		vec4 normalCameraSpace = normalize(cameraView * vec4(circlepointposition.xyz, 0));  // hack, rough representation of normal
		float cosTheta = max(dot(normalCameraSpace, lightDir), 0.1);  // clamp(dot, 0, 1)
		vec4 diffuse = vec4(color.xyz * cosTheta, color.a);

		// dump to FS
		worldPos = circleWorldPos;
		blendedcolor = diffuse; // just dumping this
		gl_Position = cameraViewProj * vec4(circleWorldPos.xyz, 1.0);
	}
	]]

	local fsSrc =  [[
	#version 330

	#extension GL_ARB_uniform_buffer_object : require
	#extension GL_ARB_shading_language_420pack: require

	#line 20000

	uniform vec4 circleuniforms;

	uniform sampler2D heightmapTex;

	//__ENGINEUNIFORMBUFFERDEFS__

	in DataVS {
		vec4 worldPos; // w = range
		flat vec4 blendedcolor;
	};

	out vec4 fragColor;

	void main() {
		fragColor.rgba = blendedcolor.rgba;
		//fragColor.a *= sin((worldPos.x+worldPos.z)*0.12	 - timeInfo.z*0.033415); // stippling or whatever you want
	}
	]]

	local engineUniformBufferDefs = LuaShader.GetEngineUniformBufferDefs()
	circleShader =  LuaShader(
	{
		vertex = vsSrc:gsub("//__ENGINEUNIFORMBUFFERDEFS__", engineUniformBufferDefs),
		fragment = fsSrc:gsub("//__ENGINEUNIFORMBUFFERDEFS__", engineUniformBufferDefs),
		--geometry = gsSrc, no geom shader for now
		uniformInt = {
			heightmapTex = 0, -- but not really needed here
		},
		uniformFloat = {
			circleuniforms = {1,1,1,1}, -- for uniform floats, 4 of them
		},
	},
		"AI DBG cone shader GL4" -- the human name
	)
	shaderCompiled = circleShader:Initialize()
	if not shaderCompiled then goodbye("Failed to compile AI DBG cone shader") end
	local circleVBO,numVertices = makeConeVBO(circleSegments + 1, coneHeight)
	local circleInstanceVBOLayout = {
		{id = 1, name = 'centerposxyz_radius', size = 4}, -- the start pos + radius
		{id = 2, name = 'color', size = 4}, --- color
	}
	circleInstanceVBO = makeInstanceVBOTable(circleInstanceVBOLayout, 128, "AI DBG mrk VBO")
	circleInstanceVBO.numVertices = numVertices
	circleInstanceVBO.vertexVBO = circleVBO
	circleInstanceVBO.primitiveType = GL.TRIANGLES
	circleInstanceVBO.VAO = makeVAOandAttach(
		circleInstanceVBO.vertexVBO,
		circleInstanceVBO.instanceVBO
	)
	return circleInstanceVBO, circleShader
end

local function InitGL4()
	Spring.Echo("Initializing GL4")

	local goodbye = function(reason)
		Spring.Echo("AI DBG widget exiting with reason: "..reason)
		gadgetHandler:RemoveGadget()
	end

	local luaShaderDir = "LuaUI/Widgets/Include/"
	local LuaShader = VFS.Include(luaShaderDir.."LuaShader.lua")
	VFS.Include(luaShaderDir.."instancevbotable.lua")

	circleInstanceVBOthr, circleShaderThr = MakeCircleGL4(LuaShader, goodbye)
	circleInstanceVBOmrk, circleShaderMrk = MakeConeGL4(LuaShader, goodbye)
	isGL4Way = true
end
----  END GL4 THINGS --------------

local aiData = {
	map = {},
	width = 0,
	height = 0,
	size = 0,  -- tile size
	div = 1.0,
	base = 1.0,
	isDraw = false,
	isPrint = false,
	mapChanged = 0,  -- frame of last change

	marks = {},
	marksChanged = 0,  -- marks size before last change
}

local mapWidth  = Game.mapSizeX
local mapHeight = Game.mapSizeZ

local function HandleAiDbgEvent(cmd, teamID, dataStr)
-- 	Spring.Echo("cmd: " .. cmd .. " | teamID: " .. tostring(teamID) .. " | dataStr: '" .. dataStr .. "'")
	local cmdThrData  = "ai_thr_data:"
	local cmdBlkData  = "ai_blk_data:"
	local cmdThrSize  = "ai_thr_size:"
	local cmdThrDiv   = "ai_thr_div:"
	local cmdThrDraw  = "ai_thr_draw:"
	local cmdThrPrint = "ai_thr_print:"
	local cmdMrkAdd   = "ai_mrk_add:"
	local cmdMrkClear = "ai_mrk_clear:"

	if dataStr:sub(1, #cmdThrData) == cmdThrData then
		-- "ai_thr_data:<float1><float2><float3>..."
		local mapStr = dataStr:sub(#cmdThrData + 1)
		aiData.map = VFS.UnpackF32(mapStr, 1, aiData.width * aiData.height)
		aiData.mapChanged = Spring.GetGameFrame()
		--Spring.Echo("aiData.mapChanged", Spring.GetGameFrame())
	elseif dataStr:sub(1, #cmdBlkData) == cmdBlkData then
		-- "ai_blk_data:<byte1><byte2><byte3>..."
		local mapStr = dataStr:sub(#cmdBlkData + 1)
		aiData.map = VFS.UnpackU8(mapStr, 1, aiData.width * aiData.height)
		aiData.mapChanged = Spring.GetGameFrame()
		--Spring.Echo("aiData.mapChanged", Spring.GetGameFrame())
	elseif dataStr:sub(1, #cmdThrSize) == cmdThrSize then
		-- "ai_thr_size:<square_size> <threat_base>"
		local sb = dataStr:sub(cmdThrSize:len() + 1)
		local slash = sb:find(" ", 1, true)
		if not slash then return end
		local ss = tonumber(sb:sub(1, slash - 1))
		local tb = tonumber(sb:sub(slash + 1))
		if not ss or not tb then return end
		aiData.base = tb
		aiData.size = ss
		aiData.width = mapWidth / aiData.size
		aiData.height = mapHeight / aiData.size
	elseif dataStr:sub(1, #cmdThrDiv) == cmdThrDiv then
		-- "ai_thr_div:<divider>"
		aiData.div = tonumber(dataStr:sub(#cmdThrDiv + 1))
	elseif dataStr:sub(1, #cmdThrDraw) == cmdThrDraw then
		-- "ai_thr_draw:"
		aiData.isDraw = not aiData.isDraw
	elseif dataStr:sub(1, #cmdThrPrint) == cmdThrPrint then
		-- "ai_thr_print:"
		aiData.isPrint = not aiData.isPrint
	elseif dataStr:sub(1, #cmdMrkAdd) == cmdMrkAdd then
		-- "ai_mrk_add:<pos.x> <pos.z> <radius> <color.r> <color.g> <color.b> <color.a> <text>"
		local markStr = dataStr:sub(#cmdMrkAdd + 1)
		local mark = {}
		for m in markStr:gmatch("%S+") do
			mark[#mark + 1] = m + 0.0;
		end
		aiData.marks[#aiData.marks + 1] = mark;
	elseif dataStr:sub(1, #cmdMrkClear) == cmdMrkClear then
		-- "ai_mrk_clear:"
		aiData.marks = {}
	end
end

local spGetGroundHeight  = Spring.GetGroundHeight

local lastupdateframe = 0

local function drawThrOldWay()
	local width = aiData.width
	local height = aiData.height
	local size = aiData.size
	local div = aiData.div
	local base = aiData.base
	local threatMap = aiData.map

	if aiData.isDraw then
		for x = 1, width do
			px = (x - 1) * size
			for z = 0, height - 1 do
				value = threatMap[z * width + x] - base
				draw = (value ~= 0)
				local mycolor = {0.0, 0.0, 0.0, 0.6} -- alpha defined here!

				if value > 0 then
					pz = z * size
					value = value / div
					mycolor[1] = value
				elseif value < 0 then
					pz = z * size
					value = -value / div
					mycolor[3] = value
				end
				if draw then
					-- could create DrawList and update it on aiData.mapChanged. But old way will be deprecated anyway
					gl.Color(mycolor)
					gl.DrawGroundQuad(px, pz, px + size, pz + size)
				end
			end
		end
	end

	if aiData.isPrint then
		local halfSize = size / 2;
--		local cx, cy, cz = Spring.GetCameraDirection()
--		local dir = ((math.atan2(cx, cz) / math.pi) + 1) * 180

		for x = 1, width do
			px = (x - 1) * size + halfSize
			for z = 0, height - 1 do
				value = threatMap[z * width + x]
				if value ~= base then
					pz = z * size + halfSize

					local py = spGetGroundHeight(px, pz)
					if Spring.IsSphereInView(px, py, pz, size) then
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
	end

	gl.Color(1,1,1,1)
end

local function drawMrkOldWay()
	if #aiData.marks > 0 then
		for k, v in pairs(aiData.marks) do
			local halfSize = v[3]
			gl.Color(v[4], v[5], v[6], v[7])
			gl.DrawGroundQuad(v[1] - halfSize, v[2] - halfSize, v[1] + halfSize, v[2] + halfSize)
			if v[8] then
				local py = spGetGroundHeight(v[1], v[2])
				if Spring.IsSphereInView(v[1], py, v[2], halfSize) then
					if py < 0 then py = 0 end
					gl.PushMatrix()
					gl.Translate(v[1], py, v[2])
					gl.Rotate(-90, 1, 0, 0)
					gl.Text(("%i"):format(v[8]), 0.0, 0.0, 10, "cno")
					gl.PopMatrix()
				end
			end
		end
	end

	gl.Color(1,1,1,1)
end

local function drawThrGL4Way()
	local width = aiData.width
	local height = aiData.height
	local size = aiData.size
	local div = aiData.div
	local base = aiData.base
	local threatMap = aiData.map

	if aiData.isDraw then
		if aiData.mapChanged > lastupdateframe then
			lastupdateframe = Spring.GetGameFrame()
-- 			Spring.Echo("mapChanged updated", lastupdateframe, aiData.mapChanged)

			clearInstanceTable(circleInstanceVBOthr) -- remove all our previous geometry from buffer
			for x = 1, width do
				px = (x - 1) * size
				for z = 0, height - 1 do
					value = threatMap[z * width + x] - base
					draw = (value ~= 0)
					local mycolor = {0.0, 0.0, 0.0, 0.6} -- alpha defined here!

					if value > 0 then
						pz = z * size
						value = value / div
						mycolor[1] = value
					elseif value < 0 then
						pz = z * size
						value = -value / div
						mycolor[3] = value
					end
					if draw then
						pushElementInstance( -- pushElementInstance(iT,thisInstance, instanceID, updateExisting, noUpload, unitID)
							circleInstanceVBOthr, -- the buffer to push into
							{ -- the data per instance
								px + size/2, 0, (z * size) + size/2, size/2, -- in vec4 centerposxyz_radius;
								mycolor[1],mycolor[2],mycolor[3],mycolor[4], -- in vec4 color;
							},
							nil, -- no instance key given, if given, it will update existing instance with same key instead of new instance
							true, -- updateExisting
							true) -- noUpload = true, we will upload whole buffer to gpu after we are done filling it
					end
				end
			end
			uploadAllElements(circleInstanceVBOthr) -- upload everything if noUpload was used
		end

		-- This part is the drawing part, the whole pushElementInstance only needs to be called every time the
		gl.DepthTest(false) -- so that it doesnt get hidden under terrain
		gl.Texture(0, "$heightmap")
		circleShaderThr:Activate()
-- 		Spring.Echo("Drawing AI DBG circleInstanceVBOthr", circleInstanceVBOthr.usedElements)
		drawInstanceVBO(circleInstanceVBOthr)
		circleShaderThr:Deactivate()
		gl.Texture(0, false)
		gl.DepthTest(true)
	end

	if aiData.isPrint then
		local halfSize = size / 2;
--		local cx, cy, cz = Spring.GetCameraDirection()
--		local dir = ((math.atan2(cx, cz) / math.pi) + 1) * 180

		for x = 1, width do
			px = (x - 1) * size + halfSize
			for z = 0, height - 1 do
				value = threatMap[z * width + x]
				if value ~= base then
					pz = z * size + halfSize

					local py = spGetGroundHeight(px, pz)
					if Spring.IsSphereInView(px, py, pz, size) then
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
	end
end

local function drawMrkGL4Way()
	if aiData.marksChanged ~= #aiData.marks then
		if #aiData.marks == 0 then
			clearInstanceTable(circleInstanceVBOmrk) -- remove all our previous geometry from buffer
		else
			local num = #aiData.marks - aiData.marksChanged
			-- "ai_mrk_add:<pos.x> <pos.z> <radius> <color.r> <color.g> <color.b> <color.a> <text>"
			for i = 1, num do
				local v = aiData.marks[aiData.marksChanged + i]
				pushElementInstance( -- pushElementInstance(iT,thisInstance, instanceID, updateExisting, noUpload, unitID)
					circleInstanceVBOmrk, -- the buffer to push into
					{ -- the data per instance
						v[1], 0, v[2], v[3], -- in vec4 centerposxyz_radius;
						v[4], v[5], v[6], v[7], -- in vec4 color;
					},
					nil, -- no instance key given, if given, it will update existing instance with same key instead of new instance
					true, -- updateExisting
					true) -- noUpload = true, we will upload whole buffer to gpu after we are done filling it
			end
		end
		uploadAllElements(circleInstanceVBOmrk) -- upload everything if noUpload was used
		aiData.marksChanged = #aiData.marks
	end

	if #aiData.marks > 0 then
		-- This part is the drawing part, the whole pushElementInstance only needs to be called every time the
		gl.DepthMask(false)  -- transparent objects look better without depthmask, though shading is wrong
		gl.DepthTest(true)
		gl.Texture(0, "$heightmap")
		circleShaderMrk:Activate()
-- 		Spring.Echo("Drawing AI DBG circleInstanceVBOmrk", circleInstanceVBOmrk.usedElements)
-- 		gl.Blending("alpha")
-- 		gl.Culling(GL.FRONT)
-- 		drawInstanceVBO(circleInstanceVBOmrk)
		gl.Culling(GL.BACK)
		drawInstanceVBO(circleInstanceVBOmrk)
-- 		gl.Blending(false)
		circleShaderMrk:Deactivate()
		gl.Texture(0, false)
		gl.Culling(false)
		gl.DepthTest(false)
		gl.DepthMask(true)

		for k, v in pairs(aiData.marks) do
			if v[8] then
				local py = spGetGroundHeight(v[1], v[2])
				if Spring.IsSphereInView(v[1], py, v[2], v[3]) then
					if py < 0 then py = 0 end
					gl.PushMatrix()
					gl.Translate(v[1], py, v[2])
					gl.Rotate(-90, 1, 0, 0)
					gl.Text(("%i"):format(v[8]), 0.0, 0.0, 10, "cno")
					gl.PopMatrix()
				end
			end
		end
	end
end

function gadget:DrawWorldPreUnit()
--	Spring.Echo(aiData.map[0 * width + 1])
--	Spring.Echo(aiData.map[(height - 1) * width + width])

	if isGL4Way then
		drawThrGL4Way()
	else
		drawThrOldWay()
	end
end

function gadget:DrawWorld()
	if isGL4Way then
		drawMrkGL4Way()
	else
		drawMrkOldWay()
	end
end

function gadget:Initialize()
	Spring.Echo("Initialize AI DBG")
	gadgetHandler:AddSyncAction("AiDbgEvent", HandleAiDbgEvent)
	InitGL4()
end

function gadget:Shutdown()
	Spring.Echo("Shutdown AI DBG")
	gadgetHandler:RemoveSyncAction("AiDbgEvent")
end

end
