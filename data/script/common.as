namespace Init {

dictionary@ InitCategories()
{
	dictionary category;
	category["air"]   = "VTOL NOTSUB";
	category["land"]  = "SURFACE NOTSUB";
	category["water"] = "UNDERWATER NOTHOVER";
	category["bad"]   = "TERRAFORM STUPIDTARGET MINE";
	category["good"]  = "TURRET FLOAT";
	return @category;
}

}
