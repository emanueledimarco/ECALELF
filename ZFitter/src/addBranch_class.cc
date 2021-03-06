#include "../interface/addBranch_class.hh"
#include "../interface/ElectronCategory_class.hh"
#include <TTreeFormula.h>
#include <TLorentzVector.h>
#include <TGraph.h>
#include <iostream>
#include "TH2F.h"

//#define DEBUG
//#define NOFRIEND

addBranch_class::addBranch_class(void):
	scaler(NULL)
{
}

addBranch_class::~addBranch_class(void)
{
}

/** \param originalChain standard ntuple
 *  \param treename name of the new tree (not important)
 *  \param BranchName invMassSigma or iSMEle (important, define which new branch you want)
 */
TTree *addBranch_class::AddBranch(TChain* originalChain, TString treename, TString BranchName, bool fastLoop, bool isMC, TString energyBranchName)
{
	if(BranchName.Contains("invMassSigma")) return AddBranch_invMassSigma(originalChain, treename, BranchName, fastLoop, isMC);
	if(BranchName.CompareTo("iSM") == 0)       return AddBranch_iSM(originalChain, treename, BranchName, fastLoop);
	if(BranchName.CompareTo("smearerCat") == 0)       return AddBranch_smearerCat(originalChain, treename, isMC);
	if(BranchName.CompareTo("R9Eleprime") == 0)       return AddBranch_R9Eleprime(originalChain, treename, isMC); //after r9 transformation
	if(BranchName.Contains("ZPt"))   return AddBranch_ZPt(originalChain, treename, BranchName.ReplaceAll("ZPt_", ""), fastLoop);
	if(BranchName.CompareTo("LTweight") == 0) return AddBranch_LTweight(originalChain, treename);
	if(BranchName.Contains("EleIDSF")) return AddBranch_EleIDSF(originalChain, treename, BranchName, energyBranchName, isMC);
	std::cerr << "[ERROR] Request to add branch " << BranchName << " but not defined" << std::endl;
	return NULL;
}

