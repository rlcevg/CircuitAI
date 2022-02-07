function gadget:GetInfo() return {
	name    = "Chokepointer",
	desc    = "BWEM based chokepoint detector",
	author  = "rlcevg",
	date    = "Feb, 2022",
	license = "GNU GPL, v2 or later",
	layer   = -1337,
	enabled = true,
} end

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
if (gadgetHandler:IsSyncedCode()) then  -- Synced
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
--------------------------------------------------------------------------------
else  -- Unsynced
--------------------------------------------------------------------------------
--------------------------------------------------------------------------------

local altitude_scale = 8  -- multiplier, inherited from BWEM and 8x8 pixels
local granularity = Game.squareSize * 8  -- minimum granularity is 16 elmos, related to slopeMap
local walkGran = granularity / 16
local sectorXSize = Game.mapSizeX / granularity
local sectorZSize = Game.mapSizeZ / granularity

local areas_merge_size = 20  -- was 80
local areas_merge_altitude = 20  -- was 80 = 10 * altitude_scale
local area_min_miniTiles = 32  -- was 64
local lake_max_miniTiles = 8  -- was 64

local NODE_END1 = 1
local NODE_MIDDLE = 2
local NODE_END2 = 3
-- Result
local MiniTiles = {}
local Areas = {}
local RawFrontier = {}
local ChokePointsMatrix = {}


local function calcWalkable(p1, p2, maxSlope, minWaterDepth, maxWaterDepth)
	for i = 0, walkGran - 1 do
		for j = 0, walkGran - 1 do
			local x = p1 * granularity + i * 16
			local z = p2 * granularity + j * 16
			local _, _, _, slope = Spring.GetGroundNormal(x, z)
			if slope > maxSlope then
				return false
			end
			local y = Spring.GetGroundHeight(x, z)
			if y > -minWaterDepth or y < -maxWaterDepth then
				return false
			end
		end
	end
	return true
end

function AnalyzeMap(unitDefName)
	collectgarbage("collect")

	local ud = UnitDefNames[unitDefName]
	local md = ud.moveDef
	local maxSlope = md.maxSlope
	local minWaterDepth = ud.minWaterDepth
	local maxWaterDepth = ud.maxWaterDepth
	MiniTiles = {}
	for i = 0, sectorXSize - 1 do
		MiniTiles[i] = {}
		for j = 0, sectorZSize - 1 do
			MiniTiles[i][j] = {
				walkable = calcWalkable(i, j, maxSlope, minWaterDepth, maxWaterDepth),
				altitude = 0,
				areaId = 0
			}
		end
	end
	map_AreaPair_counter = {}

	ComputeAltitude()
	ComputeAreas()
	Graph_CreateChokePoints()

	map_AreaPair_counter = nil

	collectgarbage("collect")
end

-------------------------------------------------------------------------------
-- Helpers
-------------------------------------------------------------------------------

local function GetChokePoints(a, b)
	if a > b then a, b = b, a end
	return ChokePointsMatrix[b][a];
end

local function TileToPos(p)
	local x = p[1] * granularity + granularity / 2
	local z = p[2] * granularity + granularity / 2
	return {x, Spring.GetGroundHeight(x, z) + 10, z}
end

local function GetMiniTile(p)
	return MiniTiles[p[1]][p[2]]
end

local function isWalkable(p)
	return GetMiniTile(p).walkable
end

local function isAltitudeMissing(p)
	return GetMiniTile(p).altitude <= 0
end

local function setAltitude(p, altitude)
	GetMiniTile(p).altitude = altitude
	return altitude
end

local function length2D(x, y)
	return math.sqrt(x * x + y * y)
end

local function isValid(p)
	return p[1] >= 0 and p[1] < sectorXSize and p[2] >= 0 and p[2] < sectorZSize
end

local function addPos(p, delta)
	return {p[1] + delta[1], p[2] + delta[2]}
end

