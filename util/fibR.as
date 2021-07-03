int fibR(int n)
{
	if (n < 2) return n;
	return (fibR(n-2) + fibR(n-1));
}

void init()
{
	for (int i = 0; i < 10; ++i) {
		fibR(30);
		aiLog("AngelScript Rules! " + i);
	}
}