TTree* addBranch_class::AddBranch_EleIDSF(TChain* originalChain, TString treename, TString branchname, TString energyBranchName, bool isMC)
{
	Float_t etaSCEle[] = {0,0,0}; 
	Float_t energy[] = {0,0,0}; 
	originalChain->SetBranchStatus("*", 0);
	if(isMC) {
		originalChain->SetBranchStatus("etaSCEle", 1);
		originalChain->SetBranchStatus(energyBranchName, 1);
		originalChain->SetBranchAddress("etaSCEle", etaSCEle);
		originalChain->SetBranchAddress(energyBranchName, energy);
	}

	std::cout << gDirectory->GetName() << std::endl;
	TTree* newtree = new TTree(treename, treename);
	newtree->SetDirectory(gDirectory);
	Float_t EleIDSF[] = {1,1,1};
	newtree->Branch(branchname, EleIDSF, branchname + "[3]/F");

	TH2F * sf = NULL;

	sf = (TH2F *) TFile::Open("/eos/project/c/cms-ecal-calibration/data/EleIDSF/" + branchname + ".root")->Get("EGamma_SF2D");
	if (sf == NULL) {
		std::cerr << "[ERROR] Request to add branch " << branchname << " but does not contain valid ID" << std::endl;
		return NULL;
	}

	Float_t max_pT = sf->GetYaxis()->GetBinCenter(sf->GetYaxis()->GetLast());
	Float_t min_pT = sf->GetYaxis()->GetBinCenter(sf->GetYaxis()->GetFirst());
	Float_t max_eta = sf->GetXaxis()->GetBinCenter(sf->GetXaxis()->GetLast());
	Float_t min_eta = sf->GetXaxis()->GetBinCenter(sf->GetXaxis()->GetFirst());

	Long64_t nentries = originalChain->GetEntries();
	TString oldfilename = "";
	for(Long64_t ientry = 0; ientry < nentries; ientry++) {
		originalChain->GetEntry(ientry);

		for(Int_t i = 0; i < 3; ++i) {
			EleIDSF[i] = 0;
			if(etaSCEle[i] == -999 || energy[i] == -999) continue;
			Float_t pT = energy[i]/cosh(etaSCEle[i]);
			//if outside TH2F then move values to highest bin
			pT = min(pT, max_pT);
			pT = max(pT, min_pT);
			etaSCEle[i] = min(etaSCEle[i], max_eta);
			etaSCEle[i] = max(etaSCEle[i], min_eta);

			Int_t bin = sf->FindBin(etaSCEle[i], pT);
			EleIDSF[i] = sf->GetBinContent(bin);
			if(EleIDSF[i] == 0)
				std::cout << Form("[DEBUG] etaSCEle[%d]=%.1f pT=%.1f energy=%.1f", i, etaSCEle[i], pT, energy[i]) << std::endl;
		}
		newtree->Fill();
	}
	originalChain->SetBranchStatus("*", 1);
	originalChain->ResetBranchAddresses();
	return newtree;
}
TTree* addBranch_class::AddBranch_LTweight(TChain* originalChain, TString treename)
{

	originalChain->SetBranchStatus("*", 0);

	TTree* newtree = new TTree(treename, treename);
	Float_t LTweight = 0.;
	newtree->Branch("LTweight", &LTweight, "LTweight/F");

	Float_t lumi = 1000; // to normalize to 1fb-1
	Long64_t Nevents[10] = {
		5061547, 1915515, 2853483, 2987343,
		1960045, 1310896, 2280265, 1194817,
		1023888, 933956
	};

	Float_t LTweights[10] = { // cross-sections in pb
		8.670e+02, 1.345e+02, 1.599e+02, 2.295e+02,
		1.654e+02, 4.896e+01, 9.401e+01, 3.588e+00,
		2.012e-01, 8.329e-03
	}; //After Filter
	//Float_t LTweights[10] = {0.4977, 0.4072, 0.4118, 0.3982, 0.3213, 0.1674, 0.1403, 0.0679, 0.0588, 0.0633};

	Long64_t nentries = originalChain->GetEntries();
	TString oldfilename = "";
	for(Long64_t ientry = 0; ientry < nentries; ientry++) {
		originalChain->GetEntry(ientry);
		TString filename = originalChain->GetFile()->GetName();
		if(filename != oldfilename) {
			oldfilename = filename;
			if(!filename.Contains("LT")) {
				LTweight = 1.;
			} else if(filename.Contains("LT_5To75")) {
				LTweight = LTweights[0] / Nevents[0];
			} else if(filename.Contains("LT_75To80")) {
				LTweight = LTweights[1] / Nevents[1];
			} else if(filename.Contains("LT_80To85")) {
				LTweight = LTweights[2] / Nevents[2];
			} else if(filename.Contains("LT_85To90")) {
				LTweight = LTweights[3] / Nevents[3];
			} else if(filename.Contains("LT_90To95")) {
				LTweight = LTweights[4] / Nevents[4];
			} else if(filename.Contains("LT_95To100")) {
				LTweight = LTweights[5] / Nevents[5];
			} else if(filename.Contains("LT_100To200")) {
				LTweight = LTweights[6] / Nevents[6];
			} else if(filename.Contains("LT_200To400")) {
				LTweight = LTweights[7] / Nevents[7];
			} else if(filename.Contains("LT_400To800")) {
				LTweight = LTweights[8] / Nevents[8];
			} else if(filename.Contains("LT_800To2000")) {
				LTweight = LTweights[9] / Nevents[9];
			}
			if(LTweight != 1) LTweight *= lumi;
			std::cout << LTweight << "\t" << filename << std::endl;

		}
		newtree->Fill();
	}

	return newtree;

}