local function ArrayRemove(t, fnKeep)
	local j, n = 1, #t;

	for i=1,n do
		if (fnKeep(t, i, j)) then
			-- Move i's kept value to j's position, if it's not already there.
			if (i ~= j) then
				t[j] = t[i];
				t[i] = nil;
			end
			j = j + 1; -- Increment position of where we'll place the next kept value.
		else
			t[i] = nil;
		end
	end

	return t;
end

-------------------------------------------------------------------------------
-- ComputeAltitude
-------------------------------------------------------------------------------

local function seaSide(p)
	if isWalkable(p) then
		return false
	end

	local offsets = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}}
	for i, delta in ipairs(offsets) do
		local np = addPos(p, delta)
		if isValid(np) and isWalkable(np) then
			return true
		end
	end

	return false
end

local function floodFill(DeltasByAscendingAltitude, ActiveSeaSideList)
	local seaSideSize = #ActiveSeaSideList
	collectgarbage("collect")
	for i, delta_altitude in ipairs(DeltasByAscendingAltitude) do
		local d = {delta_altitude.dx, delta_altitude.dz}
		local altitude = delta_altitude.altitude
		for j = 1, seaSideSize do
			local Current = ActiveSeaSideList[j]
			if altitude - Current.lastAltitudeGenerated >= 2 * altitude_scale then
				-- optimization : once a seaside miniTile verifies this condition,
				-- we can throw it away as it will not generate min altitudes anymore
				ActiveSeaSideList[j] = ActiveSeaSideList[seaSideSize]
				j = j - 1
				seaSideSize = seaSideSize - 1
				if seaSideSize == 0 then
					return
				end
			else
				local offsets = {
					{d[1], d[2]}, {-d[1], d[2]}, {d[1], -d[2]}, {-d[1], -d[2]},
					{d[2], d[1]}, {-d[2], d[1]}, {d[2], -d[1]}, {-d[2], -d[1]}
				}
				for k, delta in ipairs(offsets) do
					local w = addPos(Current.origin, delta)
					if isValid(w) and isAltitudeMissing(w) and isWalkable(w) then
						Current.lastAltitudeGenerated = setAltitude(w, altitude)
					end
				end
			end
		end
	end
	collectgarbage("collect")
end

function ComputeAltitude(unitDefName)
	-- 1) Fill in and sort DeltasByAscendingAltitude
	local range = math.floor(math.max(sectorXSize, sectorZSize) / 2 + 3)

	local DeltasByAscendingAltitude = {}
	for dz = 0, range do
		for dx = dz, range do  -- Only consider 1/8 of possible deltas. Other ones obtained by symmetry.
			if dx or dz then
				DeltasByAscendingAltitude[#DeltasByAscendingAltitude + 1] = {
					dx = dx, dz = dz,
					altitude = length2D(dx, dz) * altitude_scale
				}
			end
		end
	end

	table.sort(DeltasByAscendingAltitude, function (a, b) return a.altitude < b.altitude end)

	-- 2) Fill in ActiveSeaSideList, which basically contains all the seaside miniTiles (from which altitudes are to be computed)
	--    It also includes extra border-miniTiles which are considered as seaside miniTiles too.
	local ActiveSeaSideList = {}
	for z = -1, sectorZSize do
		for x = -1, sectorXSize do
			local w = {x, z}
			if (not isValid(w)) or seaSide(w) then
				ActiveSeaSideList[#ActiveSeaSideList + 1] = {
					origin = w, lastAltitudeGenerated = 0
				}
			end
		end
	end

	-- 3) Dijkstra's algorithm
	floodFill(DeltasByAscendingAltitude, ActiveSeaSideList)
end

-------------------------------------------------------------------------------
-- ComputeAreas
-------------------------------------------------------------------------------

