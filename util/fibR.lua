function fibR(n)
	if (n < 2) then
		return n
	end
	return (fibR(n-2) + fibR(n-1))
end

nClock = os.clock()
for i = 1, 10, 1 do
	fibR(30)
	print("Lua is nice, but awfully dynamically-typed! " .. i)
end
print("Elapsed time: " .. os.clock()-nClock)