TTree* addBranch_class::AddBranch_R9Eleprime(TChain* originalChain, TString treename, bool isMC)
{
	//to have a branch with r9prime
	//From the original tree take R9 and eta
	///\todo put the filename and the graph names in the .dat file and activate the parsing as for the pileupHist
	Float_t         R9Ele[3];
	Float_t         etaEle[3];

	originalChain->SetBranchStatus("*", 0);
	originalChain->SetBranchStatus("etaEle", 1);
	originalChain->SetBranchStatus("R9Ele", 1);
	originalChain->SetBranchAddress("etaEle", etaEle);
	originalChain->SetBranchAddress("R9Ele", R9Ele);

	//2015
	//  TFile* f = TFile::Open("~gfasanel/public/R9_transformation/transformation.root");
	//  //root file with r9 transformation:
	//  TGraph* gR9EB = (TGraph*) f->Get("transformR90");
	//  TGraph* gR9EE = (TGraph*) f->Get("transformR91");
	//2016
	//TFile* f = TFile::Open("~gfasanel/public/R9_transformation/transformation_80X_v1.root");
	//TGraph* gR9EB = (TGraph*) f->Get("TGraphtransffull5x5R9EB");
	//TGraph* gR9EE = (TGraph*) f->Get("TGraphtransffull5x5R9EE");
	//f->Close();
	//2017
	TFile* f = TFile::Open("/eos/project/c/cms-ecal-calibration/data/R9transf/transformation_Moriond17_v1.root");
	TGraph* gR9EB = (TGraph*) f->Get("transffull5x5R9EB");
	TGraph* gR9EE = (TGraph*) f->Get("transffull5x5R9EE");
	f->Close();

	TTree* newtree = new TTree(treename, treename);
	Float_t R9Eleprime[3];
	newtree->Branch("R9Eleprime", R9Eleprime, "R9Eleprime[3]/F");

	Long64_t nentries = originalChain->GetEntries();
	for(Long64_t ientry = 0; ientry < nentries; ientry++) {
		originalChain->GetEntry(ientry);
		if(isMC) {
			//electron 0
			if(abs(etaEle[0]) < 1.4442) { //barrel
				R9Eleprime[0] = gR9EB->Eval(R9Ele[0]);
			} else if(abs(etaEle[0]) > 1.566 && abs(etaEle[0]) < 2.5 && R9Ele[0]>0.8) { //endcap
				R9Eleprime[0] = gR9EE->Eval(R9Ele[0]);
			} else {
				R9Eleprime[0] = R9Ele[0];
			}

			//electron 1
			if(abs(etaEle[1]) < 1.4442) { //barrel
				R9Eleprime[1] = gR9EB->Eval(R9Ele[1]);
			} else if(abs(etaEle[1]) > 1.566 && abs(etaEle[1]) < 2.5 && R9Ele[1]>0.8) { //endcap
				R9Eleprime[1] = gR9EE->Eval(R9Ele[1]);
			} else {
				R9Eleprime[1] = R9Ele[1];
			}
		} else { //no transformation needed for data
			//std::cout<<"R9 in data is not transformed ->R9Eleprime==R9Ele"<<std::endl;
			R9Eleprime[0] = R9Ele[0];
			R9Eleprime[1] = R9Ele[1];
		}

		R9Eleprime[2] = -999; //not used the third electron
		newtree->Fill();
	}

	originalChain->SetBranchStatus("*", 1);
	originalChain->ResetBranchAddresses();
	return newtree;
}

TTree* addBranch_class::AddBranch_ZPt(TChain* originalChain, TString treename, TString energyBranchName, bool fastLoop)
{
	//sanity checks

	TTree* newtree = new TTree(treename, treename);

	//add pt branches
	Float_t         phiEle[2];
	Float_t         etaEle[2];
	Float_t         energyEle[2];
	Float_t         corrEle[2] = {1., 1.};
	Float_t         ZPt, ZPta;
	TLorentzVector ele1, ele2;

	originalChain->SetBranchAddress("etaEle", etaEle);
	originalChain->SetBranchAddress("phiEle", phiEle);
	originalChain->SetBranchAddress(energyBranchName, energyEle);

	if(fastLoop) {
		originalChain->SetBranchStatus("*", 0);
		originalChain->SetBranchStatus("etaEle", 1);
		originalChain->SetBranchStatus("phiEle", 1);
		originalChain->SetBranchStatus(energyBranchName, 1);
		if(originalChain->GetBranch("scaleEle") != NULL) {
			std::cout << "[STATUS] Adding electron energy correction branch from friend " << originalChain->GetTitle() << std::endl;
			originalChain->SetBranchAddress("scaleEle", corrEle);
		}
	}

	newtree->Branch("ZPt_" + energyBranchName, &ZPt, "ZPt/F");
	//px = pt*cosphi; py = pt*sinphi; pz = pt*sinh(eta)
	//p^2 = E^2 - m^2 = pt^2*(1+sinh^2(eta)) = pt^2*(cosh^2(eta))
	float mass = 0.; //0.000511;
	Long64_t nentries = originalChain->GetEntries();
	for(Long64_t ientry = 0; ientry < nentries; ientry++) {
		originalChain->GetEntry(ientry);
		float regrCorr_fra_pt0 = sqrt(((energyEle[0] * energyEle[0]) - mass * mass) / (1 + sinh(etaEle[0]) * sinh(etaEle[0])));
		float regrCorr_fra_pt1 = sqrt(((energyEle[1] * energyEle[1]) - mass * mass) / (1 + sinh(etaEle[1]) * sinh(etaEle[1])));
		ZPt =
		    TMath::Sqrt(pow(regrCorr_fra_pt0 * TMath::Sin(phiEle[0]) + regrCorr_fra_pt1 * TMath::Sin(phiEle[1]), 2) + pow(regrCorr_fra_pt0 * TMath::Cos(phiEle[0]) + regrCorr_fra_pt1 * TMath::Cos(phiEle[1]), 2));

		ele1.SetPtEtaPhiE(energyEle[0] / cosh(etaEle[0]), etaEle[0], phiEle[0], energyEle[0]);
		ele2.SetPtEtaPhiE(energyEle[1] / cosh(etaEle[1]), etaEle[1], phiEle[1], energyEle[1]);
		ZPta = (ele1 + ele2).Pt();
		if(fabs(ZPt - ZPta) > 0.001) {
			std::cerr << "[ERROR] ZPt not well calculated" << ZPt << "\t" << ZPta << std::endl;
			exit(1);
		}

		newtree->Fill();
	}

	originalChain->SetBranchStatus("*", 1);
	originalChain->ResetBranchAddresses();
	return newtree;
}