local function SortMiniTiles()
	local MiniTilesByDescendingAltitude = {}
	for z = 0, sectorZSize - 1 do
		for x = 0, sectorXSize - 1 do
			local miniTile = MiniTiles[x][z]
			if miniTile.areaId <= 0 and miniTile.altitude > 0 then
				MiniTilesByDescendingAltitude[#MiniTilesByDescendingAltitude + 1] = {x, z}
			end
		end
	end

	table.sort(MiniTilesByDescendingAltitude, function (a, b)
		return GetMiniTile(a).altitude > GetMiniTile(b).altitude
	end)

	return MiniTilesByDescendingAltitude;
end

local function findNeighboringAreas(p)
	local result = {0, 0}  -- pair of areaId

	local offsets = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}}
	for i, delta in ipairs(offsets) do
		local np = addPos(p, delta)
		if isValid(np) then
			local areaId = GetMiniTile(np).areaId
			if areaId > 0 then
				if result[1] == 0 then
					result[1] = areaId
				elseif result[1] ~= areaId and (result[2] == 0 or areaId < result[2]) then
					result[2] = areaId
				end
			end
		end
	end

	return result
end

local function addToTempArea(TempArea, miniTile)
	TempArea.size = TempArea.size + 1
	miniTile.areaId = TempArea.id
end

local function createTempArea(TempAreaList, p, miniTile)
	local areaId = #TempAreaList + 1
	TempAreaList[areaId] = {
		valid = true,
		id = areaId,
		top = p,
		highestAltitude = miniTile.altitude,
		size = 0
	}
	addToTempArea(TempAreaList[areaId], miniTile)
end

local function ReplaceAreaIds(p, newAreaId)
	local Origin = GetMiniTile(p)
	local oldAreaId = Origin.areaId
	Origin.areaId = newAreaId

	local offsets = {{0, -1}, {-1, 0}, {1, 0}, {0, 1}}
	local ToSearch = {p}
	local ToSearchSize = #ToSearch
	while (ToSearchSize > 0) do
		local current = ToSearch[ToSearchSize]

		ToSearchSize = ToSearchSize - 1
		for i, delta in ipairs(offsets) do
			local nxt = addPos(current, delta)
			if isValid(nxt) then
				local nxtTile = GetMiniTile(nxt)
				if nxtTile.areaId == oldAreaId then
					ToSearchSize = ToSearchSize + 1
					ToSearch[ToSearchSize] = nxt
					nxtTile.areaId = newAreaId
				end
			end
		end
	end

	-- also replaces references of oldAreaId by newAreaId in m_RawFrontier:
	if newAreaId > 0 then
		for i, v in ipairs(RawFrontier) do
			if v.areaId1 == oldAreaId then v.areaId1 = newAreaId end
			if v.areaId2 == oldAreaId then v.areaId2 = newAreaId end
		end
	else
		ArrayRemove(RawFrontier, function(t, i, j)
			local v = t[i]  -- Return true to keep the value, or false to discard it.
			return v.areaId1 ~= oldAreaId and v.areaId2 ~= oldAreaId
		end)
	end
end

local function mergeAreas(TempArea, Absorbed)
	TempArea.size = TempArea.size + Absorbed.size;
	Absorbed.valid = false;
end

local map_AreaPair_counter = {}
local function chooseNeighboringArea(a, b)
	if a > b then a, b = b, a end
	local pairA = map_AreaPair_counter[a]
	if pairA == nil then
		pairA = {}
		map_AreaPair_counter[a] = pairA
	end
	local count = pairA[b] or 0
	pairA[b] = count + 1
	if count % 2 == 0 then
		return a
	end
	return b
end

