/**\file
\brief 	General program for Z fit (monitoring, calibration validation)

The aim of the program is to provide a common interface to all the Z
fitting algorithms reading and combining in the proper way the
configuration files.

\todo
   - remove commonCut from category name and add it in the ZFit_class in order to not repeate the cut
   - make alpha fitting more generic (look for alphaName)
   - Implement the iterative Et dependent scale corrections

  # ZFitter options:
 \include ZFitter_options.txt

*/

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <boost/program_options.hpp>

#include <TChain.h>
#include <TStopwatch.h>
#include <TFriendElement.h>

/// @cond SHOW
/// \code
#include "../interface/anyVar_class.h"
#include "../interface/ZFit_class.hh"
#include "../interface/puWeights_class.hh"
#include "../interface/r9Weights_class.hh"
#include "../interface/ZPtWeights_class.hh"

#include "../interface/runDivide_class.hh"
#include "../interface/EnergyScaleCorrection_class.hh"
#include "../interface/addBranch_class.hh"

#include "../interface/RooSmearer.hh"

#include "../interface/nllProfile.hh"
#include "../interface/auxFunctions.hh"

#include "../../EOverPCalibration/interface/FastCalibratorEB.h"
#include "../../EOverPCalibration/interface/FastCalibratorEE.h"
#include "../../EOverPCalibration/interface/LaserMonitoringEoP.h"


/// \endcode
/// @endcond

#include <RooMinuit.h>
//#include <RooStats/UniformProposal.h>
//#include <RooStats/PdfProposal.h>
//#include <RooStats/SequentialProposal.h>
#include <RooGaussian.h>
#include <RooStats/MetropolisHastings.h>
#include "RooBinning.h"
//#include "../src/ShervinMinuit.cc"
//#include <RooStats/ProposalHelper.h>
#include <RooMultiVarGaussian.h>

#include "Math/Minimizer.h"

#include <TPRegexp.h>
#include <RooFormulaVar.h>
#include <RooPullVar.h>

#include <TMatrixDSym.h>

#include <TRandom3.h>
#include <queue>

#define profile

//#define DEBUG
#define smooth


//#include "../macro/loop.C" // a way to use compiled macros with ZFitter


///\endcond

//------------------------------------------------------------
/// type to hold TChain Pointers
typedef std::shared_ptr<TChain> pTChain_t;

//------------------------------------------------------------
/// map that associates the name of the tree and the pointer to the chain
typedef std::map< TString, pTChain_t > chain_map_t;

/** \brief map that associates the name of the tag to the chain_map_t
 *
 * the logic is better described in \ref validationConfig
 */
typedef std::map< TString, chain_map_t > tag_chain_map_t;

//------------------------------------------------------------
/** Function parsing the region files
 * \retval vector of strings, each string is the name of one region
 */
std::vector<TString> ReadRegionsFromFile(TString fileName)
{
	ifstream file(fileName);
	std::vector<TString> regions;
	TString region;

	while(file.peek() != EOF && file.good()) {
		if(file.peek() == 10) { // 10 = \n
			file.get();
			continue;
		}

		if(file.peek() == 35) { // 35 = #
			file.ignore(1000, 10); // ignore the rest of the line until \n
			continue;
		}

		file >> region;
		file.ignore(1000, 10); // ignore the rest of the line until \n
#ifdef DEBUG
		std::cout << "[DEBUG] Reading region: " << region << std::endl;
#endif
		regions.push_back(region);

	}
	return regions;
}

//------------------------------------------------------------
/**
 * This function reassociates the chains as friends of the "selected" tree.
 *
 * This function should be run when new chains or files are added to the tagChainMap.
 *
 */
void UpdateFriends(tag_chain_map_t& tagChainMap, TString regionsFileNameTag)
{
//void UpdateFriends(tag_chain_map_t& tagChainMap){
	// loop over all the tags
	for(tag_chain_map_t::const_iterator tag_chain_itr = tagChainMap.begin();
	        tag_chain_itr != tagChainMap.end();
	        tag_chain_itr++) {
		// take the selected tree of that tag
		TChain * chain = (tag_chain_itr->second.find("selected"))->second.get();

		// loop over all the trees
		for(chain_map_t::const_iterator chain_itr = tag_chain_itr->second.begin();
		        chain_itr != tag_chain_itr->second.end();
		        chain_itr++) {

			if(chain_itr->first != "selected") { //except the selected
				if(chain->GetFriend(chain_itr->first) == NULL) {
					std::cout << "[STATUS] Adding friend branch: " << chain_itr->first
					          << " to tag " << tag_chain_itr->first << std::endl;
					chain->AddFriend(chain_itr->second.get());
				} // already added
			}

			if(chain->GetEntries() != chain_itr->second->GetEntries()) {
				std::cerr << "[ERROR] Not the same number of events: " << chain->GetEntries() << "\t" << chain_itr->second->GetEntries() << std::endl;
				exit(1);
			}
		}
	}
	return;
}

//------------------------------------------------------------
void Dump(tag_chain_map_t& tagChainMap, TString tag = "s", Long64_t firstentry = 0)
{
	(tagChainMap[tag])["selected"]->Scan("etaEle:R9Ele:energySCEle_regrCorrSemiParV5_pho/cosh(etaSCEle):smearerCat:catName", "", "col=5:4:5:3:50", 5, firstentry);
}

//------------------------------------------------------------
/**
 * \param tagChainMap map of all the tags declared in the validation config file
 * \param tag name of the new \b tag created by the function, all the existent tags with name starting with
b tag are merged in the new \b tag
 *
 * A new tagChain with name=tag is added to the tagChainMap. All the tagChains with tag starting with \b tag are merged
 * After the merging the friend list is updated by \ref UpdateFriends
 */
void MergeSamples(tag_chain_map_t& tagChainMap, TString regionsFileNameTag, TString tag = "s")
{

	std::pair<TString, chain_map_t > pair_tmp_tag(tag, chain_map_t()); // make_pair not work with scram b
	tagChainMap.insert(pair_tmp_tag);

	//loop over all the tags
	for(tag_chain_map_t::const_iterator tag_chain_itr = tagChainMap.begin();
	        tag_chain_itr != tagChainMap.end();
	        tag_chain_itr++) {

		// consider tags matching the tag input parameter
		if(tag_chain_itr->first.CompareTo(tag) == 0 || !tag_chain_itr->first.Contains(tag)) continue; //do it for each sample

		// loop over all the trees
		for(chain_map_t::const_iterator chain_itr = tag_chain_itr->second.begin();
		        chain_itr != tag_chain_itr->second.end();
		        chain_itr++) {
			TString chainName = chain_itr->first;
			if(tagChainMap.count(chainName) == 0) { // create the new chain if does not exist for tag
				(tagChainMap[tag]).insert(make_pair(chainName, pTChain_t(new TChain(chainName))));
				(tagChainMap[tag])[chainName]->SetTitle(tag);
			}
			(tagChainMap[tag])[chainName]->Add(chain_itr->second.get());
			std::cout << tag << "\t" << tag_chain_itr->first << "\t" << chainName <<  "\t" << chain_itr->second.get() << "\t" << chain_itr->second->GetTitle() << std::endl;

		}
	}
	UpdateFriends(tagChainMap, regionsFileNameTag);
	return;
}


//------------------------------------------------------------
TString energyBranchNameFromInvMassName(TString invMass_var)
{
	return TString( (energyBranchNameFromInvMassName(std::string(invMass_var))).c_str());
}