TTree* addBranch_class::AddBranch_invMassSigma(TChain* originalChain, TString treename, TString invMassSigmaName, bool fastLoop, bool isMC)
{
	if(scaler == NULL) {
		std::cerr << "[ERROR] EnergyScaleCorrection class not initialized" << std::endl;
		exit(1);
	}

	//sanity checks
	TString etaEleBranchName = "etaEle", phiEleBranchName = "phiEle", etaSCEleBranchName = "etaSCEle";
	TString R9EleBranchName = "R9Ele";
	TString energyBranchName, invMassBranchName, sigmaEnergyBranchName;

	TTree *newtree = new TTree(treename, treename);

	if(newtree == NULL) {
		std::cerr << "[ERROR] New tree for branch " << invMassSigmaName << " is NULL" << std::endl;
		exit(1);
	}
	//delete branches
	//TBranch *b = originalChain-> GetBranch("name of branch");
	//originalChain->GetListOfBranches()->Remove(b);

	Int_t runNumber;
	//add pt branches
	Float_t         phiEle[2];
	Float_t         etaEle[2];
	Float_t         energyEle[2];
	Float_t         sigmaEnergyEle[2];
	Float_t         invMass;
	Float_t         corrEle[2] = {1., 1.};
	//Float_t         smearEle[2]={1,1};

	Float_t etaSCEle_[2];
	Float_t R9Ele_[2];

	//TBranch         *smearEle_b;
	if(invMassSigmaName == "invMassSigma_SC") {
		invMassBranchName = "invMass_SC";
		energyBranchName = "energySCEle";
		sigmaEnergyBranchName = "";
		std::cerr << "[ERROR] No energy error estimation for std. SC" << std::endl;
		exit(1);
	} else if(invMassSigmaName == "invMassSigma_SC_regrCorr_ele") {
		invMassBranchName = "invMass_SC_regrCorr_ele";
		energyBranchName = "energySCEle_regrCorr_ele";
		sigmaEnergyBranchName = "energySigmaSCEle_regrCorr_ele";
	} else if(invMassSigmaName == "invMassSigma_SC_regrCorr_pho") {
		energyBranchName = "energySCEle_regrCorr_pho";
		invMassBranchName = "invMass_SC_regrCorr_pho";
		sigmaEnergyBranchName = "energySigmaSCEle_regrCorr_pho";
	} else if(invMassSigmaName == "invMassSigma_regrCorr_fra") {
		invMassBranchName = "invMass_regrCorr_fra";
		energyBranchName = "energyEle_regrCorr_fra";
		sigmaEnergyBranchName = "energysigmaEle_regrCorr_fra";
	} else if(invMassSigmaName.Contains("SemiPar")) {
		//    invMassSigmaRelBranchName=invMassSigmaName;
		//    invMassSigmaRelBranchName.ReplaceAll("Sigma","SigmaRel");

		invMassBranchName = invMassSigmaName;
		invMassBranchName.ReplaceAll("Sigma", "");
		energyBranchName = invMassBranchName;
		energyBranchName.ReplaceAll("invMass_SC", "energySCEle");
		sigmaEnergyBranchName = energyBranchName;
		sigmaEnergyBranchName.ReplaceAll("energySCEle", "energySigmaSCEle");
	} else {
		std::cerr << "[ERROR] Energy branch and invMass branch for invMassSigma = " << invMassSigmaName << " not defined" << std::endl;
		exit(1);
	}

	if(fastLoop) {
		originalChain->SetBranchStatus("*", 0);
		originalChain->SetBranchStatus("runNumber", 1);
		originalChain->SetBranchStatus(etaEleBranchName, 1);
		originalChain->SetBranchStatus(etaSCEleBranchName, 1);
		originalChain->SetBranchStatus(phiEleBranchName, 1);
		originalChain->SetBranchStatus(etaSCEleBranchName, 1);
		originalChain->SetBranchStatus(R9EleBranchName, 1);
		if(originalChain->GetBranch("scaleEle") != NULL) {
			std::cout << "[STATUS] Adding electron energy correction branch from friend " << originalChain->GetTitle() << std::endl;
			originalChain->SetBranchAddress("scaleEle", corrEle);
		}

		//originalChain->SetBranchStatus(smearEleBranchName, true);
		originalChain->SetBranchStatus(energyBranchName, 1);
		originalChain->SetBranchStatus(sigmaEnergyBranchName, 1);
		originalChain->SetBranchStatus(invMassBranchName, 1);

	}
	originalChain->SetBranchAddress("runNumber", &runNumber);
	if(originalChain->SetBranchAddress(etaEleBranchName, etaEle) < 0) {
		std::cerr << "[ERROR] Branch etaEle not defined" << std::endl;
		exit(1);
	}
	if(originalChain->SetBranchAddress(phiEleBranchName, phiEle) < 0) exit(1);
	//if(originalChain->SetBranchAddress(smearEleBranchName, smearEle) < 0){
	//std::cerr << "[ERROR] Branch smearEle not defined" << std::endl;
	//exit(1);
	//}
	originalChain->SetBranchAddress(etaSCEleBranchName, etaSCEle_);
	originalChain->SetBranchAddress(R9EleBranchName, R9Ele_);

	if(originalChain->SetBranchAddress(energyBranchName, energyEle) < 0 ) exit(1);
	originalChain->SetBranchAddress(sigmaEnergyBranchName, sigmaEnergyEle);
	originalChain->SetBranchAddress(invMassBranchName, &invMass);

	Float_t invMassSigma; //, invMassSigmaRel;
	newtree->Branch(invMassSigmaName, &invMassSigma, invMassSigmaName + "/F");
	//  newtree->Branch(invMassSigmaRelBranchName, &invMassSigmaRel, invMassSigmaRelBranchName+"/F");

	for(Long64_t ientry = 0; ientry < originalChain->GetEntriesFast(); ientry++) {

		originalChain->GetEntry(ientry);
		float smearEle_[2];
		smearEle_[0] = scaler->getSmearingRho(runNumber, energyEle[0], fabs(etaSCEle_[0]) < 1.4442,
		                                      R9Ele_[0], etaSCEle_[0]);
		smearEle_[1] = scaler->getSmearingRho(runNumber, energyEle[1], fabs(etaSCEle_[1]) < 1.4442,
		                                      R9Ele_[1], etaSCEle_[1]);
		if(smearEle_[0] == 0 || smearEle_[1] == 0) {
			std::cerr << "[ERROR] Smearing = 0 " << "\t" << smearEle_[0] << "\t" << smearEle_[1] << std::endl;
			std::cout << "E_0: " << runNumber << "\t" << energyEle[0] << "\t"
			          << etaSCEle_[0] << "\t" << R9Ele_[0] << "\t" << etaEle[0] << "\t" << smearEle_[0] << "\t" << invMass << "\t" << corrEle[0] << "\t" << invMassSigma << "\t" << sigmaEnergyEle[0] << std::endl;
			std::cout << "E_1: " << runNumber << "\t" << energyEle[1] << "\t"
			          << etaSCEle_[1] << "\t" << R9Ele_[1] << "\t" << etaEle[1] << "\t" << smearEle_[1] << "\t" << sigmaEnergyEle[1] << "\t" << corrEle[1] << std::endl;
			exit(1);
		}
		if(isMC) invMass *= sqrt(///\todo it should not be getSmearingSigma, but getSmearing with already the Gaussian. to be implemented into EnergyScaleCorrection_class.cc
			                        scaler->getSmearingSigma(runNumber, energyEle[0], fabs(etaSCEle_[0]) < 1.4442,
			                                R9Ele_[0], etaSCEle_[0], 0, 0)
			                        *
			                        scaler->getSmearingSigma(runNumber, energyEle[1], fabs(etaSCEle_[1]) < 1.4442,
			                                R9Ele_[1], etaSCEle_[1], 0, 0)
			                    );

		invMass *= sqrt(corrEle[0] * corrEle[1]);

		invMassSigma = 0.5 * invMass *
		               sqrt(
		                   sigmaEnergyEle[0] / (energyEle[0] * corrEle[0]) * sigmaEnergyEle[0] / (energyEle[0] * corrEle[0]) +
		                   sigmaEnergyEle[1] / (energyEle[1] * corrEle[1]) * sigmaEnergyEle[1] / (energyEle[1] * corrEle[1]) +
		                   smearEle_[0] * smearEle_[0] + smearEle_[1] * smearEle_[1]
		               );
		//    invMassSigmaRel = invMassSigma/invMass;
#ifdef DEBUG
		E_0: 203777     55.7019 0.35335 0.958164        0.39268 inf     86.3919 1       inf     0.00636875
		E_1: 203777     33.6127 - 0.309422       0.831792        - 0.270196       inf     0.00910235      1
		if(ientry < 10) {
			std::cout << "E_0: " << runNumber << "\t" << energyEle[0] << "\t"
			          << etaSCEle_[0] << "\t" << R9Ele_[0] << "\t" << etaEle[0] << "\t" << smearEle_[0] << "\t" << invMass << "\t" << corrEle[0] << "\t" << invMassSigma << "\t" << sigmaEnergyEle[0] << std::endl;
			std::cout << "E_1: " << runNumber << "\t" << energyEle[1] << "\t"
			          << etaSCEle_[1] << "\t" << R9Ele_[1] << "\t" << etaEle[1] << "\t" << smearEle_[1] << "\t" << sigmaEnergyEle[1] << "\t" << corrEle[1] << std::endl;
		}
#endif
		newtree->Fill();
	}

	if(fastLoop)   originalChain->SetBranchStatus("*", 1);
	originalChain->ResetBranchAddresses();
	return newtree;
}