local function ComputeTempAreas(MiniTilesByDescendingAltitude)
	local TempAreaList = {}
	RawFrontier = {}
	for i, p in ipairs(MiniTilesByDescendingAltitude) do
		local miniTile = GetMiniTile(p)
		local neighboringAreas = findNeighboringAreas(p)
		if neighboringAreas[1] == 0 then  -- no neighboring area : creates of a new area
			createTempArea(TempAreaList, p, miniTile)
		elseif neighboringAreas[2] == 0 then  -- one neighboring area : adds cur to the existing area
			addToTempArea(TempAreaList[neighboringAreas[1]], miniTile)
		else  -- two neighboring areas : adds cur to one of them  &  possible merging
			local smaller = neighboringAreas[1]
			local bigger = neighboringAreas[2]
			if TempAreaList[smaller].size > TempAreaList[bigger].size then
				smaller, bigger = bigger, smaller
			end

			-- Condition for the neighboring areas to merge:
			if TempAreaList[smaller].size < areas_merge_size
				or TempAreaList[smaller].highestAltitude < areas_merge_altitude
				or miniTile.altitude / TempAreaList[bigger].highestAltitude >= 0.90
				or miniTile.altitude / TempAreaList[smaller].highestAltitude >= 0.90
				-- or check how close to base position
			then
				-- adds cur to the absorbing area:
				addToTempArea(TempAreaList[bigger], miniTile)

				-- merges the two neighboring areas:
				ReplaceAreaIds(TempAreaList[smaller].top, bigger);
				mergeAreas(TempAreaList[bigger], TempAreaList[smaller])
			else  -- no merge : cur starts or continues the frontier between the two neighboring areas
				-- adds cur to the chosen Area:
				addToTempArea(TempAreaList[chooseNeighboringArea(smaller, bigger)], miniTile)
				RawFrontier[#RawFrontier + 1] = {
					areaId1 = neighboringAreas[1],
					areaId2 = neighboringAreas[2],
					pos = p
				}
			end
		end
	end

	-- Remove from the frontier obsolete positions
	ArrayRemove(RawFrontier, function(t, i, j)
		local v = t[i]  -- Return true to keep the value, or false to discard it.
		return v.areaId1 ~= v.areaId2
	end)

	return TempAreaList;
end

local function Graph_CreateAreas(AreasList)
	Areas = {}
	for id = 1, #AreasList do
		Areas[#Areas + 1] = {
			id = id,
			top = AreasList[id][1],
			miniTiles = AreasList[id][2],
			chokePointsByArea = {}
		}
	end
end