//------------------------------------------------------------
int main(int argc, char **argv)
{
	TStopwatch myClock;
	TStopwatch globalClock;
	globalClock.Start();

	puWeights_class puWeights;
	std::string puBranchName = "nPU"; // it can be changed to nPV is the pileup hist file contains "nPV" in the name of the tree (chainName)
	std::cout << "============================== Z General Fitter" << std::endl;

	//------------------------------------------------------------
	// parsing input options to the program
	using namespace boost;
	namespace po = boost::program_options;
	unsigned int nEvents_runDivide = 100000;
	std::string chainFileListName;
	std::string regionsFileName;
	std::string runRangesFileName;
	std::vector<TString> dataPUFileNameVec, mcPUFileNameVec;
	std::string r9WeightFile;
	std::string ZPtWeightFile;
	std::string initFileName;
	//  bool savePUweightTree;
	std::string imgFormat, outDirFitResMC = "test/MC/fitres", outDirFitResData = "test/dato/fitres", outDirImgMC = "test/MC/img", outDirImgData = "test/dato/img", outDirTable = "test/dato/table", selection;
	TString eleID = "";
	//std::vector<std::string> signalFiles, bkgFiles, dataFiles;
	std::string commonCut;
	std::string corrEleFile, corrEleType;
	std::string smearEleFile, smearEleType;
	double smearingCBAlpha = 1, smearingCBPower = 5;
	std::string invMass_var;
	float invMass_min = 0, invMass_max = 0, invMass_binWidth = 0.250;
	int fit_type_value = 1;
	int signal_type_value = 0;
	unsigned long long int nToys = 0;
	float constTermToy = 0;
	unsigned long long int nEventsPerToy = 0;
	unsigned int nIter = 0;
	unsigned int nEventsMinDiag = 0;
	unsigned int nEventsMinOffDiag = 0;
	unsigned int nSmearToy = 1;

	int pdfSystWeightIndex = -1;
	std::string minimType;
	std::vector<std::string> branchList;
	float scale;
	unsigned int modulo;
//options for E/p
	std::string miscalibMap;
	bool isMiscalib;
	bool applyPcorr;
	bool applyEcorr;
	bool isSaveEPDistribution;
	bool isMCTruth;
	bool isEPselection;
	bool isPtCut;
	float PtMin;
	bool isfbrem;
	float fbremMax;
	bool isR9selection;
	float R9Min;
	float EPMin;
	int smoothCut;
	int miscalibMethod;
	std::string inputMomentumScale;
	std::string inputEnergyScale;
	std::string typeEB;
	std::string typeEE;
	std::string outputFile;
	int numberOfEvents;
	int useZ, useW;
	int useRawEnergy;
	int splitStat;
	int nLoops;
	bool isDeadTriggerTower;
	std::string inputFileDeadXtal;
	std::string EBEE;
	std::string EBEEpu;
	int evtsPerPoint;

	int useRegression;
	float yMIN;
	float yMAX;
	std::string dayMin;
	std::string dayMax;
	std::string dayZOOM;
	std::string LUMI;
	int EoPvsPU;

	//------------------------------ setting option categories
	po::options_description desc("Main options");
	po::options_description outputOption("Output options");
	po::options_description inputOption("Input options");
	po::options_description fitterOption("Z fitter options");
	po::options_description smearerOption("Z smearer options");
	po::options_description anyVarOption("anyVar options");
	po::options_description toyOption("toyMC options");
	po::options_description EoverPOption("EoverP options");
	po::options_description laserMonitoringEPOption("laser monitoring with E/p options");

	//po::options_description cmd_line_options;
	//cmd_line_options.add(desc).add(fitOption).add(smearOption);

	//------------------------------ adding options' description
	desc.add_options()
	("help,h", "Help message")
	("loop", "")

	("runDivide", "execute the run division")
	("nEvents_runDivide", po::value<unsigned int>(&nEvents_runDivide)->default_value(100000), "Minimum number of events in a run range")

	//
	("noPU", "")
	("savePUTreeWeight", "")
	//
	("corrEleFile", po::value<string>(&corrEleFile), "File with energy scale corrections")
	("corrEleType", po::value<string>(&corrEleType), "Correction type/step")
	("saveCorrEleTree", "")

	("smearEleFile", po::value<string>(&smearEleFile), "File with energy smearings")
	("smearEleType", po::value<string>(&smearEleType), "Correction type/step")
	("smearingCBAlpha", po::value<double>(&smearingCBAlpha), "Correction type/step")
	("smearingCBPower", po::value<double>(&smearingCBPower), "Correction type/step")
	//
	("r9WeightFile", po::value<string>(&r9WeightFile), "File with r9 photon-electron weights")
	("useR9weight", "use r9 photon-electron weights")
	("saveR9TreeWeight", "")
	("ZPtWeightFile", po::value<string>(&ZPtWeightFile), "File with ZPt weights")
	("useZPtweight", "use ZPt weights")
	("useFSRweight", "activate the FSR weight in MC")
	("useWEAKweight", "activate the WEAK interference weight in MC")
	("saveRootMacro", "")
	//
	("selection", po::value<string>(&selection)->default_value("loose25nsRun22016Moriond"), "") //"cutBasedElectronID-Spring15-25ns-V1-standalone-loose"
	("commonCut", po::value<string>(&commonCut)->default_value("isEle-Et_25"), "")
	("invMass_var", po::value<string>(&invMass_var)->default_value("invMass_ECAL_ele"), "")
	("invMass_min", po::value<float>(&invMass_min)->default_value(65.), "")
	("invMass_max", po::value<float>(&invMass_max)->default_value(115.), "")
	("invMass_binWidth", po::value<float>(&invMass_binWidth)->default_value(0.25), "Smearing binning")
	("isOddMC", "Activate if use only odd events in MC")
	("isOddData", "Activate if use only odd events in data")
	("scale", po::value<float>(&scale)->default_value(1.), "scale shift for tests")
	//
	("readDirect", "") //read correction directly from config file instead of passing as a command line arg
	//("addPtBranches", "")  //add new pt branches ( 3 by default, fra, ele, pho)
	("addBranch", po::value< std::vector<string> >(&branchList), "")
	("saveAddBranchTree", "")
	//    ("signal,s", po::value< std::vector <string> >(&signalFiles), "Signal file (can be called multiple times putting the files in a chain")
	//    ("bkg,b", po::value< std::vector <string> >(&bkgFiles), "Bkg file (can be called multiple times putting the files in a chain")
	//    ("data,d", po::value< std::vector <string> >(&dataFiles), "Data file (can be called multiple times putting the files in a chain")
	//("_ZFit_class", "call the Z fitter")
	//;
	//
	//  po::options_description fitOption("Z Fit options");
	//  po::options_description smearOption("Z smearing options");
	//  smearOption.add_options()
	;
	fitterOption.add_options()
	("zFit", "call the ZFit_class")
	("fit_type_value", po::value<int>(&fit_type_value)->default_value(1), "0=floating tails, 1=fixed tails")
	("signal_type_value", po::value<int>(&signal_type_value)->default_value(0), "0=BW+CB, 1=Cruijff")
	("forceNewFit", "refit MC also if fit exists")
	("updateOnly",  "do not fit data if fit exists")
	;
	smearerOption.add_options()
	("smearerFit",  "call the smearing")
	("smearerType", po::value<string>(&minimType)->default_value("profile"), "minimization algo")
	("onlyDiagonal", "if want to use only diagonal categories")
	("autoBin", "")
	("autoNsmear", "")
	("smearscan", "")
	("isDataSmeared", "")
	("plotOnly", "active if you don't want to do the smearing")
	("profileOnly", "")
	("numIter", po::value<unsigned int>(&nIter)->default_value(300), "number of MCMC steps")
	("nEventsMinDiag", po::value<unsigned int>(&nEventsMinDiag)->default_value(1000), "min num events in diagonal categories")
	("nEventsMinOffDiag", po::value<unsigned int>(&nEventsMinOffDiag)->default_value(2000), "min num events in off-diagonal categories")
	("onlyScale",    "fix the smearing to constant")
	("constTermFix", "constTerm not depending on Et")
	("alphaGoldFix", "alphaTerm for gold electrons fixed to the low eta region")
	("smearingEt", "alpha term depend on sqrt(Et) and not on sqrt(E)")
	("nSmearToy", po::value<unsigned int>(&nSmearToy)->default_value(0), "")
	("pdfSystWeightIndex", po::value<int>(&pdfSystWeightIndex)->default_value(-1), "Index of the weight to be used")
	;


	std::vector<std::string> branchListAny;
	branchListAny.push_back(invMass_var);
	anyVarOption.add_options()
		("anyVar", "call the anyVar_class")
		("anyVarBranches", po::value<std::vector<std::string> >(&branchListAny),"list of branches")
	;

	inputOption.add_options()
	("chainFileList,f", po::value< string >(&chainFileListName), "Configuration file with input file list")
	("regionsFile", po::value< string >(&regionsFileName), "Configuration file with regions")
	("runRangesFile", po::value< string >(&runRangesFileName), "Configuration file with run ranges")
	("initFile", po::value< string >(&initFileName), "Configuration file with init values of fit model")
	;
	outputOption.add_options()
	("imgFormat", po::value<string>(&imgFormat)->default_value("png"), "")
	("outDirFitResMC", po::value<string>(&outDirFitResMC), "")
	("outDirFitResData", po::value<string>(&outDirFitResData), "")
	("outDirImgMC", po::value<string>(&outDirImgMC), "")
	("outDirImgData", po::value<string>(&outDirImgData), "")
	//("outDirTable", po::value<string>(&outDirTable),"")
	;
	toyOption.add_options()
	("runToy", "")
	("nToys", po::value<unsigned long long int>(&nToys)->default_value(1000), "")
	("constTermToy", po::value<float>(&constTermToy)->default_value(0.01), "")
	("eventsPerToy", po::value<unsigned long long int>(&nEventsPerToy)->default_value(0), "=0: all events")
	("modulo", po::value<unsigned int>(&modulo)->default_value(1), "=1: no splitting of events")
	;
	laserMonitoringEPOption.add_options()
	("laserMonitoringEP", "call the laser monitoring with E/p")
	("yMIN", po::value<float>(&yMIN)->default_value(0.65), "y min")
	("yMAX", po::value<float>(&yMAX)->default_value(1.05), "y max")
	("EBEE", po::value<string>(&EBEE)->default_value("EB"), "barrel or endcap")
	("evtsPerPoint", po::value<int>(&evtsPerPoint)->default_value(1000), "events per point")
	("useRegression", po::value<int>(&useRegression)->default_value(0), "use regression")
	("dayMin", po::value<string>(&dayMin)->default_value("1-7-2015"), "day min")
	("dayMax", po::value<string>(&dayMax)->default_value("15-11-2015"), "day max")
	("dayZOOM", po::value<string>(&dayZOOM)->default_value("10-8-2015"), "day ZOOM")
	("LUMI", po::value<string>(&LUMI)->default_value("1.9"), "LUMI")
	("EoPvsPU", po::value<int>(&EoPvsPU)->default_value(0), "EoPvsPU")
	;
	EoverPOption.add_options()
	("EOverPCalib",  "call the E/p calibration")
	("doEB", "do EoP IC calibration for EB")
	("isMiscalib", po::value<bool>(&isMiscalib)->default_value(false), "apply the initial miscalibration")
	("miscalibMethod", po::value<int>(&miscalibMethod)->default_value(1), "miscalibration method")
	("miscalibMap", po::value<string>(&miscalibMap)->default_value("/gwteray/users/brianza/scalibMap2.txt"), "map for the miscalibration")
	("isSaveEPDistribution", po::value<bool>(&isSaveEPDistribution)->default_value(false), "save E/P distribution")
	("isMCTruth", po::value<bool>(&isMCTruth)->default_value(false), "option for MC")
	("isEPselection", po::value<bool>(&isEPselection)->default_value(false), "apply E/p selection")
	("isPtCut", po::value<bool>(&isPtCut)->default_value(false), "apply Pt cut")
	("PtMin", po::value<float>(&PtMin)->default_value(0.), "treshold for the Pt cut")
	("isfbrem", po::value<bool>(&isfbrem)->default_value(false), "apply fbrem cut")
	("fbremMax", po::value<float>(&fbremMax)->default_value(100.), "fbrem treshold")
	("isR9selection", po::value<bool>(&isR9selection)->default_value(false), "apply R9 selection")
	("R9Min", po::value<float>(&R9Min)->default_value(-1.), "R9 treshold")
	("applyPcorr", po::value<bool>(&applyPcorr)->default_value(true), "apply momentum correction")
	("inputMomentumScale", po::value<string>(&inputMomentumScale)->default_value("/afs/cern.ch/user/l/lbrianza/work/public/EoP_additionalFiles/MomentumCalibration2015_eta1_eta1.root"), "input momentum scale")
	("applyEcorr", po::value<bool>(&applyEcorr)->default_value(false), "apply energy correction")
	("inputEnergyScale", po::value<string>(&inputEnergyScale)->default_value("/afs/cern.ch/user/l/lbrianza/work/public/EoP_additionalFiles/momentumCalibration2015_EB_scE.root"), "input energy scale")
	("typeEB", po::value<string>(&typeEB)->default_value("eta1"), "")
	("typeEE", po::value<string>(&typeEE)->default_value("eta1"), "")
	("outputFile", po::value<string>(&outputFile)->default_value("FastCalibrator_Oct2015_runD"), "output file for E/P calibration")
	("numberOfEvents", po::value<int>(&numberOfEvents)->default_value(-1), "number of events (-1=all)")
	("useRawEnergy", po::value<int>(&useRawEnergy)->default_value(0), "use raw energy")
	("useZ", po::value<int>(&useZ)->default_value(1), "use Z events")
	("useW", po::value<int>(&useW)->default_value(1), "use W events")
	("splitStat", po::value<int>(&splitStat)->default_value(1), "split statistic")
	("nLoops", po::value<int>(&nLoops)->default_value(20), "number of iteration of the L3 algorithm")
	("isDeadTriggerTower", po::value<bool>(&isDeadTriggerTower)->default_value(false), "")
	("inputFileDeadXtal", po::value<string>(&inputFileDeadXtal)->default_value("NULL"), "")
	("EPMin", po::value<float>(&EPMin)->default_value(100.), "E/p window")
	("smoothCut", po::value<int>(&smoothCut)->default_value(0), "apply smooth cut on the E/p window")
	;

	desc.add(inputOption);
	desc.add(outputOption);
	desc.add(fitterOption);
	desc.add(anyVarOption);
	desc.add(smearerOption);
	desc.add(toyOption);
	desc.add(EoverPOption);
	desc.add(laserMonitoringEPOption);
	po::variables_map vm;
	//
	// po::store(po::parse_command_line(argc, argv, smearOption), vm);
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if(vm.count("anyVarBranches")){
		for(auto& b : branchListAny){
			std::cout << "branches: " << b << std::endl;
		}
	}else branchListAny.clear();
	for(auto& b : branchListAny){
		std::cout << "branches: " << b << std::endl;
	}
	
	branchListAny.push_back(invMass_var); //not needed because the invMass is actually added

	//------------------------------ checking options
	if(!vm.count("invMass_binWidth") && !vm.count("smearerFit")) {
		std::cout << "[INFO] Bin Width=0.5" << std::endl;
		invMass_binWidth = 0.5;
	}

	if (vm.count("help")) {
		cout << desc << "\n";
		return 1;
	}

	if(vm.count("useZPtweight") && !vm.count("pdfSystWeightIndex")) {
		std::cerr << "[ERROR] Asked for ZPt weights but no pdfSystWeightIndex indicated" << std::endl;
		exit(1);
	}

	TString energyBranchName = energyBranchNameFromInvMassName(invMass_var).c_str();

	if(!vm.count("chainFileList") && !vm.count("runToy")) {
		std::cerr << "[ERROR] Missing mandatory option \"chainFile\"" << std::endl;
		return 1;
	}


	if(!vm.count("regionsFile") &&
	        !vm.count("runDivide") && !vm.count("savePUTreeWeight") &&
	        !vm.count("saveR9TreeWeight") && !vm.count("saveCorrEleTree") && !vm.count("EOverPCalib")
	        && !vm.count("laserMonitoringEP")
	        //&& !vm.count("saveRootMacro")
	  ) {
		std::cerr << "[ERROR] Missing mandatory option \"regionsFile\"" << std::endl;
		return 1;
	}
	//  if(!vm.count("runRangesFile")){
	//    std::cerr << "[ERROR] Missing mandatory option \"runRangesFile\"" << std::endl;
	//    return 1;
	//  }

	if(vm.count("dataPU") != vm.count("mcPU")) {
		std::cerr << "[ERROR] One PU distribution (data or mc) are not provided" << std::endl;
		return 1;
	}


	//if((vm.count("corrEleType") != vm.count("corrEleFile"))&&(vm.count("corrEleType") != vm.count("readDirect")) ){
	//std::cout << "[ERROR] Either provide correction file name, or provide read direct option" << std::endl;
	//return 1;
	//}

	if( vm.count("corrEleType") && vm.count("corrEleFile") && vm.count("readDirect") ) {
		std::cout << "[ERROR] Either provide correction file name, or provide read direct option - choose only one" << std::endl;
		return 1;
	}

	//============================== Check output folders
	bool checkDirectories = true;
	checkDirectories = checkDirectories && !system("[ -d " + TString(outDirFitResMC) + " ]");
	if(!checkDirectories && !vm.count("EOverPCalib") && !vm.count("laserMonitoringEP")) {
		std::cerr << "[ERROR] Directory " << outDirFitResMC << " not found" << std::endl;
	}
	checkDirectories = checkDirectories && !system("[ -d " + TString(outDirFitResData) + " ]");
	if(!checkDirectories && !vm.count("EOverPCalib") && !vm.count("laserMonitoringEP")) {
		std::cerr << "[ERROR] Directory " << outDirFitResData << " not found" << std::endl;
	}
	checkDirectories = checkDirectories &&   !system("[ -d " + TString(outDirImgMC) + " ]");
	if(!checkDirectories && !vm.count("EOverPCalib") && !vm.count("laserMonitoringEP")) {
		std::cerr << "[ERROR] Directory " << outDirImgMC << " not found" << std::endl;
	}
	checkDirectories = checkDirectories && !system("[ -d " + TString(outDirImgData) + " ]");
	if(!checkDirectories && !vm.count("EOverPCalib") && !vm.count("laserMonitoringEP")) {
		std::cerr << "[ERROR] Directory " << outDirImgData << " not found" << std::endl;
	}

	//   checkDirectories=checkDirectories && !system("[ -d "+TString(outDirTable)+" ]");
	//   if(!checkDirectories){
	//     std::cerr << "[ERROR] Directory " << outDirTable << " not found" << std::endl;
	//   }
	if(!checkDirectories
	        && !vm.count("runDivide")
	        && !vm.count("savePUTreeWeight")
	        && !vm.count("saveCorrEleTree")
	        && !vm.count("saveR9TreeWeight")
	        && !vm.count("saveRootMacro")
	        && !vm.count("EOverPCalib")
	        && !vm.count("laserMonitoringEP")

	  ) return 1;

	//============================== Reading the config file with the list of chains
	tag_chain_map_t tagChainMap;
	TString tag, chainName, fileName;

	TString chainFileListTag = chainFileListName;
	chainFileListTag.Remove(0, chainFileListTag.Last('/') + 1);
	chainFileListTag.ReplaceAll(".dat", "");

	TString regionsFileNameTag = regionsFileName;
	regionsFileNameTag.Remove(0, regionsFileNameTag.Last('/') + 1);
	regionsFileNameTag.ReplaceAll(".dat", "");

	std::ifstream chainFileList(chainFileListName.c_str());
	while(chainFileList >> tag, chainFileList.good()) {
		if(tag.Contains('#')) {
			chainFileList.ignore(1000, '\n');
			continue;
		}
		chainFileList >> chainName >> fileName;
		if(chainName.Contains("Hist")) {
			if(chainName.Contains("nPV")) puBranchName = "nPV";
			// use these value only if not provided by the command line
			if(tag.Contains("d")) dataPUFileNameVec.push_back(fileName);
			else if(tag.Contains("s")) mcPUFileNameVec.push_back(fileName);
			else std::cerr << "[ERROR] in configuration file Hist not recognized" << std::endl;
			continue;
		}

		// discard file with energy corrections different from the specified type
		if(chainName.Contains("scaleEle")) {
			if(chainName != "scaleEle_" + corrEleType) continue;
		}

		// discard file with energy smearings different from the specified type
		if(chainName.Contains("smearEle")) {
			if(chainName != "smearEle_" + smearEleType) continue;
		}

		// discard file with categories for "smearingMethod" different from the region file name
		if(chainName.Contains("smearerCat")) {
			if(chainName != "smearerCat_" + regionsFileNameTag) continue;
		}

		if(!tagChainMap.count(tag)) {

#ifdef DEBUG
			std::cout << "[DEBUG] Create new tag map for tag: " << tag << std::endl;
#endif
			tagChainMap.insert(make_pair(tag, chain_map_t()));
		}

		if(!tagChainMap[tag].count(chainName)) {
			tagChainMap[tag].insert(make_pair(chainName, pTChain_t(new TChain(chainName))));
			tagChainMap[tag][chainName]->SetTitle(tag);
		}
		std::cout << "Adding file: " << tag << "\t" << chainName << "\t" << fileName << std::endl;
		if((tagChainMap[tag])[chainName]->Add(fileName, -1) == 0) exit(1);
#ifdef DEBUG
		std::cout << "[DEBUG] " << tag << "\t" << chainName << "\t" << fileName << "\t" << (tagChainMap[tag])[chainName]->GetEntries() << std::endl;
#endif

	}

	//init chains and print
	for(tag_chain_map_t::const_iterator tag_chain_itr = tagChainMap.begin();
	        tag_chain_itr != tagChainMap.end();
	        tag_chain_itr++) {
#ifdef DEBUG
		std::cout << tag_chain_itr->first << std::endl;
#endif
		for(chain_map_t::const_iterator chain_itr = tag_chain_itr->second.begin();
		        chain_itr != tag_chain_itr->second.end();
		        chain_itr++) {
			//#ifdef DEBUG
			std::cout << " - " << chain_itr->first << "\t" << chain_itr->second->GetName() << "\t" << chain_itr->second->GetTitle() << "\t" << chain_itr->second->GetEntries() << std::endl;
			//#endif
			chain_itr->second->GetEntries();
		}
	}





	TString r(regionsFileName.c_str());
	r.Remove(0, r.Last('/') + 1);
	r.ReplaceAll(".dat", "");

	std::vector<TString> regions = ReadRegionsFromFile(regionsFileName);
	std::vector<TString> runRanges = ReadRegionsFromFile(runRangesFileName);
	std::vector<TString> categories;

	for(std::vector<TString>::const_iterator region_itr = regions.begin();
	        region_itr != regions.end();
	        region_itr++) {
		if(runRanges.size() > 0) {
			for(std::vector<TString>::const_iterator runRange_itr = runRanges.begin();
			        runRange_itr != runRanges.end();
			        runRange_itr++) {
				TString token1, token2;
				//Ssiz_t ss=0;
				//runRange_itr->Tokenize(token1,ss,"-");
				//ss=runRange_itr->First('-');
				//runRange_itr->Tokenize(token2,ss,"-");
				TObjArray *tx = runRange_itr->Tokenize("-");
				token1 = ((TObjString *)(tx->At(0)))->String();
				token2 = ((TObjString *)(tx->At(1)))->String();
				categories.push_back((*region_itr) + "-runNumber_" + token1 + "_" + token2 + "-" + commonCut.c_str());
			}
		} else categories.push_back((*region_itr) + "-" + commonCut.c_str());
	}



	///------------------------------ to obtain r9weights
	if(vm.count("saveR9TreeWeight") && !vm.count("r9WeightFile")) {
		std::cerr << "[ERROR] No r9WeightFile specified" << std::endl;
		return 1;
	}
	if(vm.count("r9WeightFile")) {
		std::cout << "------------------------------------------------------------" << std::endl;
		std::cout << "[STATUS] Getting r9Weights from file: " << r9WeightFile << std::endl;
		r9Weights_class r9Weights;
		r9Weights.ReadFromFile(r9WeightFile);

		TString treeName = "r9Weight";

		// mc // save it in a file and reload it as a chain to be safe against the reference directory for the tree
		for(tag_chain_map_t::iterator tag_chain_itr = tagChainMap.begin();
		        tag_chain_itr != tagChainMap.end();
		        tag_chain_itr++) {
			if(tag_chain_itr->first.CompareTo("d") == 0 || tag_chain_itr->first.CompareTo("s") == 0) continue;
			if(tag_chain_itr->second.count(treeName) != 0) continue; //skip if already present
			TChain * ch = tag_chain_itr->second.find("selected")->second.get();

			TString filename = "tmp/r9Weight_" + tag_chain_itr->first + "-" + chainFileListTag + ".root";
			std::cout << "[STATUS] Saving r9Weights tree to root file:" << filename << std::endl;

			TFile f(filename, "recreate");
			if(!f.IsOpen() || f.IsZombie()) {
				std::cerr << "[ERROR] File for r9Weights: " << filename << " not opened" << std::endl;
				exit(1);
			}
			TTree *corrTree = r9Weights.GetTreeWeight(ch);
			f.cd();
			corrTree->Write();
			std::cout << "[INFO] Data      entries: " << ch->GetEntries() << std::endl;
			std::cout << "       r9Weights entries: " << corrTree->GetEntries() << std::endl;
			delete corrTree;

			f.Write();
			f.Close();
			chain_map_t::iterator chain_itr = (tag_chain_itr->second.insert(make_pair(treeName, pTChain_t(new TChain(treeName))))).first;
			chain_itr->second->SetTitle(tag_chain_itr->first);
			chain_itr->second->Add(filename);

		} // end of data samples loop
	} // end of r9Weight


	if(vm.count("saveR9TreeWeight")) return 0;

	///------------------------------ to obtain ZPt weights
	if(vm.count("ZPtWeightFile")) {
		std::cout << "------------------------------------------------------------" << std::endl;
		std::cout << "[STATUS] Getting ZPtWeights from file: " << ZPtWeightFile << std::endl;
		UpdateFriends(tagChainMap, regionsFileNameTag);
		ZPtWeights_class ZPtWeights;
		ZPtWeights.ReadFromFile(ZPtWeightFile);

		TString treeName = "ZPtWeight";

		// mc // save it in a file and reload it as a chain to be safe against the reference directory for the tree
		for(tag_chain_map_t::iterator tag_chain_itr = tagChainMap.begin();
		        tag_chain_itr != tagChainMap.end();
		        tag_chain_itr++) {
			if(tag_chain_itr->first.Contains("d")) continue; /// \todo ZPtWeight only on MC! because from PdfWeights, to make it more general
			if(tag_chain_itr->first.CompareTo("d") == 0 || tag_chain_itr->first.CompareTo("s") == 0) continue;
			if(tag_chain_itr->second.count(treeName) != 0) continue; //skip if already present
			TChain * ch = (tag_chain_itr->second.find("selected"))->second.get();

			TString filename = "tmp/ZPtWeight_" + tag_chain_itr->first + "-" + chainFileListTag + ".root";
			std::cout << "[STATUS] Saving r9Weights tree to root file:" << filename << std::endl;

			TFile f(filename, "recreate");
			if(!f.IsOpen() || f.IsZombie()) {
				std::cerr << "[ERROR] File for ZPtWeights: " << filename << " not opened" << std::endl;
				exit(1);
			}
			TTree *corrTree = ZPtWeights.GetTreeWeight(ch, "ZPt_" + energyBranchName);
			f.cd();
			corrTree->Write();
			std::cout << "[INFO] Data      entries: " << ch->GetEntries() << std::endl;
			std::cout << "       ZPtWeights entries: " << corrTree->GetEntries() << std::endl;
			delete corrTree;

			f.Write();
			f.Close();
			chain_map_t::iterator chain_itr = (tag_chain_itr->second.insert(make_pair(treeName, pTChain_t(new TChain(treeName))))).first;
			chain_itr->second->SetTitle(tag_chain_itr->first);
			chain_itr->second->Add(filename);

		} // end of data samples loop
	} // end of r9Weight


	//==============================
	//  if(vm.count("dataPU")==0 && (tagChainMap["s"]).count("pileupHist")==0 && (tagChainMap["s"]).count("pileup")==0){
	if(vm.count("noPU") == 0 && !vm.count("runToy")) {
		if(dataPUFileNameVec.empty() && (tagChainMap.count("s") != 0) && (tagChainMap["s"]).count("pileup") == 0) {
			std::cerr << "[ERROR] Nor pileup mc tree configured in chain list file either dataPU histograms are not provided" << std::endl;
			return 1;
		} else if( !vm.count("runToy") && (vm.count("dataPU") != 0 || (!dataPUFileNameVec.empty() && ((tagChainMap.count("s") == 0) || (tagChainMap["s"]).count("pileup") == 0)))) {
			std::cout << "[STATUS] Creating pileup weighting tree and saving it" << std::endl;
			for(unsigned int i = 0; i < mcPUFileNameVec.size(); i++) {
				TString mcPUFileName_ = mcPUFileNameVec[i];
				TString dataPUFileName_ = dataPUFileNameVec[i];
				TString runMin_ = "";
				if(!mcPUFileName_.Contains("nPU")) {
// 	if(mcPUFileName_.Index(".runMin_")!=-1){
					runMin_ = mcPUFileName_;
					runMin_.Remove(0, runMin_.Last('/') + 1);
					runMin_.Remove(runMin_.First('-'));
// 	  runMin_.Remove(runMin_.First('.'));
// 	  runMin_.ReplaceAll("runMin_","");
				}
				int runMin = runMin_.Sizeof() > 1 ? runMin_.Atoi() : 1;
				std::cout << "********* runMin = " << runMin << "\t" << runMin_ << std::endl;
				puWeights.ReadFromFiles(mcPUFileName_.Data(), dataPUFileName_.Data(), runMin);
			}

			// for each mc sample create a tree with the per-event-weight
			// but exclude the chain "s" since it's supposed to be created mergin alle the s-type samples
			for(tag_chain_map_t::iterator tag_chain_itr = tagChainMap.begin();
			        tag_chain_itr != tagChainMap.end();
			        tag_chain_itr++) {
				if(tag_chain_itr->first.CompareTo("s") == 0 || !tag_chain_itr->first.Contains("s")) continue;
				TChain * ch = (tag_chain_itr->second.find("selected"))->second.get();
				if((tag_chain_itr->second.count("pileup"))) continue;
				TString treeName = "pileup";
				TString filename = "tmp/mcPUtree" + tag_chain_itr->first + ".root";
				TFile f(filename, "recreate");
				if(f.IsOpen()) {
					f.cd();

					TTree *puTree = puWeights.GetTreeWeight(ch, true, puBranchName.c_str());
					puTree->SetName(treeName);
					puTree->Write();
					delete puTree;
					f.Write();
					f.Close();
					chain_map_t::iterator chain_itr = (tag_chain_itr->second.insert(make_pair(treeName, pTChain_t(new TChain(treeName))))).first;
					chain_itr->second->SetTitle(tag_chain_itr->first);
					chain_itr->second->Add(filename);
				}
			}
		}
	}

	//read corrections directly from file
	if (vm.count("corrEleType") && corrEleFile != "") {
		std::cout << "------------------------------------------------------------" << std::endl;
		std::cout << "[STATUS] Getting energy scale corrections from file: " << corrEleFile << std::endl;
		TString treeName = "scaleEle_" + corrEleType;
		EnergyScaleCorrection_class eScaler(corrEleFile, 0, true, false);

		for(tag_chain_map_t::iterator tag_chain_itr = tagChainMap.begin();
		        tag_chain_itr != tagChainMap.end();
		        tag_chain_itr++) {
			if(tag_chain_itr->first.CompareTo("d") == 0 || !tag_chain_itr->first.Contains("d")) continue; //only data
			if(tag_chain_itr->second.count(treeName) != 0) continue; //skip if already present
			TChain * ch = (tag_chain_itr->second.find("selected"))->second.get();

			TString filename = "tmp/scaleEle_" + corrEleType + "_" + tag_chain_itr->first + "-" + chainFileListTag + ".root";
			std::cout << "[STATUS] Saving electron scale corrections to root file:" << filename << std::endl;

			TFile f(filename, "recreate");
			if(!f.IsOpen() || f.IsZombie()) {
				std::cerr << "[ERROR] File for scale corrections: " << filename << " not opened" << std::endl;
				exit(1);
			}
#ifdef toBeFixed
			TTree *corrTree = eScaler.GetCorrTree(ch, "runNumber", "R9Eleprime");
#else
			TTree *corrTree = NULL;
#endif
			corrTree->SetName(TString("scaleEle_") + corrEleType.c_str());
			corrTree->SetTitle(corrEleType.c_str());
			f.cd();
			corrTree->Write();
			std::cout << "[INFO] Data entries: "    << ch->GetEntries() << std::endl;
			std::cout << "       corrEle entries: " << corrTree->GetEntries() << std::endl;
			delete corrTree;

			f.Write();
			f.Close();
			chain_map_t::iterator chain_itr = (tag_chain_itr->second.insert(make_pair(treeName, pTChain_t(new TChain(treeName))))).first;
			chain_itr->second->SetTitle(tag_chain_itr->first);
			chain_itr->second->Add(filename);

		} // end of data samples loop
	} // end of corrEle loop


	//read corrections directly from file
	if (vm.count("smearEleType")) {
		std::cout << "------------------------------------------------------------" << std::endl;
		std::cout << "[STATUS] Getting energy smearings from file: " << smearEleFile << std::endl;
		TString treeName = "smearEle_" + smearEleType;
		EnergyScaleCorrection_class eScaler(smearEleFile, 0, false, true);
		for(tag_chain_map_t::iterator tag_chain_itr = tagChainMap.begin();
		        tag_chain_itr != tagChainMap.end();
		        tag_chain_itr++) {
			if(tag_chain_itr->first.CompareTo("s") == 0 || !tag_chain_itr->first.Contains("s")) continue; //only data
			if(tag_chain_itr->second.count(treeName) != 0) continue; //skip if already present
			TChain * ch = (tag_chain_itr->second.find("selected"))->second.get();

			TString filename = "tmp/smearEle_" + smearEleType + "_" + tag_chain_itr->first + "-" + chainFileListTag + ".root";
			std::cout << "[STATUS] Saving electron smearings to root file:" << filename << std::endl;

			TFile f(filename, "recreate");
			if(!f.IsOpen() || f.IsZombie()) {
				std::cerr << "[ERROR] File for scale corrections: " << filename << " not opened" << std::endl;
				exit(1);
			}
#ifdef shervin
			if(TString(smearEleType).Contains("CB")) {
				eScaler.SetSmearingType(1);
				eScaler.SetSmearingCBAlpha(smearingCBAlpha);
			}
			TTree *corrTree = eScaler.GetSmearTree(ch, true, energyBranchName );
#else
			TTree *corrTree = NULL;
#endif

			f.cd();
			corrTree->SetName(TString("smearEle_") + smearEleType.c_str());
			corrTree->SetTitle(smearEleType.c_str());
			corrTree->Write();
			std::cout << "[INFO] Data entries: "    << ch->GetEntries() << std::endl;
			std::cout << "       smearEle entries: " << corrTree->GetEntries() << std::endl;
			delete corrTree;

			f.Write();
			f.Close();
			chain_map_t::iterator chain_itr = (tag_chain_itr->second.insert(make_pair(treeName, pTChain_t(new TChain(treeName))))).first;
			chain_itr->second->SetTitle(tag_chain_itr->first);
			chain_itr->second->Add(filename);
		}
		// \todo need the data part in case of needs
	}


	addBranch_class newBrancher;
	newBrancher._commonCut = commonCut.c_str();
	newBrancher._regionList = categories;

	for( std::vector<string>::const_iterator branch_itr = branchList.begin();
	        branch_itr != branchList.end();
	        branch_itr++) {
		UpdateFriends(tagChainMap, regionsFileNameTag);

		TString treeName = *branch_itr;
		TString t;
		if(treeName == "smearerCat_s") {
			treeName.ReplaceAll("_s", "");
			t = "s";
		}
		if(treeName == "smearerCat_d") {
			treeName.ReplaceAll("_d", "");
			t = "d";
		}
		TString branchName = treeName;
		std::cout << "#### --> " << treeName << "\t" << t << "\t" << *branch_itr << std::endl;
		if(branchName == "smearerCat") treeName += "_" + regionsFileNameTag;
#ifdef invMassSigma
		if(treeName.Contains("invMassSigma")) {
			newBrancher.scaler = new EnergyScaleCorrection_class("", smearEleFile);
		}
#endif
		for(tag_chain_map_t::iterator tag_chain_itr = tagChainMap.begin();
		        tag_chain_itr != tagChainMap.end();
		        tag_chain_itr++) {
			if((tag_chain_itr->first.CompareTo("s") == 0 || tag_chain_itr->first.CompareTo("d") == 0)) continue; //only data
			if(tag_chain_itr->second.count(treeName) != 0) continue; //skip if already present
			if(t != "" && !tag_chain_itr->first.Contains(t)) continue;
			TChain * ch = (tag_chain_itr->second.find("selected"))->second.get();

			//data
			std::cout << "[STATUS] Adding branch " << branchName << " to " << tag_chain_itr->first << std::endl;
			TString filename = "tmp/" + treeName + "_" + tag_chain_itr->first + "-" + chainFileListTag + ".root";

			TFile f(filename, "recreate");
			if (!f.IsOpen()) {
				std::cerr << "[ERROR] File for branch " << branchName << " not created" << std::endl;
				return 1;
			}
			f.cd();

			TTree *newTree = newBrancher.AddBranch(ch, treeName, branchName, true, tag_chain_itr->first.Contains("s"), energyBranchName);
			if(newTree == NULL) {
				std::cerr << "[ERROR] New tree for branch " << treeName << " is NULL" << std::endl;
				return 1;
			}
			f.cd();
			newTree->SetTitle(tag_chain_itr->first);
			newTree->Write();
			delete newTree;
			//f.Write();
			f.Close();
			std::pair<TString, pTChain_t > pair_tmp(treeName, pTChain_t(new TChain(treeName)));
			chain_map_t::iterator chain_itr = (tag_chain_itr->second.insert(pair_tmp)).first;
			chain_itr->second->SetTitle(tag_chain_itr->first);
			chain_itr->second->Add(filename);
		} //end of sample loop
	} //end of branches loop

	//(tagChainMap["s"])["selected"]->GetEntries();
	UpdateFriends(tagChainMap, regionsFileNameTag);

	//create tag "s" if not present (due to multiple mc samples)
	if(!tagChainMap.count("s")) {
		//#ifdef DEBUG
		std::cout << "==============================" << std::endl;
		std::cout << "==============================" << std::endl;
		MergeSamples(tagChainMap, regionsFileNameTag, "s");
		MergeSamples(tagChainMap, regionsFileNameTag, "d");
	}

	ElectronCategory_class cutter;
	cutter.invMassBranchName = invMass_var;
	cutter.energyBranchName = energyBranchName;

	///------------------------------ to obtain run ranges
	if(vm.count("runDivide")) {
		runDivide_class runDivider(cutter.GetCut(commonCut+"-eleID_"+selection, false,0), cutter.GetBranchNameNtuple(commonCut+"-eleID_"+selection));
		runDivider.Divide((tagChainMap["d"])["selected"].get(), "data/runRanges/runRangeLimits.dat", nEvents_runDivide);
		runDivider.PrintRunRangeEvents();
		// std::vector<TString> runRanges;
		// if(runRangesFileName != "") runRanges = ReadRegionsFromFile(runRangesFileName);
		// for(std::vector<TString>::const_iterator itr = runRanges.begin();
		//         itr != runRanges.end();
		//         itr++) {
		// 	std::cout << *itr << "\t" << "-1" << "\t" << runDivider.GetRunRangeTime(*itr) << std::endl;
		// }

	}


//   Dump(tagChainMap, "s",0);
//   Dump(tagChainMap, "s",(tagChainMap["s1"])["selected"]->GetEntries());
//   Dump(tagChainMap, "s",(tagChainMap["s2"])["selected"]->GetEntries());
//   exit(0);
	if(vm.count("saveRootMacro")) {
		for(tag_chain_map_t::const_iterator tag_chain_itr = tagChainMap.begin();
		        tag_chain_itr != tagChainMap.end();
		        tag_chain_itr++) {
			for(chain_map_t::const_iterator chain_itr = tag_chain_itr->second.begin();
			        chain_itr != tag_chain_itr->second.end();
			        chain_itr++) {
				chain_itr->second->SaveAs("tmp/" + tag_chain_itr->first + "_" + chain_itr->first + "_chain.root");
				//chain_itr->second->SavePrimitive(std::cout); // not implemented
			}
		}
	}

	if(
	    vm.count("saveRootMacro")
	    || vm.count("saveCorrEleTree")
	    || vm.count("saveAddBranchTree")
	    || vm.count("savePUTreeWeight")
	    || vm.count("runDivide")
	    || vm.count("saveCorrEleTree")
	    || vm.count("saveR9TreeWeight")
	) return 0;

	eleID += selection.c_str();
	eleID.ReplaceAll("-", "|");
	//Note for expert users: whenever the eleID selection name (the one in CMSSW) has a "-" it must be replaced (before categorization) with "|"
	//This is because ElectronCategory_class splits strings based on "-" and you don't want to split the eleID name!
	eleID.ReplaceAll("_", "");

	TChain * data = NULL;
	TChain * mc = NULL;
	if(!vm.count("smearerFit")) {
		data = (tagChainMap["d"])["selected"].get();
		mc  = (tagChainMap["s"])["selected"].get();
	}


	//------------------------------ Take the list of branches needed for the defined categories
	std::set<TString> activeBranchList;
	for(std::vector<TString>::const_iterator region_itr = categories.begin();
	        region_itr != categories.end();
	        region_itr++) {
		TString r = *region_itr;
		r.ReplaceAll(commonCut, "");
		std::set<TString> tmpList = cutter.GetBranchNameNtuple(r);
		assert(tmpList.size()>0);
		activeBranchList.insert(tmpList.begin(), tmpList.end());
		// add also the friend branches!
	}

	if(vm.count("zFit")) {

		ZFit_class fitter( data, mc, NULL,
		                   invMass_var.c_str(), invMass_min, invMass_max, invMass_binWidth);

		fitter._oddMC = vm.count("isOddMC");
		fitter._oddData = vm.count("isOddData");

		if(vm.count("r9WeightFile")) {
			// if the data are weighted need to use the unbinned likelihood fit to have the correct errors
			fitter._isDataUnbinned = true;
			fitter._isDataSumW2 = true;
		}

		fitter._forceNewFit = vm.count("forceNewFit");
		//  fitter._initFitMC=true;
		fitter.SetFitType(fit_type_value);
		fitter._updateOnly = vm.count("updateOnly");

		fitter.imgFormat = imgFormat;
		fitter.outDirFitResMC = outDirFitResMC;
		fitter.outDirFitResData = outDirFitResData;
		fitter.outDirImgMC = outDirImgMC;
		fitter.outDirImgData = outDirImgData;

		// check folder existance
		fitter.SetPDF_model(signal_type_value, 0); // (0,0) convolution, CB no_bkg
		//fitter.SetPDF_model(1,0); // cruijff, no_bkg

		fitter.Import(commonCut.c_str(), eleID, activeBranchList);
		for(std::vector<TString>::const_iterator category_itr = categories.begin();
		        category_itr != categories.end();
		        category_itr++) {
			myClock.Start();
#ifdef DEBUG
			std::cout << "[DEBUG] category: " << *category_itr << std::endl;
#endif
			if (vm.count("runToy")) {
				cout << "number of toys: " << nToys << endl;
				fitter.SetInitFileMC(outDirFitResMC + "/" + *category_itr + ".txt");
				fitter.FitToy(*category_itr, nToys, nEventsPerToy);
			} else	fitter.Fit(*category_itr);
			myClock.Stop();
			myClock.Print();
		}
	}

	myClock.Reset();




//------------------------------ LASER MONITORING WITH E/P  ------------------------------------------------------

	if(vm.count("laserMonitoringEP")) {

		std::cout << "start monitoring. " << std::endl;

		LaserMonitoringEoP analyzer(data, mc, useRegression);
		analyzer.Loop(yMIN, yMAX, EBEE, evtsPerPoint, useRegression, dayMin, dayMax, dayZOOM, LUMI, EoPvsPU);

	}

	///////////--------------------------- E/P calibration ----------------------------------------------------------------------

	if(vm.count("EOverPCalib")) {
		bool isEB = vm.count("doEB");
		TString partition = (isEB) ? "EB" : "EE";
		if(isEB) {
			std::cout << "---- START E/P CALIBRATION: BARREL ----" << std::endl;
		} else {
			std::cout << "---- START E/P CALIBRATION: ENDCAP ----" << std::endl;
		}

		int nRegions = (isEB) ? GetNRegionsEB(typeEB) : GetNRegionsEE(typeEE);

		system(("mkdir -p " + outDirFitResData).c_str());

		/// open calibration momentum graph
		TFile* momentumscale = new TFile((inputMomentumScale.c_str()));//+"_"+typeEB+"_"+typeEE+".root").c_str());
		std::vector<TGraphErrors*> g_EoC;

		TString Name = (isEB) ? "g_EoC_EB_0" : "g_EoC_EE_0";
		g_EoC.push_back( (TGraphErrors*)(momentumscale->Get(Name)) );

		std::cout << "momentum calibration file correctly opened" << std::endl;

		/// open calibration energy graph
		TFile* energyscale = new TFile((inputEnergyScale.c_str()));
		std::vector<TGraphErrors*> g_EoE;

		for(int i = 0; i < nRegions; ++i) {
			TString Name = "g_pData_" + partition + Form("_0_%d", i);
			g_EoE.push_back( (TGraphErrors*)(energyscale->Get(Name)) );
		}

		std::cout << "energy calibration file correctly opened" << std::endl;

		///Use the whole sample statistics if numberOfEvents < 0
		if ( numberOfEvents < 0 ) numberOfEvents = data->GetEntries();

		std::cout << "number of events: " << numberOfEvents << std::endl;

		/// run in normal mode: full statistics
		if ( splitStat == 0 ) {

			TString name = Form("%s%s_%s.root", outDirFitResData.c_str(), outputFile.c_str(), partition.Data());
			TFile *outputName = new TFile(name, "RECREATE");

			TString outEPDistribution;
			if(isSaveEPDistribution == true)
				outEPDistribution = "Weight_" + name;
			else
				outEPDistribution = "NULL";

			TString DeadXtal = inputFileDeadXtal.c_str();

			std::cout << "start calibration. " << std::endl;

			FastCalibratorEB analyzerEB(data, g_EoC, g_EoE, typeEB, outEPDistribution);
			FastCalibratorEE analyzerEE(data, g_EoC, g_EoE, typeEE, outEPDistribution);
			if(isEB) {
				analyzerEB.bookHistos(nLoops);
				analyzerEB.AcquireDeadXtal(DeadXtal, isDeadTriggerTower);
				analyzerEB.Loop(numberOfEvents, useZ, useW, splitStat, nLoops, applyPcorr, applyEcorr, useRawEnergy, isMiscalib, isSaveEPDistribution, isEPselection, isR9selection, R9Min, EPMin, smoothCut, isfbrem, fbremMax, isPtCut, PtMin, isMCTruth, miscalibMethod, miscalibMap);
				analyzerEB.saveHistos(outputName);
			} else {
				analyzerEE.bookHistos(nLoops);
				analyzerEE.AcquireDeadXtal(DeadXtal, isDeadTriggerTower);
				analyzerEE.Loop(numberOfEvents, useZ, useW, splitStat, nLoops, applyPcorr, applyEcorr, useRawEnergy, isMiscalib, isSaveEPDistribution, isEPselection, isR9selection, R9Min, EPMin, smoothCut, isfbrem, fbremMax, isPtCut, PtMin, isMCTruth,  miscalibMethod, miscalibMap);
				analyzerEE.saveHistos(outputName);
			}

		}

		/// run in even-odd mode: half statistics
		else if ( splitStat == 1 ) {

			/// Prepare the outputs
			TString name1;
			TString name2;

			name1 = Form("%s%s_%s_even.root", outDirFitResData.c_str(), outputFile.c_str(), partition.Data());
			name2 = name1;
			name2.ReplaceAll("even", "odd");

			TFile *outputName1 = new TFile(outDirFitResData + name1, "RECREATE");
			TFile *outputName2 = new TFile(outDirFitResData + name2, "RECREATE");

			TString DeadXtal = Form("%s", inputFileDeadXtal.c_str());

			/// Run on odd
			FastCalibratorEB analyzer_even_EB(data, g_EoC, g_EoE, typeEB);
			FastCalibratorEB analyzer_odd_EB(data, g_EoC, g_EoE, typeEB);

			FastCalibratorEE analyzer_even_EE(data, g_EoC, g_EoE, typeEE);
			FastCalibratorEE analyzer_odd_EE(data, g_EoC, g_EoE, typeEE);
			if(isEB) {
				analyzer_even_EB.bookHistos(nLoops);
				analyzer_even_EB.AcquireDeadXtal(DeadXtal, isDeadTriggerTower);
				analyzer_even_EB.Loop(numberOfEvents, useZ, useW, splitStat, nLoops, applyPcorr, applyEcorr, useRawEnergy, isMiscalib, isSaveEPDistribution, isEPselection, isR9selection, R9Min, EPMin, smoothCut, isfbrem, fbremMax, isPtCut, PtMin, isMCTruth,  miscalibMethod, miscalibMap);
				analyzer_even_EB.saveHistos(outputName1);

				analyzer_odd_EB.bookHistos(nLoops);
				analyzer_odd_EB.AcquireDeadXtal(DeadXtal, isDeadTriggerTower);
				analyzer_odd_EB.Loop(numberOfEvents, useZ, useW, -splitStat, nLoops, applyPcorr, applyEcorr, useRawEnergy, isMiscalib, isSaveEPDistribution, isEPselection, isR9selection, R9Min, EPMin, smoothCut, isfbrem, fbremMax, isPtCut, PtMin, isMCTruth,  miscalibMethod, miscalibMap);
				analyzer_odd_EB.saveHistos(outputName2);

			} else {
				analyzer_even_EE.bookHistos(nLoops);
				analyzer_even_EE.AcquireDeadXtal(DeadXtal, isDeadTriggerTower);
				analyzer_even_EE.Loop(numberOfEvents, useZ, useW, splitStat, nLoops, applyPcorr, applyEcorr, useRawEnergy, isMiscalib, isSaveEPDistribution, isEPselection, isR9selection, R9Min, EPMin, smoothCut, isfbrem, fbremMax, isPtCut, PtMin, isMCTruth,  miscalibMethod, miscalibMap);
				analyzer_even_EE.saveHistos(outputName1);


				analyzer_odd_EE.bookHistos(nLoops);
				analyzer_odd_EE.AcquireDeadXtal(DeadXtal, isDeadTriggerTower);
				analyzer_odd_EE.Loop(numberOfEvents, useZ, useW, -splitStat, nLoops, applyPcorr, applyEcorr, useRawEnergy, isMiscalib, isSaveEPDistribution, isEPselection, isR9selection, R9Min, EPMin, smoothCut, isfbrem, fbremMax, isPtCut, PtMin, isMCTruth,  miscalibMethod, miscalibMap);
				analyzer_odd_EE.saveHistos(outputName2);

			}

		}

	}
///////////////////////////////////-------E/P calibration done!--------------////////////////////////



//------------------------------ anyVar_class declare and set the options
	if(vm.count("anyVar")) {
		TFile *reduced_trees_file = new TFile((outDirFitResData + "/reduced_trees_file.root").c_str(), "RECREATE");

		TDirectory *dir = reduced_trees_file->GetDirectory("");
		//TDirectory *dir = new TDirectory(); //
		{
			//anyVar_class anyVar(data, branchListAny, cutter, invMass_var, outDirFitResData + "/", reduced_trees_file->GetDirectory(""), true); // vm.count("updateOnly"));
			anyVar_class anyVar(data, branchListAny, cutter, invMass_var, outDirFitResData + "/", dir, true); // vm.count("updateOnly"));
			anyVar._exclusiveCategories = false;
			anyVar.Import(commonCut, eleID, activeBranchList, modulo); //activeBranchList is the list of branches for category selections
			///\todo allocating both takes too much memory
			if(vm.count("runToy") && modulo > 0) {
				// splitting the events by "modulo" and obtaining statistically indipendent subsamples
				for(unsigned int moduloIndex = 0; moduloIndex < modulo; ++moduloIndex) {
					//change the output directory for the results
					std::string dir = outDirFitResData;
					size_t slashpos = dir.find_last_of('/');
					if( slashpos != std::string::npos && slashpos != dir.size()) dir.erase(dir.rfind("/"));
					dir += "-modulo_" + std::to_string(moduloIndex) + "/";
					system(("mkdir -p " + dir).c_str());
					anyVar.SetOutDirName(dir, vm.count("updateOnly"));
					std::cout << "[INFO] setting new output dir: " << dir << std::endl;

					//anyVar.Import(commonCut, eleID, activeBranchList, modulo, moduloIndex);
					anyVar.ChangeModulo(moduloIndex);
					anyVar.SaveReducedTree(reduced_trees_file);
#ifndef dump_root_tree
					for(auto& region : categories) {
						std::cout << "------------------------------------------------------------" << std::endl;
						std::cout << "[DEBUG ZFitter] category is: " << region << std::endl;
						anyVar.TreeAnalyzeShervin(region.Data(), cutter.GetCut(region, false, 1), cutter.GetCut(region, false, 2), scale);
					}
					break;
#else
					std::cerr << "[ERROR] dump_root_tree defined and running the toys with modulo: not implemented" << std::endl;
#endif
				}
				reduced_trees_file->Write();
				reduced_trees_file->Close();
			} else {
				// anyVar_class anyVarMC(mc, branchListAny, cutter, invMass_var, outDirFitResMC + "/", vm.count("updateOnly"));
				// anyVarMC._exclusiveCategories = false;
				// anyVarMC.Import(commonCut, eleID, activeBranchList);

//			anyVar.SaveReducedTree(reduced_trees_file);
#ifndef dump_root_tree
				anyVar._exclusiveCategories = true;
				for(auto& region : regions) { //categories
					anyVar.ChangeModulo(0);
					if(runRanges.size() > 0) {
						for(auto& runRange : runRanges) {
							TString token1, token2;
							TObjArray *tx = runRange.Tokenize("-");
							token1 = ((TObjString *)(tx->At(0)))->String();
							token2 = ((TObjString *)(tx->At(1)))->String();
							TString category = region + "-runNumber_" + token1 + "_" + token2;
							TString c = category + "-" + commonCut.c_str();
							anyVar.TreeAnalyzeShervin(c.Data(), cutter.GetCut(category, false, 1), cutter.GetCut(category, false, 2));
						}
					} else {
						TString category = region;
						TString c = category + "-" + commonCut.c_str();
						anyVar.TreeAnalyzeShervin(c.Data(), cutter.GetCut(category, false, 1), cutter.GetCut(category, false, 2));
					}
//				anyVarMC.TreeAnalyzeShervin(region.Data(), cutter.GetCut(region, true, 1), cutter.GetCut(region, true, 2), scale);
				}
#endif
				reduced_trees_file->Write();
			}
		}
		reduced_trees_file->Close();
	}

	if(vm.count("smearerFit")) {
		//------------------------------ RooSmearer
		RooArgSet args;
		std::vector<RooArgSet> args_vec;

		for(std::vector<TString>::const_iterator region_itr = categories.begin();
		        region_itr != categories.end();
		        region_itr++) {
			RooRealVar *scale_ = new RooRealVar("scale_" + *region_itr, "scale_" + *region_itr, 1.0, 0.95, 1.05, "GeV"); //0.9,1.1,"GeV")
			scale_->setError(0.005); // 1%
			//    scale_->setConstant();
			//    scale_->setBinning(RooBinning(
			args.add(*scale_);

			TString varName = *region_itr;
			TPRegexp reg("Et_[0-9.]*_[0-9.]*");
			reg.Substitute(varName, "");
			TPRegexp reg2("energySC_[0-9.]*_[0-9.]*");
			reg2.Substitute(varName, "");
			varName.ReplaceAll("--", "-");
			if(varName.First("-") == 0) varName.Remove(0, 1);

			//RooRealVar *const_term_ = new RooRealVar("constTerm_"+*region_itr, "constTerm_"+*region_itr, 0.01, 0.0005, 0.05);
			RooAbsReal *const_term_ = NULL;
			RooRealVar *const_term_v = args.getSize() == 0 ? NULL : (RooRealVar *) args.find("constTerm_" + varName);
			if(const_term_v == NULL) {
				if(vm.count("constTermFix") == 0)
					const_term_v = new RooRealVar("constTerm_" + *region_itr, "constTerm_" + varName, 0.00, 0.000, 0.05);
				else
					const_term_v = new RooRealVar("constTerm_" + varName, "constTerm_" + varName, 0.00, 0.000, 0.02);

				if(smearEleType == "" && initFileName == "") {
					const_term_v->setVal(0.01);
				}
				const_term_v->setError(0.03); // 1%
				//const_term_v->setConstant(true);
				args.add(*const_term_v);
			}
			if((reg.MatchB(*region_itr) || reg2.MatchB(*region_itr) ) && vm.count("constTermFix") == 1) {
				const_term_ = new RooFormulaVar("constTerm_" + *region_itr, "constTerm_" + varName, "@0", *const_term_v);
				const_term_v->setConstant(false);
			} else const_term_ = const_term_v;



			RooAbsReal *alpha_ = NULL;
			RooRealVar *alpha_v = args.getSize() == 0 ? NULL : (RooRealVar *) args.find("alpha_" + varName);
			if(alpha_v == NULL) {
				alpha_v = new RooRealVar("alpha_" + varName, "alpha_" + varName, 0.0, 0., 0.20);
				alpha_v->setError(0.01);
				alpha_v->setConstant(true);
				//alpha_v->Print();
				if(!vm.count("alphaGoldFix") || !region_itr->Contains("absEta_1_1.4442-gold")) {
					args.add(*alpha_v);
				}
			}
			if(reg.MatchB(*region_itr) && vm.count("constTermFix") == 1) {
				if(vm.count("alphaGoldFix") && region_itr->Contains("absEta_1_1.4442-gold")) {
					std::cout << "[STATUS] Fixing alpha term to low eta region " << *region_itr << std::endl;
					std::cerr << "[STATUS] Fixing alpha term to low eta region " << *region_itr << std::endl;
					TString lowRegionVarName = varName;
					lowRegionVarName.ReplaceAll("absEta_1_1.4442", "absEta_0_1");
					alpha_v = (RooRealVar *)args.find("alpha_" + lowRegionVarName);
					alpha_ = new RooFormulaVar("alpha_" + *region_itr, "alpha_" + lowRegionVarName, "@0", *alpha_v);
				} else {
					alpha_ = new RooFormulaVar("alpha_" + *region_itr, "alpha_" + varName, "@0", *alpha_v);
				}
				alpha_v->setConstant(false);
			} else alpha_ = alpha_v;

			args_vec.push_back(RooArgSet(*scale_, *alpha_, *const_term_));
		}

		if(vm.count("onlyScale")) {
			TIterator *it1 = NULL;
			it1 = args.createIterator();
			for(RooRealVar *var = (RooRealVar *) it1->Next(); var != NULL;
			        var = (RooRealVar *) it1->Next()) {
				TString name(var->GetName());
				if(name.Contains("scale")) continue;
				var->setConstant(true);
			}
		}

		args.sort(kFALSE);
		if(vm.count("smearerFit")) {
			std::cout << "------------------------------ smearer parameters" << std::endl;
			args.writeToStream(std::cout, kFALSE);
		}

		TRandom3 g(0);
		Long64_t randomInt = g.Integer(1000000);
		TString filename = "tmp/tmpFile-";
		filename += randomInt;
		filename += ".root";
		TFile *tmpFile = new TFile(filename, "recreate");
		tmpFile->cd();
		RooSmearer smearer("smearer", (tagChainMap["d"])["selected"].get(), (tagChainMap["s"])["selected"].get(), NULL,
		                   categories,
		                   args_vec, args, energyBranchName);
		smearer._isDataSmeared = vm.count("isDataSmeared");
		if(vm.count("runToy")) smearer.SetPuWeight(false);
		smearer.SetOnlyDiagonal(vm.count("onlyDiagonal"));
		smearer._autoBin = vm.count("autoBin");
		smearer._autoNsmear = vm.count("autoNsmear");
		smearer.smearscan = vm.count("smearscan");
		//smearer.nEventsMinDiag = nEventsMinDiag;
		smearer._deactive_minEventsOffDiag = nEventsMinOffDiag;
		smearer.SetSmearingEt(vm.count("smearingEt"));
		smearer.SetR9Weight(vm.count("useR9weight"));
		smearer.SetPdfSystWeight(pdfSystWeightIndex);
		smearer.SetZPtWeight(vm.count("useZPtweight"));
		smearer.SetFsrWeight(vm.count("useFSRweight"));
		smearer.SetWeakWeight(vm.count("useWEAKweight"));

		if(nSmearToy > 0) smearer._nSmearToy = nSmearToy;


		smearer.SetHistBinning(80, 100, invMass_binWidth); // to do before Init
		if(vm.count("runToy")) {
			smearer.SetPuWeight(false);

			smearer.SetToyScale(1, constTermToy);
			if(vm.count("initFile")) smearer.Init(commonCut.c_str(), eleID, nEventsPerToy, vm.count("runToy"), true, initFileName.c_str());
			//	  if(vm.count("initFile")) smearer.Init(commonCut.c_str(), eleID, nEventsPerToy, vm.count("runToy"), false,initFileName.c_str());
			else smearer.Init(commonCut.c_str(), eleID, nEventsPerToy, vm.count("runToy"));
			std::cout << "[DEBUG] " << constTermToy << std::endl;
		} else {
			if(vm.count("initFile")) {
				std::cout << "[INFO] Reading init file: " << initFileName << std::endl;
				args.readFromFile(initFileName.c_str());
			}
			args.writeToStream(std::cout, kFALSE);
			smearer.Init(commonCut.c_str(), eleID);
		}
		myClock.Start();
		smearer.evaluate();
		myClock.Stop();
		myClock.Print();
		smearer.DumpNLL();

		//if(vm.count("plotOnly")) //smearer.SetNSmear(10);
		RooMinuit m(smearer);
		if(! vm.count("plotOnly") && ! vm.count("profileOnly")) {
			//m.setStrategy(0);
			//m.setVerbose();

			//m.hesse(); // one turn of hess to guess the covariance matrix
			//RooFitResult *fitres = m.save();
			ShervinMinuit sh(100, smearer);//(RooArgSet&)fitres->floatParsFinal());
			sh.SetMinuit(m);
			//ph.SetVariables();
			//ph.SetCovMatrix(fitres->covarianceMatrix());
			//ph.SetUpdateProposalParameters(kTRUE); // auto-create mean vars and add mappings
			//ph.SetCacheSize(100);
			//ProposalFunction* pf = ph.GetProposalFunction();

			if(minimType == "migrad") {

				//fitres =
				m.fit("");
				//m.migrad();
				//m.hesse();
			} else if(minimType == "profile") {
				MinimizationProfile(smearer, args, nIter);
				args.writeToStream(std::cout, kFALSE);
			} else if(minimType == "MCMC") {
				MinimizationProfile(smearer, args, nIter, true);
				args.writeToStream(std::cout, kFALSE);
			} else if(minimType == "sampling") {
				RooArgList 	 argList_(args);
				TIterator 	*it_ = argList_.createIterator();
				for(RooRealVar *var = (RooRealVar*)it_->Next(); var != NULL; var = (RooRealVar*)it_->Next()) {
					if (var->isConstant() || !var->isLValue()) continue;
					TString  name(var->GetName());
					if(name.Contains("scale")) var->setConstant();
				}

				RooStats::ProposalHelper ph;
				ph.SetVariables(args);
				ph.SetCovMatrix(*(smearer._markov.GetAsDataSet()->covarianceMatrix(args)));
				ph.SetUniformFraction(0.05);
				ph.SetWidthRangeDivisor(2);

				RooStats::MetropolisHastings MCMC(smearer, args, *(ph.GetProposalFunction()), nIter);
				MCMC.SetNumBurnInSteps(10);
				MCMC.SetSign(RooStats::MetropolisHastings::kNegative);
				MCMC.SetType(RooStats::MetropolisHastings::kLog);
				//MCMC.SetProposalFunction(sh);
				RooStats::MarkovChain *mcChain = (MCMC.ConstructChain());
				mcChain->SaveAs("tmp/newChain.root");
				delete mcChain;

			}
		}

		std::cout << "[INFO] Minimization time: \t";
		myClock.Stop();
		myClock.Print();
		if(!vm.count("profileOnly") && !vm.count("plotOnly")) {
			args.writeToFile(outDirFitResData + "/params-" + r + "-" + TString(commonCut.c_str()) + ".txt");
			smearer._markov.SaveAs((outDirFitResData + "/markov-" + r + "-" + TString(commonCut.c_str()) + ".root"));
		}

		//RooDataSet *dSet = smearer.GetMarkovChainAsDataSet();
		//dSet->SaveAs("tmp/dataset.root");
		//save the smeared histograms

		for(std::vector<ZeeCategory>::iterator itr = smearer.ZeeCategories.begin();
		        itr != smearer.ZeeCategories.end();
		        itr++) {
			smearer.GetSmearedHisto(*itr, true, false);
			smearer.GetSmearedHisto(*itr, true, true);
			smearer.GetSmearedHisto(*itr, false, smearer._isDataSmeared);
		}

		if(vm.count("plotOnly") || !vm.count("profileOnly")) {
			TFile *f = new TFile(outDirFitResData + "/histos-" + r + "-" + TString(commonCut.c_str()) + ".root", "recreate");
			f->Print();
			f->cd();
			for(std::vector<ZeeCategory>::iterator itr = smearer.ZeeCategories.begin();
			        itr != smearer.ZeeCategories.end();
			        itr++) {
				//if(!itr->active) continue;
				TH1F *MC = smearer.GetSmearedHisto(*itr, true, false);
				TH1F *smearMC = smearer.GetSmearedHisto(*itr, true, true);
				TH1F *data = smearer.GetSmearedHisto(*itr, false, smearer._isDataSmeared);

				MC->Write();
				smearMC->Write();
				data->Write();
				f->Write();
			}
			f->Close();
		}

		if(vm.count("profileOnly") || !vm.count("plotOnly")) {
			//if(vm.count("profileOnly") && !vm.count("runToy")) smearer.SetNSmear(10);

			std::cout << "==================PROFILE==================" << endl;
			//if(!vm.count("constTermFix")) smearer.SetNSmear(0,20);
			//create profiles
			TString outFile = outDirFitResData.c_str();
			outFile += "/outProfile-";
			outFile += r + "-" + TString(commonCut.c_str()) + ".root";
			TFile fOutProfile(outFile, "recreate");
			//test/dato/fitres/Hgg_Et_v7/0.03//outProfile-data/regions/test.dat-Et_25-EB.root

			for (int ivar = 0; ivar < args.getSize(); ++ivar) {
				RooArgList argList(args);
				RooRealVar* var = (RooRealVar*)(argList.at(ivar));
				if (var->isConstant())
					continue;

				TString name(var->GetName());
				//if(name.Contains("scale")) continue;
				//if(name.Contains("absEta_1_1.4442-gold") && vm.count("alphaGoldFix")) continue;
				// special part for alpha fitting
				double min = 0.;
				TString  alphaName = name;
				alphaName.ReplaceAll("constTerm", "alpha");
				RooRealVar *var2 = name.Contains("constTerm") ? (RooRealVar *)argList.find(alphaName) : NULL;
				if(var2 != NULL && name.Contains("constTerm") && var2->isConstant() == false) {
					TString name2 = name;

					double rho = 0, Emean = 0;
					smearer.SetDataSet(name, TString(var->GetName()) + TString(var2->GetName()));
					if(vm.count("constTermFix")) MinProfile2D(var, var2, smearer, -1, 0., min, rho, Emean, false);
					smearer.dataset->Write();

					// rho profile
					name2.ReplaceAll("constTerm", "rho");
					smearer.SetDataSet(name2, "rho");
					Double_t v1 = var->getVal();
					Double_t v2 = var2->getVal();
					var2->setVal(0);
					var->setVal(rho);
					TGraph *profil = NULL;
					profil = GetProfile(var, smearer, 0);
					var2->setVal(v2);
					var->setVal(v1);
					TString n = "profileChi2_" + name2 + "_";
					n += randomInt;
					profil->SetName(n);
					TCanvas c("c_" + name);
					profil->Draw("AP*");
					fOutProfile.cd();
					profil->Write();
					delete profil;
					smearer.dataset->Write();


					// phi profile
					name2.ReplaceAll("rho", "phi");
					smearer.SetDataSet(name2, "phi");
					profil = GetProfile(var, var2, smearer, true, rho, Emean);
					n = "profileChi2_" + name2 + "_";
					n += randomInt;
					profil->SetName(n);
					profil->Draw("AP*");
					fOutProfile.cd();
					profil->Write();
					delete profil;
					smearer.dataset->Write();

					// rho profile with fixed phi!=pi/2
					name2.ReplaceAll("phi", "rho_phi4");
					smearer.SetDataSet(name2, "rho_phi4");
					profil = GetProfile(var, var2, smearer, true, 0, Emean, 0.785);
					n = "profileChi2_" + name2 + "_";
					n += randomInt;
					profil->SetName(n);
					profil->Draw("AP*");
					fOutProfile.cd();
					profil->Write();
					delete profil;
					smearer.dataset->Write();

// 		// rho profile with fixed phi!=pi/2
// 		name2.ReplaceAll("rho_phi4", "rho_phi6");
// 		smearer.SetDataSet(name2,"rho_phi6");
// 		profil = GetProfile(var, var2, smearer, true, 0, Emean, 0.524);
// 		n="profileChi2_"+name2+"_"; n+=randomInt;
// 		profil->SetName(n);
// 		profil->Draw("AP*");
// 		fOutProfile.cd();
// 		profil->Write();
// 		delete profil;
// 		smearer.dataset->Write();

// 		// rho profile with fixed phi!=pi/2
// 		name2.ReplaceAll("rho_phi6", "rho_phi3");
// 		smearer.SetDataSet(name2,"rho_phi3");
// 		profil = GetProfile(var, var2, smearer, true, 0, Emean, 1.05);
// 		n="profileChi2_"+name2+"_"; n+=randomInt;
// 		profil->SetName(n);
// 		profil->Draw("AP*");
// 		fOutProfile.cd();
// 		profil->Write();
// 		delete profil;
// 		smearer.dataset->Write();


				}

				//if(!name.Contains("scale")) continue;
				std::cout << "Doing " << name << "\t" << var->getVal() << std::endl;
				smearer.SetDataSet(name, name);
				//	      if(vm.count("runToy")){
				//		MinProfile(var, smearer, -1, 0., min, false);
				//}
				TGraph *profil = GetProfile(var, smearer, 0);
				TString n = "profileChi2_" + name + "_";
				n += randomInt;
				profil->SetName(n);
				TCanvas c("c_" + name);
				profil->Draw("AP*");
				fOutProfile.cd();
				profil->Write();
				std::cout << "Saved profile for " << name << std::endl;
				smearer.dataset->Write();
				delete profil;
			}
			std::cout << "Cloning args" << std::endl;
			//	  RooArgSet *mu = (RooArgSet *)args.snapshot();
			//std::cout << "creating roomultivargaussian" << std::endl;
			//RooMultiVarGaussian g("multi","",args, *mu, *(smearer.GetMarkovChainAsDataSet()->covarianceMatrix()));
			//std::cout << "created" << std::endl;

			// 	  RooDataSet *dataset = g.generate(args,10);
			// 	  for(Long64_t iGen=0; iGen<1; iGen++){
			// 	    RooArgList argList_(* dataset->get(iGen));
			// 	    TIterator *it_ = argList_.createIterator();
			// 	    for(RooRealVar *var = (RooRealVar*)it_->Next(); var != NULL; var = (RooRealVar*)it_->Next()){
			// 	      if (!var->isLValue()) continue;
			// 	      TString  name(var->GetName());
			// 	      //std::cout << name << var->getVal() << std::endl;
			// 	      var->Print();
			// 	      //args.setRealValue(name,var->getVal());
// 	    }
// 	    //smearer.evaluate();
// 	  }
			fOutProfile.Close();

		}
		tmpFile->Close();

	}
	globalClock.Stop();
	std::cout << "[INFO] Total elapsed time: ";
	globalClock.Print();
	return 0;
}


//  LocalWords:  etaSCEle