TTree* addBranch_class::AddBranch_iSM(TChain* originalChain, TString treename, TString iSMEleName, bool fastLoop)
{

	TString seedXSCEleBranchName = "xSeedSC", seedYSCEleBranchName = "ySeedSC";

	TTree *newtree = new TTree(treename, treename);

	Short_t       seedXSCEle_[2];
	Short_t       seedYSCEle_[2];


	if(fastLoop) {
		originalChain->SetBranchStatus("*", 0);
		originalChain->SetBranchStatus(seedXSCEleBranchName, 1);
		originalChain->SetBranchStatus(seedYSCEleBranchName, 1);
	}

	if(originalChain->SetBranchAddress(seedXSCEleBranchName, seedXSCEle_) < 0) {
		std::cerr << "[ERROR] Branch seedXSCEle not defined" << std::endl;
		exit(1);
	}

	if(originalChain->SetBranchAddress(seedYSCEleBranchName, seedYSCEle_) < 0) {
		std::cerr << "[ERROR] Branch seedYSCEle not defined" << std::endl;
		exit(1);
	}


	Int_t iSM_[2];
	newtree->Branch(iSMEleName, iSM_, iSMEleName + "[2]/I");

	for(Long64_t ientry = 0; ientry < originalChain->GetEntriesFast(); ientry++) {
		iSM_[0] = -1;
		iSM_[1] = -1;
		originalChain->GetEntry(ientry);
		if(seedXSCEle_[0] != 0) {
			if(seedXSCEle_[0] > 0) {
				// EB+
				iSM_[0] = (int)((seedYSCEle_[0] - 1)) / 20 + 1;
			} else {
				// EB-
				iSM_[0] = (int)((seedYSCEle_[0] - 1)) / 20 + 19;
			}
		}

		if(seedYSCEle_[1] != 0) {
			if(seedXSCEle_[1] > 0) {
				// EB+
				iSM_[1] = (int)((seedYSCEle_[1] - 1)) / 20 + 1;
			} else {
				// EB-
				iSM_[1] = (int)((seedYSCEle_[1] - 1)) / 20 + 19;
			}
		}
		if(ientry < 10) std::cout << seedXSCEle_[0] << "\t" << seedYSCEle_[0] << "\t" << iSM_[0] << std::endl;
		if(ientry < 10) std::cout << seedXSCEle_[1] << "\t" << seedYSCEle_[1] << "\t" << iSM_[1] << std::endl;
		if(seedXSCEle_[1] < 0 && iSM_[1] < 19) std::cout << seedXSCEle_[1] << "\t" << seedYSCEle_[1] << "\t" << iSM_[1] << std::endl;
		newtree->Fill();
	}

	if(fastLoop)   originalChain->SetBranchStatus("*", 1);
	originalChain->ResetBranchAddresses();
	return newtree;
}