local function CreateAreas(TempAreaList)
	local AreasList = {}

	local newAreaId = 1
	local newTinyAreaId = -2

	for i, TempArea in ipairs(TempAreaList) do
		if TempArea.valid then
			if TempArea.size >= area_min_miniTiles then
				if newAreaId ~= TempArea.id then
					ReplaceAreaIds(TempArea.top, newAreaId)
				end

				AreasList[#AreasList + 1] = {TempArea.top, TempArea.size}
				newAreaId = newAreaId + 1
			else
				ReplaceAreaIds(TempArea.top, newTinyAreaId)
				newTinyAreaId = newTinyAreaId - 1
			end
		end
	end

	Graph_CreateAreas(AreasList)
end

-- Assigns MiniTile::m_areaId for each miniTile having AreaIdMissing()
-- Areas are computed using MiniTile::Altitude() information only.
-- The miniTiles are considered successively in descending order of their Altitude().
-- Each of them either:
--   - involves the creation of a new area.
--   - is added to some existing neighbouring area.
--   - makes two neighbouring areas merge together.
function ComputeAreas()
	local MiniTilesByDescendingAltitude = SortMiniTiles()
	local TempAreaList = ComputeTempAreas(MiniTilesByDescendingAltitude)
	CreateAreas(TempAreaList)
--	SetAreaIdInTiles()  -- When using few tile resolutions adapt mini data to large tiles
end

-------------------------------------------------------------------------------
-- Graph_CreateChokePoints
-------------------------------------------------------------------------------

local function queenWiseDist(p, w)
	local dx = p[1] - w[1]
	local dz = p[2] - w[2]
	return math.max(math.abs(dx), math.abs(dz))
end

local function initNodes(choke)
	choke.nodes[NODE_END1] = choke.geometry[1]
	choke.nodes[NODE_END2] = choke.geometry[#choke.geometry]

	local i = math.floor(#choke.geometry / 2) + 1
	while (i > 1 and GetMiniTile(choke.geometry[i - 1]).altitude > GetMiniTile(choke.geometry[i]).altitude) do
		i = i - 1
	end
	while (i < #choke.geometry and GetMiniTile(choke.geometry[i + 1]).altitude > GetMiniTile(choke.geometry[i]).altitude) do
		i = i + 1
	end
	choke.nodes[NODE_MIDDLE] = choke.geometry[i]

	return choke
end

function Graph_CreateChokePoints()
	local newIndex = 1  -- ChokePoint::index

	-- 1) Size the matrix
	ChokePointsMatrix = {}
	local RawFrontierByAreaPair = {}
	for a = 1, #Areas do
		ChokePointsMatrix[a] = {}
		RawFrontierByAreaPair[a] = {}
		for b = 1, a - 1 do
			ChokePointsMatrix[a][b] = {}  -- triangular matrix
			RawFrontierByAreaPair[a][b] = {}
		end
	end

	-- 2) Dispatch the global raw frontier between all the relevant pairs of Areas:
	for i, raw in ipairs(RawFrontier) do
		local a = raw.areaId1
		local b = raw.areaId2
		if a > b then a, b = b, a end

		local rawAB = RawFrontierByAreaPair[b][a]
		rawAB[#rawAB + 1] = raw.pos
	end

	-- 3) For each pair of Areas (A, B):
	local cluster_min_dist = math.sqrt(lake_max_miniTiles)
	for a, pairA in ipairs(RawFrontierByAreaPair) do
		for b, RawFrontierAB in ipairs(pairA) do
			-- Because our dispatching preserved order,
			-- and because Map::m_RawFrontier was populated in descending order of the altitude (see Map::ComputeAreas),
			-- we know that RawFrontierAB is also ordered the same way

			-- 3.1) Use that information to efficiently cluster RawFrontierAB in one or several chokepoints.
			--    Each cluster will be populated starting with the center of a chokepoint (max altitude)
			--    and finishing with the ends (min altitude).
			local Clusters = {}
			for j, w in ipairs(RawFrontierAB) do
				local added = false
				for k, Cluster in ipairs(Clusters) do
					local distToFront = queenWiseDist(Cluster[1], w)
					local distToBack = queenWiseDist(Cluster[#Cluster], w)
					if math.min(distToFront, distToBack) <= cluster_min_dist then
						if distToFront < distToBack then
							table.insert(Cluster, 1, w)
						else
							Cluster[#Cluster + 1] = w
						end

						added = true
						break
					end
				end

				if not added then
					Clusters[#Clusters + 1] = {w}
				end
			end

			-- 3.2) Create one Chokepoint for each cluster:
			for j, Cluster in ipairs(Clusters) do
				local choke = {
					id = newIndex,
					areaId1 = a,
					areaId2 = b,
					geometry = Cluster,
					nodes = {}
				}
				table.insert(GetChokePoints(a, b), initNodes(choke))
				newIndex = newIndex + 1
			end
		end
	end

	-- 4) Create one Chokepoint for each pair of blocked areas, for each blocking Neutral:
	-- NOTE: BlockingNeutrals were cut off

	-- 5) Set the references to the freshly created Chokepoints:
	for a = 1, #Areas do
		for b = 1, a - 1 do
			local chokes = GetChokePoints(a, b)
			if #chokes > 0 then
				Areas[a].chokePointsByArea[b] = chokes
				Areas[b].chokePointsByArea[a] = chokes
			end
		end
	end
end


local isAnalyzed = false

function gadget:Initialize()
	Spring.Echo("The generator's gone.")
	AnalyzeMap("armcom")
	Spring.Echo("Any way we can fix it?")
end

function gadget:Shutdown()
	Spring.Echo("It's GONE, MacReady.")
end

-- WARNING: usage example
function gadget:GameFrame(n)
	if isAnalyzed then
		return
	end
	isAnalyzed = true
	for a = 1, #Areas do
		for b = 1, a - 1 do
			local chokes = GetChokePoints(a, b)
			for i, cp in ipairs(chokes) do
				local m = TileToPos(cp.nodes[NODE_MIDDLE])
				Spring.MarkerAddPoint(m[1], m[2], m[3], "choke" .. a .. ":" .. b)
				local e1 = TileToPos(cp.nodes[NODE_END1])
				local e2 = TileToPos(cp.nodes[NODE_END2])
				Spring.MarkerAddLine(e1[1], e1[2], e1[3], e2[1], e2[2], e2[3])
			end
		end
	end
end

end
