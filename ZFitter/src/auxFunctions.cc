#include "../interface/auxFunctions.hh"
#include <iostream>

std::string energyBranchNameFromInvMassName(std::string invMass_var)
{
	std::string energyBranchName = "";
	if(invMass_var == "invMass_ECAL_ele") energyBranchName = "LepGood_ecalEnergy";
	else if(invMass_var == "mZ1") energyBranchName = "energyFromPt";
	else if(invMass_var == "invMass_rawSC") energyBranchName = "LepGood_superCluster_rawEnergy";
	else {
		std::cerr << "Energy branch name not define for invariant mass branch: " << invMass_var << std::endl;
		exit(1);
	}
	return energyBranchName;
}