// branch with the smearing category index
TTree* addBranch_class::AddBranch_smearerCat(TChain* originalChain, TString treename, bool isMC)
{

	ElectronCategory_class cutter;
	if(originalChain->GetBranch("scaleEle") != NULL) {
		cutter._corrEle = true;
		std::cout << "[INFO] Activating scaleEle for smearerCat" << std::endl;

	}
	TString oddString = "";

	//setting the new tree
	TTree *newtree = new TTree(treename, treename);
	Int_t  smearerCat[2];
	Char_t cat1[10];
	sprintf(cat1, "XX");
	newtree->Branch("smearerCat", smearerCat, "smearerCat[2]/I");
	newtree->Branch("catName", cat1, "catName/C");
	//  newtree->Branch("catName2", cat2, "catName2/C");

	/// \todo disable branches using cutter
	originalChain->SetBranchStatus("*", 0);
	//originalChain->SetBranchStatus("R9Eleprime",1);

	std::vector< std::pair<TTreeFormula*, TTreeFormula*> > catSelectors;
	for(std::vector<TString>::const_iterator region_ele1_itr = _regionList.begin();
	        region_ele1_itr != _regionList.end();
	        region_ele1_itr++) {

		// \todo activating branches // not efficient in this loop
		std::set<TString> branchNames = cutter.GetBranchNameNtuple(*region_ele1_itr);
		for(std::set<TString>::const_iterator itr = branchNames.begin();
		        itr != branchNames.end(); itr++) {
			std::cout << "Activating branches in addBranch_class.cc" << std::endl;
			std::cout << "Branch is " << *itr << std::endl;
			originalChain->SetBranchStatus(*itr, 1);
		}
		if(    cutter._corrEle == true) originalChain->SetBranchStatus("scaleEle", 1);


		for(std::vector<TString>::const_iterator region_ele2_itr = region_ele1_itr;
		        region_ele2_itr != _regionList.end();
		        region_ele2_itr++) {

			if(region_ele2_itr == region_ele1_itr) {
				TString region = *region_ele1_itr;
				region.ReplaceAll(_commonCut, ""); //remove the common Cut!
				TTreeFormula *selector = new TTreeFormula("selector-" + (region), cutter.GetCut(region + oddString, isMC), originalChain);
				catSelectors.push_back(std::pair<TTreeFormula*, TTreeFormula*>(selector, NULL));
				//selector->Print();
				std::cout << cutter.GetCut(region + oddString, isMC) << std::endl;
				//exit(0);
			} else {
				TString region1 = *region_ele1_itr;
				TString region2 = *region_ele2_itr;
				region1.ReplaceAll(_commonCut, "");
				region2.ReplaceAll(_commonCut, "");
				TTreeFormula *selector1 = new TTreeFormula("selector1-" + region1 + region2,
				        cutter.GetCut(region1 + oddString, isMC, 1) &&
				        cutter.GetCut(region2 + oddString, isMC, 2),
				        originalChain);
				TTreeFormula *selector2 = new TTreeFormula("selector2-" + region1 + region2,
				        cutter.GetCut(region1 + oddString, isMC, 2) &&
				        cutter.GetCut(region2 + oddString, isMC, 1),
				        originalChain);
				catSelectors.push_back(std::pair<TTreeFormula*, TTreeFormula*>(selector1, selector2));
				//selector1->Print();
				//	selector2->Print();
				//exit(0);
			}

		}
	}


	Long64_t entries = originalChain->GetEntries();
	originalChain->LoadTree(originalChain->GetEntryNumber(0));
	Long64_t treenumber = -1;

	std::cout << "[STATUS] Get smearerCat for tree: " << originalChain->GetTitle()
	          << "\t" << "with " << entries << " entries" << std::endl;
	std::cerr << "[00%]";

	for(Long64_t jentry = 0; jentry < entries; jentry++) {
		originalChain->GetEntry(jentry);
		if (originalChain->GetTreeNumber() != treenumber) {
			treenumber = originalChain->GetTreeNumber();
			for(std::vector< std::pair<TTreeFormula*, TTreeFormula*> >::const_iterator catSelector_itr = catSelectors.begin();
			        catSelector_itr != catSelectors.end();
			        catSelector_itr++) {

				catSelector_itr->first->UpdateFormulaLeaves();
				if(catSelector_itr->second != NULL)       catSelector_itr->second->UpdateFormulaLeaves();
			}
		}

		int evIndex = -1;
		bool _swap = false;
		for(std::vector< std::pair<TTreeFormula*, TTreeFormula*> >::const_iterator catSelector_itr = catSelectors.begin();
		        catSelector_itr != catSelectors.end() && evIndex < 0;
		        catSelector_itr++) {
			_swap = false;
			TTreeFormula *sel1 = catSelector_itr->first;
			TTreeFormula *sel2 = catSelector_itr->second;
			//if(sel1==NULL) continue; // is it possible?
			if(sel1->EvalInstance() == false) {
				if(sel2 == NULL || sel2->EvalInstance() == false) continue;
				else {
					_swap = true;
					//sprintf(cat1,"%s", sel2->GetName());
				}
			} //else sprintf(cat1,"%s", sel1->GetName());

			evIndex = catSelector_itr - catSelectors.begin();
		}

		smearerCat[0] = evIndex;
		smearerCat[1] = _swap ? 1 : 0;
		newtree->Fill();
		if(jentry % (entries / 100) == 0) std::cerr << "\b\b\b\b" << std::setw(2) << jentry / (entries / 100) << "%]";
	}
	std::cout << std::endl;

	//if(fastLoop)
	originalChain->SetBranchStatus("*", 1);
	originalChain->ResetBranchAddresses();
	return newtree;
}



#ifdef shervinM



TTree *addBranch_class::GetTreeDistIEtaDistiPhi(TChain *tree,  bool fastLoop, int xDist, int yDist, TString iEtaBranchName, TString iPhiBranchName)
{


	Int_t iEta;
	Int_t iPhi;
	TTree *newTree = new TTree("distIEtaIPhi", "");
	newTree->Branch("distIEta", &distIEta, "distIEta/I");
	newTree->Branch("distIPhi", &distIPhi, "distIPhi/I");

	if(fastLoop) {
		tree->SetBranchStatus("*", 0);
		tree->SetBranchStatus(iEtaBranchName, 1);
		tree->SetBranchStatus(iPhiBranchName, 1);
	}

	Int_t seedXSCEle[2];
	Int_t seedYSCEle[2];
	tree->SetBranchAddress(iEtaBranchName, seedXSCEle);
	tree->SetBranchAddress(iPhiBranchName, seedYSCEle);

	// loop over tree
	for(Long64_t ientry = 0; ientry < tree->GetEntriesFast(); ientry++) {
		tree->GetEntry(ientry);
		weight = GetWeight((int)nPU[0]); //only intime pu
		newTree->Fill();
	}


	if(fastLoop) tree->SetBranchStatus("*", 1);
	tree->ResetBranchAddresses();
	std::cout << "[WARNING] nPU > nPU max for " << warningCounter << " times" << std::endl;
	return newTree;
}




#endif

